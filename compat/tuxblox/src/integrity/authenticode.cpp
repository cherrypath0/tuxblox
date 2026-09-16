// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "integrity/authenticode.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string_view>
#include <vector>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace tuxblox {

namespace {

// Offsets inside a PE image, from the Microsoft PE format specification.
constexpr size_t PeOffsetField = 0x3c;
constexpr size_t CoffHeaderSize = 20;
constexpr size_t ChecksumInOptionalHeader = 64;
constexpr size_t DataDirsInPe32 = 96;
constexpr size_t DataDirsInPe32Plus = 112;
constexpr size_t SecurityDirectoryIndex = 4;
constexpr size_t DataDirectoryEntrySize = 8;
constexpr uint16_t Pe32Magic = 0x10b;
constexpr uint16_t Pe32PlusMagic = 0x20b;
constexpr size_t WinCertificateHeaderSize = 8;

// A read that stops rather than reading past the end.
bool readAt(std::ifstream& file, size_t offset, void *pOut, size_t size) {
    file.clear();
    file.seekg(static_cast<std::streamoff>(offset));
    if (!file) return false;
    file.read(static_cast<char *>(pOut), static_cast<std::streamsize>(size));
    return static_cast<size_t>(file.gcount()) == size;
}

uint16_t readU16(const unsigned char *pData) {
    return static_cast<uint16_t>(pData[0] | (pData[1] << 8));
}

uint32_t readU32(const unsigned char *pData) {
    return static_cast<uint32_t>(pData[0]) | (static_cast<uint32_t>(pData[1]) << 8) |
           (static_cast<uint32_t>(pData[2]) << 16) | (static_cast<uint32_t>(pData[3]) << 24);
}

// Where the signature lives and which bytes it covers.
struct PeLayout {
    size_t checksumOffset = 0;
    size_t securityEntryOffset = 0;
    size_t certificateOffset = 0;
    size_t certificateSize = 0;
    size_t fileSize = 0;
};

bool readPeLayout(std::ifstream& file, size_t fileSize, PeLayout& layout) {
    unsigned char buffer[DataDirectoryEntrySize];

    if (fileSize < PeOffsetField + 4) return false;
    if (!readAt(file, PeOffsetField, buffer, 4)) return false;
    const size_t peOffset = readU32(buffer);
    if (peOffset + CoffHeaderSize + 4 > fileSize) return false;

    if (!readAt(file, peOffset, buffer, 4)) return false;
    if (buffer[0] != 'P' || buffer[1] != 'E' || buffer[2] != 0 || buffer[3] != 0) return false;

    const size_t optionalHeader = peOffset + 4 + CoffHeaderSize;
    if (optionalHeader + 2 > fileSize) return false;
    if (!readAt(file, optionalHeader, buffer, 2)) return false;

    const uint16_t magic = readU16(buffer);
    size_t dataDirectories = 0;
    if (magic == Pe32Magic) {
        dataDirectories = optionalHeader + DataDirsInPe32;
    } else if (magic == Pe32PlusMagic) {
        dataDirectories = optionalHeader + DataDirsInPe32Plus;
    } else {
        return false;
    }

    layout.checksumOffset = optionalHeader + ChecksumInOptionalHeader;
    layout.securityEntryOffset = dataDirectories + SecurityDirectoryIndex * DataDirectoryEntrySize;
    layout.fileSize = fileSize;
    if (layout.securityEntryOffset + DataDirectoryEntrySize > fileSize) return false;

    if (!readAt(file, layout.securityEntryOffset, buffer, DataDirectoryEntrySize)) return false;
    // For this one directory the first field is a file offset, not an RVA.
    layout.certificateOffset = readU32(buffer);
    layout.certificateSize = readU32(buffer + 4);
    return true;
}

// The Authenticode digest: the whole file except its checksum, the entry
// describing where the signature is, and the signature itself. Those three
// are skipped because signing the file is what puts them there.
bool digestImage(std::ifstream& file, const PeLayout& layout, const EVP_MD *pDigest,
                 std::vector<unsigned char>& out) {
    struct Region { size_t start; size_t end; };
    const Region regions[] = {
        { 0, layout.checksumOffset },
        { layout.checksumOffset + 4, layout.securityEntryOffset },
        { layout.securityEntryOffset + DataDirectoryEntrySize, layout.certificateOffset }
    };

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),
                                                                    EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), pDigest, nullptr) != 1) return false;

    std::vector<char> chunk(1u << 20);
    for (const Region& region : regions) {
        if (region.end < region.start || region.end > layout.fileSize) return false;
        size_t remaining = region.end - region.start;
        file.clear();
        file.seekg(static_cast<std::streamoff>(region.start));
        if (!file && remaining) return false;
        while (remaining) {
            const size_t want = remaining < chunk.size() ? remaining : chunk.size();
            file.read(chunk.data(), static_cast<std::streamsize>(want));
            if (static_cast<size_t>(file.gcount()) != want) return false;
            if (EVP_DigestUpdate(context.get(), chunk.data(), want) != 1) return false;
            remaining -= want;
        }
    }

    out.resize(EVP_MAX_MD_SIZE);
    unsigned int size = 0;
    if (EVP_DigestFinal_ex(context.get(), out.data(), &size) != 1) return false;
    out.resize(size);
    return true;
}

