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

#include "mac_signature.h"

#include "apple_root_ca.h"

#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace tuxblox::macclient {

namespace {

std::vector<std::string> codeHashesOf(const std::string& path);

// The signature format is documented by Apple's code-signing technotes and
// the public mach-o/loader.h constants. It is not an API: a future signing
// format may need this reader updated. Anything this reader does not
// understand refuses the bundle rather than guessing.
constexpr uint32_t kFatMagic = 0xcafebabe;         // big-endian fat header
constexpr uint32_t kThin64MagicBytes = 0xfeedfacf; // read little-endian from the file
constexpr uint32_t kX86_64CpuType = 0x01000007;
constexpr uint32_t kLcCodeSignature = 0x1d;

constexpr uint32_t kBlobSuperBlob = 0xfade0cc0;
constexpr uint32_t kBlobCodeDirectory = 0xfade0c02;
constexpr uint32_t kBlobSignature = 0xfade0b01;
constexpr uint32_t kSlotCodeDirectory = 0;
// The CMS blob lives in the dedicated signature slot, not in slot 1 (which
// is the requirement set).
constexpr uint32_t kSlotSignature = 0x10000;
constexpr uint32_t kSlotEntitlements = 5;
constexpr uint32_t kSlotEntitlementsDer = 7;

constexpr uint32_t kCdVersionTeam = 0x20200;
constexpr uint32_t kCdVersionScatter = 0x20100;
constexpr uint8_t kHashSha256 = 2;

constexpr const char* kTeam = "2CFABCH843";
constexpr const char* kOidIntermediate = "1.2.840.113635.100.6.2.6";
constexpr const char* kOidLeaf = "1.2.840.113635.100.6.1.13";

struct Failure {
    std::string reason;
};

[[noreturn]] void refuse(const char* what) { throw Failure{what}; }

std::vector<uint8_t> readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) refuse("cannot open a sealed file");
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                                std::istreambuf_iterator<char>());
}

uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t len) {
    std::array<uint8_t, 32> out{};
    unsigned int n = 0;
    if (EVP_Digest(data, len, out.data(), &n, EVP_sha256(), nullptr) != 1 || n != out.size())
        refuse("sha256 failed");
    return out;
}

bool equalDigest(const uint8_t* a, const std::array<uint8_t, 32>& b, size_t n) {
    if (n == 0 || n > b.size()) return false;
    return std::equal(a, a + n, b.begin());
}

std::string base64Decode(const std::string& text) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    int bits = 0;
    int count = 0;
    for (char c : text) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        const int v = value(c);
        if (v < 0) refuse("bad base64 in CodeResources");
        bits = (bits << 6) | v;
        count += 6;
        if (count >= 8) {
            count -= 8;
            out.push_back(static_cast<char>((bits >> count) & 0xff));
        }
    }
    return out;
}

struct Slice {
    uint32_t offset = 0;
    uint32_t size = 0;
};

Slice x86Slice(const std::vector<uint8_t>& file) {
    if (file.size() < 8) refuse("Mach-O file is too short");
    // A fat header is stored big-endian; a thin x86-64 header is little-endian.
    if (file[0] == 0xca && file[1] == 0xfe && file[2] == 0xba && file[3] == 0xbe) {
        const uint32_t count = be32(file.data() + 4);
        if (count == 0 || count > 64 || file.size() < 8 + size_t(count) * 20)
            refuse("bad Mach-O fat header");
        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* arch = file.data() + 8 + size_t(i) * 20;
            if (be32(arch) != kX86_64CpuType) continue;
            Slice s{be32(arch + 8), be32(arch + 12)};
            if (uint64_t(s.offset) + s.size > file.size()) refuse("bad Mach-O slice");
            return s;
        }
        refuse("no x86-64 slice");
    }
    if (file[0] == 0xcf && file[1] == 0xfa && file[2] == 0xed && file[3] == 0xfe) {
        if (le32(file.data() + 4) != kX86_64CpuType) refuse("not an x86-64 Mach-O");
        return Slice{0, uint32_t(file.size())};
    }
    refuse("unrecognised Mach-O header");
}

struct CodeDirectory {
    const uint8_t* bytes = nullptr;
    size_t length = 0;
    uint32_t version = 0;
    uint32_t digestOffset = 0;
    uint32_t identOffset = 0;
    uint32_t nSpecial = 0;
    uint32_t nCode = 0;
    uint32_t codeLimit = 0;
    size_t digestSize = 0;
    uint8_t hashType = 0;
    uint32_t pageSize = 0;
    uint32_t teamOffset = 0;
    std::string identifier;
    std::string team;
};

std::string cStringAt(const uint8_t* base, size_t length, uint32_t offset) {
    if (offset >= length) refuse("string offset outside code directory");
    const uint8_t* start = base + offset;
    const void* end = std::memchr(start, 0, length - offset);
    if (end == nullptr) refuse("unterminated string in code directory");
    return std::string(reinterpret_cast<const char*>(start),
                       static_cast<const uint8_t*>(end) - start);
}

CodeDirectory parseCodeDirectory(const uint8_t* cd, size_t available) {
    if (available < 52) refuse("code directory is too short");
    if (be32(cd) != kBlobCodeDirectory) refuse("not a code directory");
    const size_t cdLength = be32(cd + 4);
    if (cdLength < 52 || cdLength > available) refuse("bad code directory length");
    CodeDirectory out;
    out.bytes = cd;
    out.length = cdLength;
    out.version = be32(cd + 8);
    if (out.version < kCdVersionTeam) refuse("unsupported code directory version");
    out.digestOffset = be32(cd + 16);
    out.identOffset = be32(cd + 20);
    out.nSpecial = be32(cd + 24);
    out.nCode = be32(cd + 28);
    out.codeLimit = be32(cd + 32);
    out.digestSize = cd[36];
    out.hashType = cd[37];
    const uint8_t pageSizeShift = cd[39];
    if (pageSizeShift < 12 || pageSizeShift > 16) refuse("unsupported page size");
    out.pageSize = uint32_t(1) << pageSizeShift;
    if (cd[36] != 32 || out.hashType != kHashSha256) refuse("not a SHA-256 code directory");
    if (cd[40] != 0 || cd[41] != 0 || cd[42] != 0 || cd[43] != 0)
        refuse("unexpected code directory spare field");
    if (out.version >= kCdVersionScatter && be32(cd + 44) != 0)
        refuse("scattered code directories are not supported");
    out.teamOffset = be32(cd + 48);
    if (cd[38] != 0) refuse("unexpected platform binary");
    out.identifier = cStringAt(cd, cdLength, out.identOffset);
    out.team = out.teamOffset ? cStringAt(cd, cdLength, out.teamOffset) : std::string();
    // The special hashes sit immediately BEFORE digestOffset; the code page
    // hashes start at digestOffset. They are two disjoint ranges, not one
    // combined range starting at digestOffset.
    if (uint64_t(out.nSpecial) * out.digestSize + 52 > out.digestOffset)
        refuse("special slots overlap the code directory header");
    if (uint64_t(out.digestOffset) + uint64_t(out.nCode) * out.digestSize > cdLength)
        refuse("code directory hashes truncated");
    if (out.nCode != (uint64_t(out.codeLimit) + out.pageSize - 1) / out.pageSize)
        refuse("code slot count does not match the code limit");
    return out;
}

struct Signature {
    const uint8_t* data = nullptr;
    size_t length = 0;
    std::map<uint32_t, std::pair<const uint8_t*, size_t>> slots;
};