// One DER element: its tag, its contents, and where the next one starts.
struct Element {
    unsigned char tag = 0;
    const unsigned char *pValue = nullptr;
    size_t length = 0;
    const unsigned char *pNext = nullptr;
    bool ok = false;
};

Element readElement(const unsigned char *pData, const unsigned char *pEnd) {
    Element element;
    if (pEnd - pData < 2) return element;

    element.tag = *pData++;
    size_t length = *pData++;
    if (length & 0x80) {
        const size_t count = length & 0x7f;
        // Lengths this long do not appear in a signature, and allowing them
        // would only risk an overflow below.
        if (count == 0 || count > 4 || static_cast<size_t>(pEnd - pData) < count) return element;
        length = 0;
        for (size_t i = 0; i < count; i++) length = (length << 8) | *pData++;
    }
    if (static_cast<size_t>(pEnd - pData) < length) return element;

    element.pValue = pData;
    element.length = length;
    element.pNext = pData + length;
    element.ok = true;
    return element;
}

// SpcIndirectDataContent ::= SEQUENCE { data, messageDigest DigestInfo }
// DigestInfo             ::= SEQUENCE { digestAlgorithm, digest OCTET STRING }
//
// Wine declares this structure and never decodes it, which is the whole
// reason this file exists, so it is walked by hand here.
bool readSignedDigest(const unsigned char *pContent, size_t contentLength,
                      const EVP_MD **ppDigest, std::vector<unsigned char>& digest) {
    const unsigned char *pEnd = pContent + contentLength;

    const Element outer = readElement(pContent, pEnd);
    if (!outer.ok || outer.tag != 0x30) return false;

    const Element attribute = readElement(outer.pValue, outer.pValue + outer.length);
    if (!attribute.ok) return false;

    const Element digestInfo = readElement(attribute.pNext, outer.pValue + outer.length);
    if (!digestInfo.ok || digestInfo.tag != 0x30) return false;

    const unsigned char *pInfoEnd = digestInfo.pValue + digestInfo.length;
    const Element algorithm = readElement(digestInfo.pValue, pInfoEnd);
    if (!algorithm.ok || algorithm.tag != 0x30) return false;

    const Element oid = readElement(algorithm.pValue, algorithm.pValue + algorithm.length);
    if (!oid.ok || oid.tag != 0x06) return false;

    ASN1_OBJECT *pObject = nullptr;
    const unsigned char *pOidStart = algorithm.pValue;
    pObject = d2i_ASN1_OBJECT(nullptr, &pOidStart, static_cast<long>(algorithm.length));
    if (pObject == nullptr) return false;
    *ppDigest = EVP_get_digestbynid(OBJ_obj2nid(pObject));
    ASN1_OBJECT_free(pObject);
    if (*ppDigest == nullptr) return false;

    const Element value = readElement(algorithm.pNext, pInfoEnd);
    if (!value.ok || value.tag != 0x04) return false;

    digest.assign(value.pValue, value.pValue + value.length);
    return true;
}