Signature parseSuperBlob(const uint8_t* sig, size_t available) {
    if (available < 12) refuse("signature blob is too short");
    if (be32(sig) != kBlobSuperBlob) refuse("not a code signature superblob");
    const size_t total = be32(sig + 4);
    if (total < 12 || total > available) refuse("bad signature superblob length");
    const uint32_t count = be32(sig + 8);
    if (count == 0 || count > 32 || size_t(12) + size_t(count) * 8 > total)
        refuse("bad signature slot count");
    Signature out;
    out.data = sig;
    out.length = total;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* entry = sig + 12 + size_t(i) * 8;
        const uint32_t type = be32(entry);
        const uint32_t offset = be32(entry + 4);
        if (offset >= total) refuse("signature slot outside blob");
        const uint32_t blobLength = be32(sig + offset + 4);
        if (blobLength > total - offset) refuse("signature slot length outside blob");
        out.slots.emplace(type, std::make_pair(sig + offset, size_t(blobLength)));
    }
    if (!out.slots.count(kSlotCodeDirectory) || !out.slots.count(kSlotSignature))
        refuse("signature has no code directory or CMS blob");
    return out;
}

bool hasOid(const X509* cert, const char* oid) {
    ASN1_OBJECT* obj = OBJ_txt2obj(oid, 1);
    if (obj == nullptr) return false;
    const bool present = X509_get_ext_by_OBJ(const_cast<X509*>(cert), obj, -1) >= 0;
    ASN1_OBJECT_free(obj);
    return present;
}

void verifyCms(const Signature& sig, const CodeDirectory& cd, const std::string& identifier) {
    const auto& cmsBlob = sig.slots.at(kSlotSignature);
    if (be32(cmsBlob.first) != kBlobSignature || cmsBlob.second < 8)
        refuse("bad CMS blob");
    const unsigned char* p = cmsBlob.first + 8;
    const long n = static_cast<long>(cmsBlob.second - 8);
    CMS_ContentInfo* cms = d2i_CMS_ContentInfo(nullptr, &p, n);
    if (cms == nullptr) refuse("cannot parse CMS signature");

    X509* root = nullptr;
    const unsigned char* rp = kAppleRootCaDer;
    root = d2i_X509(nullptr, &rp, static_cast<long>(kAppleRootCaDerLen));
    if (root == nullptr) {
        CMS_ContentInfo_free(cms);
        refuse("cannot parse the pinned Apple root");
    }
    X509_STORE* store = X509_STORE_new();
    X509_STORE_set_flags(store, X509_V_FLAG_NO_CHECK_TIME | X509_V_FLAG_IGNORE_CRITICAL);
    // Apple's Developer ID certificates carry extensions marked critical that
    // OpenSSL does not implement, and their purpose is not S/MIME. Both are
    // properties of the certificate, not a reason to accept another signer:
    // the chain still has to end at the one root in this store.
    X509_STORE_set_purpose(store, X509_PURPOSE_ANY);
    X509_STORE_add_cert(store, root);

    // The content is detached: the signed bytes are the CodeDirectory blob.
    BIO* content = BIO_new_mem_buf(const_cast<uint8_t*>(cd.bytes), static_cast<int>(cd.length));
    if (content == nullptr) {
        X509_STORE_free(store);
        X509_free(root);
        CMS_ContentInfo_free(cms);
        refuse("cannot build the CMS content buffer");
    }
    const int ok = CMS_verify(cms, nullptr, store, content, nullptr, CMS_BINARY);
    BIO_free(content);
    STACK_OF(X509)* signers = CMS_get0_signers(cms);
    if (ok != 1 || signers == nullptr || sk_X509_num(signers) != 1) {
        if (signers != nullptr) sk_X509_free(signers);
        X509_STORE_free(store);
        X509_free(root);
        CMS_ContentInfo_free(cms);
        refuse("CMS signature did not verify against the pinned root");
    }
    X509* leaf = sk_X509_value(signers, 0);
    char ou[256] = {};
    if (X509_NAME_get_text_by_NID(X509_get_subject_name(leaf), NID_organizationalUnitName,
                                  ou, sizeof(ou)) <= 0 ||
        std::string(ou) != kTeam) {
        sk_X509_free(signers);
        X509_STORE_free(store);
        X509_free(root);
        CMS_ContentInfo_free(cms);
        refuse("wrong signing team");
    }
    const bool leafOid = hasOid(leaf, kOidLeaf);
    bool intermediateOid = false;
    STACK_OF(X509)* certs = CMS_get1_certs(cms);
    if (certs != nullptr) {
        for (int i = 0; i < sk_X509_num(certs); ++i)
            if (hasOid(sk_X509_value(certs, i), kOidIntermediate)) intermediateOid = true;
        sk_X509_pop_free(certs, X509_free);
    }
    sk_X509_free(signers);
    X509_STORE_free(store);
    X509_free(root);
    CMS_ContentInfo_free(cms);
    if (!leafOid || !intermediateOid) refuse("not a Developer ID signature");
    if (cd.identifier != identifier) refuse("unexpected code identifier");
    if (cd.team != kTeam) refuse("unexpected code directory team");
}

struct ParsedCode {
    std::vector<uint8_t> file;
    Slice slice;
    CodeDirectory cd;
    Signature sig;
};

ParsedCode parseCode(const fs::path& path) {
    ParsedCode out;
    out.file = readFile(path);
    out.slice = x86Slice(out.file);
    const uint8_t* header = out.file.data() + out.slice.offset;
    if (out.slice.size < 32 || le32(header) != kThin64MagicBytes) refuse("not a 64-bit Mach-O");
    const uint32_t ncmds = le32(header + 16);
    const uint32_t sizeofcmds = le32(header + 20);
    if (uint64_t(32) + sizeofcmds > out.slice.size || ncmds > 4096) refuse("bad load commands");
    uint32_t dataoff = 0, datasize = 0;
    const uint8_t* cmd = header + 32;
    for (uint32_t i = 0; i < ncmds; ++i) {
        if (uint64_t(cmd - header) + 8 > out.slice.size) refuse("load command truncated");
        const uint32_t kind = le32(cmd);
        const uint32_t length = le32(cmd + 4);
        if (length < 8 || uint64_t(cmd - header) + length > 32 + uint64_t(sizeofcmds))
            refuse("bad load command length");
        if (kind == kLcCodeSignature) {
            if (length < 16) refuse("bad LC_CODE_SIGNATURE");
            dataoff = le32(cmd + 8);
            datasize = le32(cmd + 12);
        }
        cmd += length;
    }
    if (dataoff == 0 || datasize == 0 || uint64_t(dataoff) + datasize > out.slice.size)
        refuse("no embedded code signature");
    out.sig = parseSuperBlob(header + dataoff, datasize);
    const auto& cdBlob = out.sig.slots.at(kSlotCodeDirectory);
    out.cd = parseCodeDirectory(cdBlob.first, cdBlob.second);
    return out;
}

void verifyPages(const ParsedCode& code) {
    const uint8_t* base = code.file.data() + code.slice.offset;
    const uint8_t* hashes = code.cd.bytes + code.cd.digestOffset;
    const auto pageSize = code.cd.pageSize;
    for (uint32_t i = 0; i < code.cd.nCode; ++i) {
        const uint64_t start = uint64_t(i) * pageSize;
        if (start >= code.cd.codeLimit) refuse("code slot past the code limit");
        const uint64_t end = std::min<uint64_t>(start + pageSize, code.cd.codeLimit);
        if (end > code.slice.size) refuse("code page past the end of the slice");
        const auto digest = sha256(base + start, size_t(end - start));
        if (!equalDigest(hashes + size_t(i) * code.cd.digestSize, digest, code.cd.digestSize))
            refuse("code page hash mismatch");
    }
    // Check the special slots we understand. A non-zero hash for a slot whose
    // contents we cannot check is a refusal, not something to skip.
    const uint8_t* special = code.cd.bytes + code.cd.digestOffset -
                             code.cd.nSpecial * code.cd.digestSize;
    for (uint32_t i = 0; i < code.cd.nSpecial; ++i) {
        const int slot = -static_cast<int>(code.cd.nSpecial - i);
        const uint8_t* hash = special + size_t(i) * code.cd.digestSize;
        const bool zero = std::all_of(hash, hash + code.cd.digestSize,
                                      [](uint8_t b) { return b == 0; });
        if (slot == -5 || slot == -7) {
            const auto it = code.sig.slots.find(slot == -5 ? kSlotEntitlements
                                                           : kSlotEntitlementsDer);
            if (it == code.sig.slots.end()) {
                if (!zero) refuse("entitlement slot is missing");
                continue;
            }
            const auto digest = sha256(it->second.first, it->second.second);
            if (!equalDigest(hash, digest, code.cd.digestSize))
                refuse("entitlement hash mismatch");
        }
        // Info.plist (-1) and CodeResources (-3) are checked by the caller,
        // which knows the bundle paths.
    }
}