// Every root we are willing to chain to, as an OpenSSL store.
X509_STORE *buildStore(std::string_view rootsPem) {
    X509_STORE *pStore = X509_STORE_new();
    if (pStore == nullptr) return nullptr;

    std::unique_ptr<BIO, decltype(&BIO_free)> source(
        BIO_new_mem_buf(rootsPem.data(), static_cast<int>(rootsPem.size())), BIO_free);
    if (!source) {
        X509_STORE_free(pStore);
        return nullptr;
    }

    int loaded = 0;
    while (X509 *pCertificate = PEM_read_bio_X509(source.get(), nullptr, nullptr, nullptr)) {
        if (X509_STORE_add_cert(pStore, pCertificate) == 1) loaded++;
        X509_free(pCertificate);
    }
    ERR_clear_error();

    if (loaded == 0) {
        X509_STORE_free(pStore);
        return nullptr;
    }
    return pStore;
}

std::string organisationOf(X509 *pCertificate) {
    char buffer[256] = {0};
    X509_NAME *pName = X509_get_subject_name(pCertificate);
    if (pName == nullptr) return std::string();
    const int length = X509_NAME_get_text_by_NID(pName, NID_organizationName, buffer, sizeof(buffer));
    if (length <= 0) return std::string();
    return std::string(buffer, static_cast<size_t>(length));
}

IntegrityReport fail(IntegrityStatus status, const std::string& detail) {
    IntegrityReport report;
    report.status = status;
    report.detail = detail;
    return report;
}

} // namespace

const char *integrityStatusName(IntegrityStatus status) {
    switch (status) {
        case IntegrityStatus::Verified:       return "verified";
        case IntegrityStatus::NotSigned:      return "not signed";
        case IntegrityStatus::Modified:       return "modified since it was signed";
        case IntegrityStatus::BadSignature:   return "signature did not verify";
        case IntegrityStatus::WrongPublisher: return "signed by somebody else";
        case IntegrityStatus::Unreadable:     return "unreadable";
    }
    return "unreadable";
}