std::string findBetween(const std::string& text, size_t from, const std::string& open,
                        const std::string& close) {
    const size_t a = text.find(open, from);
    if (a == std::string::npos) refuse("bad CodeResources plist");
    const size_t body = a + open.size();
    const size_t b = text.find(close, body);
    if (b == std::string::npos) refuse("unterminated CodeResources plist dict");
    return text.substr(body, b - body);
}

// findBetween stops at the first closing tag, which is wrong for a plist
// dictionary: an entry can contain nested dictionaries. Walk the open and
// close tags in order instead.
std::string extractDict(const std::string& text, size_t from) {
    const size_t startTag = text.find("<dict>", from);
    if (startTag == std::string::npos) refuse("CodeResources has no dictionary");
    const size_t body = startTag + 6;
    size_t pos = body;
    int depth = 1;
    while (depth > 0) {
        const size_t open = text.find("<dict>", pos);
        const size_t close = text.find("</dict>", pos);
        if (close == std::string::npos) refuse("unterminated CodeResources plist dict");
        if (open != std::string::npos && open < close) {
            ++depth;
            pos = open + 6;
        } else {
            --depth;
            if (depth == 0) return text.substr(body, close - body);
            pos = close + 7;
        }
    }
    refuse("unterminated CodeResources plist dict");
}

std::string valueAfterKey(const std::string& dict, const std::string& key) {
    const std::string needle = "<key>" + key + "</key>";
    const size_t at = dict.find(needle);
    if (at == std::string::npos) return std::string();
    const size_t start = dict.find_first_not_of(" \t\r\n", at + needle.size());
    if (start == std::string::npos) return std::string();
    if (dict.compare(start, 8, "<string>") == 0)
        return findBetween(dict, start, "<string>", "</string>");
    if (dict.compare(start, 6, "<data>") == 0) {
        std::string raw = findBetween(dict, start, "<data>", "</data>");
        raw.erase(std::remove_if(raw.begin(), raw.end(),
                                 [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }),
                  raw.end());
        return base64Decode(raw);
    }
    return std::string();
}

std::string executableName(const std::string& infoPlist) {
    return valueAfterKey(infoPlist, "CFBundleExecutable");
}

fs::path codeTarget(const fs::path& contents, const std::string& name) {
    fs::path target = contents / name;
    if (fs::is_directory(target)) {
        // A nested .app is sealed as a bundle, not as a single file. Its
        // Info.plist says which Mach-O inside it carries the signature.
        const auto bytes = readFile(target / "Contents" / "Info.plist");
        const std::string plist(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        const std::string exe = executableName(plist);
        if (exe.empty()) refuse("nested bundle has no CFBundleExecutable");
        target = target / "Contents" / "MacOS" / exe;
    }
    return target;
}

void verifyResources(const fs::path& contents, const std::string& codeResources,
                     const ParsedCode& code, std::set<std::string>& sealed,
                     std::vector<std::string>& sealedDirs) {
    const size_t files2At = codeResources.find("<key>files2</key>");
    if (files2At == std::string::npos) refuse("CodeResources has no files2 dictionary");
    const std::string files2 = extractDict(codeResources, files2At);

    size_t at = 0;
    while (true) {
        const size_t keyAt = files2.find("<key>", at);
        if (keyAt == std::string::npos) break;
        const std::string name = findBetween(files2, keyAt, "<key>", "</key>");
        const size_t dictAt = files2.find("<dict>", keyAt + 5 + name.size() + 6);
        if (dictAt == std::string::npos) break;
        const std::string entry = extractDict(files2, dictAt);
        at = dictAt + 6 + entry.size() + 7;

        const fs::path target = contents / name;
        const bool isDirectory = fs::is_directory(target);
        const size_t optionalAt = entry.find("<key>optional</key>");
        const bool optional = optionalAt != std::string::npos &&
                              entry.find("<true/>", optionalAt) != std::string::npos;
        if (!fs::exists(target)) {
            if (optional) continue;
            refuse("sealed resource is missing");
        }
        sealed.insert(name);
        if (isDirectory) sealedDirs.push_back(name + "/");
        const std::string hash2 = valueAfterKey(entry, "hash2");
        const std::string cdhash = valueAfterKey(entry, "cdhash");
        if (!hash2.empty()) {
            if (hash2.size() != 32) refuse("bad hash2 in CodeResources");
            const auto bytes = readFile(target);
            const auto digest = sha256(bytes.data(), bytes.size());
            const auto* want = reinterpret_cast<const unsigned char*>(hash2.data());
            if (!std::equal(digest.begin(), digest.end(), want))
                refuse("resource hash mismatch");
        } else if (!cdhash.empty()) {
            const auto hashes = codeHashesOf(codeTarget(contents, name).string());
            const size_t n = std::min(cdhash.size(), size_t(32));
            const auto* want = reinterpret_cast<const unsigned char*>(cdhash.data());
            bool matched = false;
            for (const std::string& h : hashes) {
                const auto* got = reinterpret_cast<const unsigned char*>(h.data());
                if (h.size() >= n && std::equal(got, got + n, want)) matched = true;
            }
            if (!matched) refuse("nested code hash mismatch");
        } else {
            refuse("unknown entry in CodeResources files2");
        }
    }
    (void)code;
}

struct SliceRange {
    uint32_t offset = 0;
    uint32_t size = 0;
};

std::vector<SliceRange> allSlices(const std::vector<uint8_t>& file) {
    if (file.size() < 8) refuse("Mach-O file is too short");
    if (file[0] == 0xca && file[1] == 0xfe && file[2] == 0xba && file[3] == 0xbe) {
        const uint32_t count = be32(file.data() + 4);
        if (count == 0 || count > 64 || file.size() < 8 + size_t(count) * 20)
            refuse("bad Mach-O fat header");
        std::vector<SliceRange> out;
        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* arch = file.data() + 8 + size_t(i) * 20;
            SliceRange s{be32(arch + 8), be32(arch + 12)};
            if (uint64_t(s.offset) + s.size > file.size()) refuse("bad Mach-O slice");
            out.push_back(s);
        }
        return out;
    }
    if (file[0] == 0xcf && file[1] == 0xfa && file[2] == 0xed && file[3] == 0xfe)
        return {SliceRange{0, uint32_t(file.size())}};
    refuse("unrecognised Mach-O header");
}

// A universal binary is signed once per architecture, and CodeResources may
// seal it by any of those code directory hashes. The executable that is
// actually run is checked in full elsewhere; here the recorded cdhash only
// has to identify one of the slices.
std::vector<std::string> codeHashesOf(const std::string& path) {
    const auto file = readFile(path);
    std::vector<std::string> out;
    for (const auto& s : allSlices(file)) {
        if (s.size < 32 || le32(file.data() + s.offset) != kThin64MagicBytes) continue;
        const uint8_t* header = file.data() + s.offset;
        const uint32_t ncmds = le32(header + 16);
        const uint32_t sizeofcmds = le32(header + 20);
        if (uint64_t(32) + sizeofcmds > s.size || ncmds > 4096) continue;
        uint32_t dataoff = 0, datasize = 0;
        const uint8_t* cmd = header + 32;
        for (uint32_t i = 0; i < ncmds; ++i) {
            if (uint64_t(cmd - header) + 8 > s.size) break;
            const uint32_t kind = le32(cmd), length = le32(cmd + 4);
            if (length < 8 || uint64_t(cmd - header) + length > 32 + uint64_t(sizeofcmds)) break;
            if (kind == kLcCodeSignature && length >= 16) {
                dataoff = le32(cmd + 8);
                datasize = le32(cmd + 12);
            }
            cmd += length;
        }
        if (dataoff == 0 || datasize == 0 || uint64_t(dataoff) + datasize > s.size) continue;
        try {
            const auto sig = parseSuperBlob(header + dataoff, datasize);
            const auto& cdBlob = sig.slots.at(kSlotCodeDirectory);
            const auto cd = parseCodeDirectory(cdBlob.first, cdBlob.second);
            const auto digest = sha256(cd.bytes, cd.length);
            out.push_back(std::string(reinterpret_cast<const char*>(digest.data()), digest.size()));
        } catch (const Failure&) {
            continue;
        }
    }
    return out;
}

} // namespace