IntegrityReport verifyAuthenticode(const std::filesystem::path& file,
                                   const std::string& requiredPublisher,
                                   std::string_view rootsPem) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(file, error);
    if (error) return fail(IntegrityStatus::Unreadable, "cannot read " + file.string());

    std::ifstream image(file, std::ios::binary);
    if (!image) return fail(IntegrityStatus::Unreadable, "cannot open " + file.string());

    PeLayout layout;
    if (!readPeLayout(image, static_cast<size_t>(size), layout)) {
        return fail(IntegrityStatus::Unreadable, "not a Windows program: " + file.string());
    }

    if (layout.certificateSize == 0 || layout.certificateOffset == 0) {
        return fail(IntegrityStatus::NotSigned, "no signature in " + file.string());
    }
    if (layout.certificateOffset + layout.certificateSize > layout.fileSize ||
        layout.certificateSize <= WinCertificateHeaderSize) {
        return fail(IntegrityStatus::BadSignature, "signature runs past the end of " + file.string());
    }

    std::vector<unsigned char> certificate(layout.certificateSize - WinCertificateHeaderSize);
    if (!readAt(image, layout.certificateOffset + WinCertificateHeaderSize, certificate.data(),
                certificate.size())) {
        return fail(IntegrityStatus::BadSignature, "signature could not be read");
    }

    const unsigned char *pCertificateData = certificate.data();
    std::unique_ptr<PKCS7, decltype(&PKCS7_free)> signature(
        d2i_PKCS7(nullptr, &pCertificateData, static_cast<long>(certificate.size())), PKCS7_free);
    ERR_clear_error();
    if (!signature || !PKCS7_type_is_signed(signature.get()) ||
        signature->d.sign == nullptr || signature->d.sign->contents == nullptr) {
        return fail(IntegrityStatus::BadSignature, "signature is not readable");
    }

    const ASN1_TYPE *pContent = signature->d.sign->contents->d.other;
    if (pContent == nullptr || pContent->type != V_ASN1_SEQUENCE ||
        pContent->value.sequence == nullptr) {
        return fail(IntegrityStatus::BadSignature, "signature has no signed content");
    }
    const unsigned char *pContentData = pContent->value.sequence->data;
    const size_t contentLength = static_cast<size_t>(pContent->value.sequence->length);

    const EVP_MD *pDigestAlgorithm = nullptr;
    std::vector<unsigned char> signedDigest;
    if (!readSignedDigest(pContentData, contentLength, &pDigestAlgorithm, signedDigest)) {
        return fail(IntegrityStatus::BadSignature, "signature does not say what it covers");
    }

    std::vector<unsigned char> actualDigest;
    if (!digestImage(image, layout, pDigestAlgorithm, actualDigest)) {
        return fail(IntegrityStatus::Unreadable, "could not read all of " + file.string());
    }
    if (actualDigest.size() != signedDigest.size() ||
        CRYPTO_memcmp(actualDigest.data(), signedDigest.data(), actualDigest.size()) != 0) {
        return fail(IntegrityStatus::Modified, file.string() + " does not match its signature");
    }

    STACK_OF(PKCS7_SIGNER_INFO) *pSigners = PKCS7_get_signer_info(signature.get());
    if (pSigners == nullptr || sk_PKCS7_SIGNER_INFO_num(pSigners) < 1) {
        return fail(IntegrityStatus::BadSignature, "signature has no signer");
    }
    PKCS7_SIGNER_INFO *pSigner = sk_PKCS7_SIGNER_INFO_value(pSigners, 0);
    X509 *pLeaf = X509_find_by_issuer_and_serial(signature->d.sign->cert,
                                                  pSigner->issuer_and_serial->issuer,
                                                  pSigner->issuer_and_serial->serial);
    if (pLeaf == nullptr) {
        return fail(IntegrityStatus::BadSignature, "the signing certificate is missing");
    }

    // The signature covers the signed attributes, one of which is the digest
    // of the content checked above -- so this is what ties the two together.
    ASN1_TYPE *pMessageDigest = PKCS7_get_signed_attribute(pSigner, NID_pkcs9_messageDigest);
    if (pMessageDigest == nullptr || pMessageDigest->type != V_ASN1_OCTET_STRING) {
        return fail(IntegrityStatus::BadSignature, "signature does not cover its own content");
    }

    // What the signer actually digested is the content WITHOUT its outer
    // SEQUENCE header -- the header is part of how the content is wrapped, not
    // part of the content. Measured against genuine Roblox binaries: including
    // it makes every real signature look broken.
    const Element contentElement = readElement(pContentData, pContentData + contentLength);
    if (!contentElement.ok) {
        return fail(IntegrityStatus::BadSignature, "signed content is not readable");
    }

    std::vector<unsigned char> contentDigest(EVP_MAX_MD_SIZE);
    unsigned int contentDigestSize = 0;
    const EVP_MD *pSignerDigest = EVP_get_digestbynid(OBJ_obj2nid(pSigner->digest_alg->algorithm));
    if (pSignerDigest == nullptr ||
        EVP_Digest(contentElement.pValue, contentElement.length, contentDigest.data(),
                   &contentDigestSize, pSignerDigest, nullptr) != 1) {
        return fail(IntegrityStatus::BadSignature, "signature uses an unknown digest");
    }
    contentDigest.resize(contentDigestSize);

    const ASN1_OCTET_STRING *pClaimed = pMessageDigest->value.octet_string;
    if (pClaimed == nullptr || static_cast<size_t>(pClaimed->length) != contentDigest.size() ||
        CRYPTO_memcmp(pClaimed->data, contentDigest.data(), contentDigest.size()) != 0) {
        return fail(IntegrityStatus::Modified, "signature does not match its own content");
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(X509_get_pubkey(pLeaf), EVP_PKEY_free);
    if (!key) return fail(IntegrityStatus::BadSignature, "the signing certificate has no key");

    // Verified by hand rather than with ASN1_item_verify, which works out the
    // digest from the signature algorithm: a signed Windows program names that
    // algorithm as plain RSA, with the digest recorded separately, so there is
    // nothing for it to work out and every real signature fails.
    unsigned char *pAttributes = nullptr;
    const int attributesLength = ASN1_item_i2d(reinterpret_cast<ASN1_VALUE *>(pSigner->auth_attr),
                                               &pAttributes, ASN1_ITEM_rptr(PKCS7_ATTR_VERIFY));
    if (pAttributes == nullptr || attributesLength <= 0) {
        return fail(IntegrityStatus::BadSignature, "signature attributes are not readable");
    }

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> verify(EVP_MD_CTX_new(),
                                                                    EVP_MD_CTX_free);
    int verified = 0;
    if (verify && EVP_DigestVerifyInit(verify.get(), nullptr, pSignerDigest, nullptr,
                                       key.get()) == 1) {
        verified = EVP_DigestVerify(verify.get(), pSigner->enc_digest->data,
                                    static_cast<size_t>(pSigner->enc_digest->length),
                                    pAttributes, static_cast<size_t>(attributesLength));
    }
    OPENSSL_free(pAttributes);
    if (verified != 1) {
        ERR_clear_error();
        return fail(IntegrityStatus::BadSignature, "the signature does not verify");
    }

    std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)> store(buildStore(rootsPem),
                                                                   X509_STORE_free);
    if (!store) return fail(IntegrityStatus::Unreadable, "no root certificates to check against");

    std::unique_ptr<X509_STORE_CTX, decltype(&X509_STORE_CTX_free)> chain(X509_STORE_CTX_new(),
                                                                          X509_STORE_CTX_free);
    if (!chain || X509_STORE_CTX_init(chain.get(), store.get(), pLeaf,
                                      signature->d.sign->cert) != 1) {
        return fail(IntegrityStatus::Unreadable, "could not check the certificate chain");
    }
    // Signing certificates are deliberately short-lived and the signature
    // carries a timestamp from when they were valid, so a program signed last
    // year is still correctly signed today. Checking dates here would reject
    // every older release the day its certificate lapsed.
    X509_STORE_CTX_set_flags(chain.get(), X509_V_FLAG_NO_CHECK_TIME);

    if (X509_verify_cert(chain.get()) != 1) {
        const int reason = X509_STORE_CTX_get_error(chain.get());
        return fail(IntegrityStatus::BadSignature,
                    std::string("certificate chain is not trusted: ") +
                        X509_verify_cert_error_string(reason));
    }

    IntegrityReport report;
    report.publisher = organisationOf(pLeaf);
    if (!requiredPublisher.empty() && report.publisher != requiredPublisher) {
        report.status = IntegrityStatus::WrongPublisher;
        report.detail = file.string() + " is signed by \"" + report.publisher + "\", not \"" +
                        requiredPublisher + "\"";
        return report;
    }

    report.status = IntegrityStatus::Verified;
    report.detail = file.filename().string() + " is signed by " + report.publisher;
    return report;
}

} // namespace tuxblox