std::string codeDirectoryHash(const std::string& path) {
    try {
        const auto hashes = codeHashesOf(path);
        return hashes.empty() ? std::string() : hashes.front();
    } catch (const Failure&) {
        return std::string();
    } catch (const std::exception&) {
        return std::string();
    }
}

std::string verifyAppSignature(const std::string& appDir, const std::string& expectedIdentifier) {
    try {
        const fs::path root(appDir);
        const fs::path contents = root / "Contents";
        const auto infoBytes = readFile(contents / "Info.plist");
        const std::string infoPlist(reinterpret_cast<const char*>(infoBytes.data()), infoBytes.size());
        const std::string exeName = executableName(infoPlist);
        if (exeName.empty()) refuse("Info.plist has no CFBundleExecutable");

        const auto code = parseCode(contents / "MacOS" / exeName);
        verifyPages(code);
        verifyCms(code.sig, code.cd, expectedIdentifier);

        const auto special = code.cd.bytes + code.cd.digestOffset -
                             code.cd.nSpecial * code.cd.digestSize;
        auto specialHash = [&](int slot) -> std::array<uint8_t, 32> {
            for (uint32_t i = 0; i < code.cd.nSpecial; ++i)
                if (int(-(code.cd.nSpecial - i)) == slot) {
                    std::array<uint8_t, 32> out{};
                    std::copy_n(special + size_t(i) * code.cd.digestSize,
                                code.cd.digestSize, out.begin());
                    return out;
                }
            refuse("required special slot is absent");
        };
        const auto infoHash = sha256(infoBytes.data(), infoBytes.size());
        if (specialHash(-1) != infoHash) refuse("Info.plist hash mismatch");
        const auto resourcesBytes = readFile(contents / "_CodeSignature" / "CodeResources");
        const std::string resources(reinterpret_cast<const char*>(resourcesBytes.data()),
                                    resourcesBytes.size());
        const auto resourcesHash = sha256(resourcesBytes.data(), resourcesBytes.size());
        if (specialHash(-3) != resourcesHash) refuse("CodeResources hash mismatch");

        std::set<std::string> sealed;
        std::vector<std::string> sealedDirs;
        verifyResources(contents, resources, code, sealed, sealedDirs);

        // A file inside Contents that is neither sealed nor part of the
        // signature machinery would be code the seal never saw. The engine
        // flag file is the deliberate exception: TuxBlox writes it after
        // installation.
        fs::recursive_directory_iterator it(contents), end;
        for (; it != end; ++it) {
            if (!it->is_regular_file()) continue;
            const std::string rel = fs::relative(it->path(), contents).generic_string();
            if (rel == "Info.plist" || rel.rfind("_CodeSignature/", 0) == 0 ||
                rel == "MacOS/" + exeName || sealed.count(rel) != 0 ||
                rel.rfind("MacOS/ClientSettings/", 0) == 0)
                continue;
            bool underSealedDir = false;
            for (const std::string& dir : sealedDirs)
                if (rel.rfind(dir, 0) == 0) underSealedDir = true;
            if (underSealedDir) continue;
            refuse("unsealed file in the bundle");
        }
        return std::string();
    } catch (const Failure& f) {
        return f.reason;
    } catch (const std::exception&) {
        return "malformed signed bundle";
    }
}

} // namespace tuxblox::macclient
