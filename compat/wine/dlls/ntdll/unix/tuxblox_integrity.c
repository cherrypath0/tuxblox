/*
 * Authenticode verification for the image a process was started with
 *
 * Checks that the Roblox client a process is about to run carries a signature
 * that matches its bytes and chains to a root we ship. It sits here rather than
 * in front of the layer because every launch passes through ntdll, however it
 * was started, and nothing else is true of every launch.
 *
 * Not WinVerifyTrust: crypt32 has no decoder for the content type a signed
 * Windows program uses, so that path rejects genuine files. The structure is
 * walked by hand here instead.
 *
 * Copyright 2026 TuxBlox Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include <stddef.h>
#include <string.h>

#include "tuxblox_integrity.h"

/* The only image whose signature is insisted on is the Roblox client, which is a
 * 64-bit PE, so the 32-bit loader never meets one and is built without any of
 * this. gnutls is only built for x86_64 for the same reason. */
#ifdef __x86_64__

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>
#include <gnutls/crypto.h>

/* Offsets inside a PE image, from the Microsoft PE format specification. */
#define PE_OFFSET_FIELD          0x3c
#define COFF_HEADER_SIZE         20
#define CHECKSUM_IN_OPT_HEADER   64
#define DATA_DIRS_IN_PE32        96
#define DATA_DIRS_IN_PE32PLUS    112
#define SECURITY_DIRECTORY_INDEX 4
#define DATA_DIRECTORY_ENTRY     8
#define PE32_MAGIC               0x10b
#define PE32PLUS_MAGIC           0x20b
#define WIN_CERTIFICATE_HEADER   8

/* A signature larger than this is not one we are going to understand. */
#define MAX_SIGNATURE_BYTES      (4u * 1024u * 1024u)

/* OIDs walked by hand. */
static const char OID_SIGNED_DATA[]    = "1.2.840.113549.1.7.2";
static const char OID_MESSAGE_DIGEST[] = "1.2.840.113549.1.9.4";

struct pe_layout
{
    size_t checksum_offset;
    size_t security_entry_offset;
    size_t certificate_offset;
    size_t certificate_size;
    size_t file_size;
};

/* One DER element: its tag, its contents, and where the next one starts. */
struct element
{
    unsigned char tag;
    /* The tag byte. value..next is the content; start..next is the whole thing,
     * which is what a DER parser handed one of these wants. */
    const unsigned char *start;
    const unsigned char *value;
    size_t length;
    const unsigned char *next;
    int ok;
};

static size_t element_total( const struct element *element )
{
    return (size_t)(element->next - element->start);
}

/* The step that refused, for the line this gets logged on. */
static const char *last_detail = "";

#define FAIL(reason) do { last_detail = (reason); goto done; } while (0)

const char *tuxblox_integrity_detail(void)
{
    return last_detail;
}

/* Everything we borrow from gnutls, resolved once. Loaded rather than linked:
 * ntdll is in every process, and linking would map gnutls, nettle and gmp into
 * all of them for a check almost none of them run. */
static void *gnutls_handle;
static int gnutls_ready = -1;

#define GNUTLS_FUNCS \
    USE_FUNC(gnutls_global_init) \
    USE_FUNC(gnutls_hash_init) \
    USE_FUNC(gnutls_hash) \
    USE_FUNC(gnutls_hash_deinit) \
    USE_FUNC(gnutls_hash_get_len) \
    USE_FUNC(gnutls_hash_fast) \
    USE_FUNC(gnutls_x509_crt_init) \
    USE_FUNC(gnutls_x509_crt_deinit) \
    USE_FUNC(gnutls_x509_crt_import) \
    USE_FUNC(gnutls_x509_crt_get_dn_by_oid) \
    USE_FUNC(gnutls_x509_crt_get_serial) \
    USE_FUNC(gnutls_x509_crt_get_raw_issuer_dn) \
    USE_FUNC(gnutls_x509_crt_get_raw_dn) \
    USE_FUNC(gnutls_x509_crt_get_key_purpose_oid) \
    USE_FUNC(gnutls_x509_crt_get_key_usage) \
    USE_FUNC(gnutls_x509_trust_list_init) \
    USE_FUNC(gnutls_x509_trust_list_deinit) \
    USE_FUNC(gnutls_x509_trust_list_add_trust_mem) \
    USE_FUNC(gnutls_x509_trust_list_verify_crt) \
    USE_FUNC(gnutls_pubkey_init) \
    USE_FUNC(gnutls_pubkey_deinit) \
    USE_FUNC(gnutls_pubkey_import_x509) \
    USE_FUNC(gnutls_pubkey_verify_data2) \
    USE_FUNC(gnutls_strerror)

#define USE_FUNC(f) static typeof(f) *p_##f;
GNUTLS_FUNCS
#undef USE_FUNC

static int load_gnutls( const char *path )
{
    if (gnutls_ready != -1) return gnutls_ready;
    gnutls_ready = 0;

    if (!(gnutls_handle = dlopen( path, RTLD_NOW ))) return 0;

#define USE_FUNC(f) \
    if (!(p_##f = dlsym( gnutls_handle, #f ))) { dlclose( gnutls_handle ); gnutls_handle = NULL; return 0; }
    GNUTLS_FUNCS
#undef USE_FUNC

    p_gnutls_global_init();
    gnutls_ready = 1;
    return 1;
}

static unsigned int read_u16( const unsigned char *data )
{
    return data[0] | (data[1] << 8);
}

static unsigned int read_u32( const unsigned char *data )
{
    return (unsigned int)data[0] | ((unsigned int)data[1] << 8) |
           ((unsigned int)data[2] << 16) | ((unsigned int)data[3] << 24);
}

/* A read that stops rather than reading past the end. */
static int read_at( int fd, size_t offset, void *out, size_t size )
{
    size_t done = 0;

    while (done < size)
    {
        ssize_t got = pread( fd, (char *)out + done, size - done, (off_t)(offset + done) );
        if (got <= 0) return 0;
        done += (size_t)got;
    }
    return 1;
}

static int read_pe_layout( int fd, size_t file_size, struct pe_layout *layout )
{
    unsigned char buffer[DATA_DIRECTORY_ENTRY];
    size_t pe_offset, optional_header, data_directories;
    unsigned int magic;

    memset( layout, 0, sizeof(*layout) );

    if (file_size < PE_OFFSET_FIELD + 4) return 0;
    if (!read_at( fd, PE_OFFSET_FIELD, buffer, 4 )) return 0;
    pe_offset = read_u32( buffer );
    if (pe_offset + COFF_HEADER_SIZE + 4 > file_size) return 0;

    if (!read_at( fd, pe_offset, buffer, 4 )) return 0;
    if (buffer[0] != 'P' || buffer[1] != 'E' || buffer[2] || buffer[3]) return 0;

    optional_header = pe_offset + 4 + COFF_HEADER_SIZE;
    if (optional_header + 2 > file_size) return 0;
    if (!read_at( fd, optional_header, buffer, 2 )) return 0;

    magic = read_u16( buffer );
    if (magic == PE32_MAGIC) data_directories = optional_header + DATA_DIRS_IN_PE32;
    else if (magic == PE32PLUS_MAGIC) data_directories = optional_header + DATA_DIRS_IN_PE32PLUS;
    else return 0;

    layout->checksum_offset = optional_header + CHECKSUM_IN_OPT_HEADER;
    layout->security_entry_offset = data_directories + SECURITY_DIRECTORY_INDEX * DATA_DIRECTORY_ENTRY;
    layout->file_size = file_size;
    if (layout->security_entry_offset + DATA_DIRECTORY_ENTRY > file_size) return 0;

    if (!read_at( fd, layout->security_entry_offset, buffer, DATA_DIRECTORY_ENTRY )) return 0;
    /* For this one directory the first field is a file offset, not an RVA. */
    layout->certificate_offset = read_u32( buffer );
    layout->certificate_size = read_u32( buffer + 4 );
    return 1;
}

/* The Authenticode digest: the whole file except its checksum, the entry
 * describing where the signature is, and the signature itself. Those three are
 * skipped because signing the file is what puts them there. */
static int digest_image( int fd, const struct pe_layout *layout, gnutls_digest_algorithm_t algorithm,
                         unsigned char *out, size_t *out_size )
{
    struct { size_t start, end; } regions[3];
    gnutls_hash_hd_t hash;
    char *chunk;
    unsigned int i;
    int size;

    regions[0].start = 0;                                                  regions[0].end = layout->checksum_offset;
    regions[1].start = layout->checksum_offset + 4;                        regions[1].end = layout->security_entry_offset;
    regions[2].start = layout->security_entry_offset + DATA_DIRECTORY_ENTRY; regions[2].end = layout->certificate_offset;

    size = p_gnutls_hash_get_len( algorithm );
    if (size <= 0 || (size_t)size > *out_size) return 0;
    if (p_gnutls_hash_init( &hash, algorithm ) < 0) return 0;
    if (!(chunk = malloc( 1u << 20 )))
    {
        p_gnutls_hash_deinit( hash, NULL );
        return 0;
    }

    for (i = 0; i < 3; i++)
    {
        size_t offset = regions[i].start, remaining;

        if (regions[i].end < regions[i].start || regions[i].end > layout->file_size) goto failed;
        remaining = regions[i].end - regions[i].start;
        while (remaining)
        {
            size_t want = remaining < (1u << 20) ? remaining : (1u << 20);

            if (!read_at( fd, offset, chunk, want )) goto failed;
            if (p_gnutls_hash( hash, chunk, want ) < 0) goto failed;
            offset += want;
            remaining -= want;
        }
    }

    free( chunk );
    p_gnutls_hash_deinit( hash, out );
    *out_size = (size_t)size;
    return 1;

failed:
    free( chunk );
    p_gnutls_hash_deinit( hash, NULL );
    return 0;
}

static struct element read_element( const unsigned char *data, const unsigned char *end )
{
    struct element element;
    size_t length;

    memset( &element, 0, sizeof(element) );
    if (end - data < 2) return element;

    element.start = data;
    element.tag = *data++;
    length = *data++;
    if (length & 0x80)
    {
        size_t count = length & 0x7f, i;

        /* Lengths this long do not appear in a signature, and allowing them
         * would only risk an overflow below. */
        if (!count || count > 4 || (size_t)(end - data) < count) return element;
        length = 0;
        for (i = 0; i < count; i++) length = (length << 8) | *data++;
    }
    if ((size_t)(end - data) < length) return element;

    element.value = data;
    element.length = length;
    element.next = data + length;
    element.ok = 1;
    return element;
}

/* Walks into a constructed element, returning its first child. */
static struct element first_child( const struct element *parent )
{
    return read_element( parent->value, parent->value + parent->length );
}

/* The OID in an AlgorithmIdentifier, as a dotted string, so it can be compared
 * without a table of numeric identifiers. */
static int oid_string( const unsigned char *data, size_t length, char *out, size_t out_size )
{
    size_t i, pos = 0;
    unsigned long long value = 0;
    int written;

    if (!length) return 0;

    written = snprintf( out, out_size, "%u.%u", data[0] / 40, data[0] % 40 );
    if (written < 0 || (size_t)written >= out_size) return 0;
    pos = (size_t)written;

    for (i = 1; i < length; i++)
    {
        value = (value << 7) | (data[i] & 0x7f);
        if (data[i] & 0x80) continue;
        written = snprintf( out + pos, out_size - pos, ".%llu", value );
        if (written < 0 || (size_t)written >= out_size - pos) return 0;
        pos += (size_t)written;
        value = 0;
    }
    out[pos] = 0;
    return 1;
}

/* An AlgorithmIdentifier's OID, mapped to the digest it names. */
static gnutls_digest_algorithm_t digest_from_algorithm( const struct element *algorithm )
{
    struct element oid = first_child( algorithm );
    char text[128];

    if (!oid.ok || oid.tag != 0x06) return GNUTLS_DIG_UNKNOWN;
    if (!oid_string( oid.value, oid.length, text, sizeof(text) )) return GNUTLS_DIG_UNKNOWN;

    if (!strcmp( text, "1.3.14.3.2.26" ))        return GNUTLS_DIG_SHA1;
    if (!strcmp( text, "2.16.840.1.101.3.4.2.1" )) return GNUTLS_DIG_SHA256;
    if (!strcmp( text, "2.16.840.1.101.3.4.2.2" )) return GNUTLS_DIG_SHA384;
    if (!strcmp( text, "2.16.840.1.101.3.4.2.3" )) return GNUTLS_DIG_SHA512;
    return GNUTLS_DIG_UNKNOWN;
}

static gnutls_sign_algorithm_t rsa_signature_for( gnutls_digest_algorithm_t digest )
{
    switch (digest)
    {
    case GNUTLS_DIG_SHA1:   return GNUTLS_SIGN_RSA_SHA1;
    case GNUTLS_DIG_SHA256: return GNUTLS_SIGN_RSA_SHA256;
    case GNUTLS_DIG_SHA384: return GNUTLS_SIGN_RSA_SHA384;
    case GNUTLS_DIG_SHA512: return GNUTLS_SIGN_RSA_SHA512;
    default: return GNUTLS_SIGN_UNKNOWN;
    }
}

/* SpcIndirectDataContent ::= SEQUENCE { data, messageDigest DigestInfo }
 * DigestInfo             ::= SEQUENCE { digestAlgorithm, digest OCTET STRING } */
static int read_signed_digest( const unsigned char *content, size_t content_length,
                               gnutls_digest_algorithm_t *algorithm,
                               const unsigned char **digest, size_t *digest_length )
{
    const unsigned char *end = content + content_length;
    struct element outer, attribute, digest_info, algorithm_id, value;
    const unsigned char *info_end;

    outer = read_element( content, end );
    if (!outer.ok || outer.tag != 0x30) return 0;

    attribute = first_child( &outer );
    if (!attribute.ok) return 0;

    digest_info = read_element( attribute.next, outer.value + outer.length );
    if (!digest_info.ok || digest_info.tag != 0x30) return 0;

    info_end = digest_info.value + digest_info.length;
    algorithm_id = read_element( digest_info.value, info_end );
    if (!algorithm_id.ok || algorithm_id.tag != 0x30) return 0;

    *algorithm = digest_from_algorithm( &algorithm_id );
    if (*algorithm == GNUTLS_DIG_UNKNOWN) return 0;

    value = read_element( algorithm_id.next, info_end );
    if (!value.ok || value.tag != 0x04) return 0;

    *digest = value.value;
    *digest_length = value.length;
    return 1;
}

/* ContentInfo ::= SEQUENCE { contentType OID, content [0] EXPLICIT SignedData }
 * SignedData  ::= SEQUENCE { version, digestAlgorithms, encapContentInfo,
 *                            certificates [0] IMPLICIT, signerInfos SET } */
struct signed_data
{
    struct element content;      /* the SpcIndirectDataContent, outer SEQUENCE included */
    struct element certificates; /* [0] IMPLICIT SET OF Certificate */
    struct element signer_info;  /* the first SignerInfo */
};

static int read_signed_data( const unsigned char *data, size_t length, struct signed_data *out )
{
    const unsigned char *end = data + length;
    struct element info, type, wrapper, signed_data, item, encap, content_type;
    const unsigned char *signed_end;

    info = read_element( data, end );
    if (!info.ok || info.tag != 0x30) return 0;

    type = first_child( &info );
    if (!type.ok || type.tag != 0x06) return 0;
    {
        char text[128];
        if (!oid_string( type.value, type.length, text, sizeof(text) )) return 0;
        if (strcmp( text, OID_SIGNED_DATA )) return 0;
    }

    wrapper = read_element( type.next, info.value + info.length );
    if (!wrapper.ok || wrapper.tag != 0xa0) return 0;

    signed_data = first_child( &wrapper );
    if (!signed_data.ok || signed_data.tag != 0x30) return 0;
    signed_end = signed_data.value + signed_data.length;

    /* version */
    item = first_child( &signed_data );
    if (!item.ok) return 0;
    /* digestAlgorithms */
    item = read_element( item.next, signed_end );
    if (!item.ok) return 0;

    /* encapContentInfo: the content type, then [0] EXPLICIT holding the content */
    encap = read_element( item.next, signed_end );
    if (!encap.ok || encap.tag != 0x30) return 0;
    content_type = first_child( &encap );
    if (!content_type.ok || content_type.tag != 0x06) return 0;
    {
        struct element holder = read_element( content_type.next, encap.value + encap.length );
        if (!holder.ok || holder.tag != 0xa0) return 0;
        out->content = first_child( &holder );
        if (!out->content.ok) return 0;
    }

    /* certificates [0] IMPLICIT, optional but always present for Authenticode */
    item = read_element( encap.next, signed_end );
    if (!item.ok) return 0;
    if (item.tag != 0xa0) return 0;
    out->certificates = item;

    /* crls [1] IMPLICIT may sit between; signerInfos is the SET that follows */
    item = read_element( item.next, signed_end );
    if (item.ok && item.tag == 0xa1) item = read_element( item.next, signed_end );
    if (!item.ok || item.tag != 0x31) return 0;

    out->signer_info = first_child( &item );
    if (!out->signer_info.ok || out->signer_info.tag != 0x30) return 0;
    return 1;
}

/* SignerInfo ::= SEQUENCE { version, issuerAndSerialNumber, digestAlgorithm,
 *                            authenticatedAttributes [0] IMPLICIT OPTIONAL,
 *                            digestEncryptionAlgorithm, encryptedDigest } */
struct signer_info
{
    struct element issuer_and_serial;
    struct element digest_algorithm;
    struct element auth_attributes; /* [0] IMPLICIT, tag 0xa0 */
    struct element encrypted_digest;
};

static int read_signer_info( const struct element *signer, struct signer_info *out )
{
    const unsigned char *end = signer->value + signer->length;
    struct element item;

    memset( out, 0, sizeof(*out) );

    item = first_child( signer );                       /* version */
    if (!item.ok) return 0;
    out->issuer_and_serial = read_element( item.next, end );
    if (!out->issuer_and_serial.ok || out->issuer_and_serial.tag != 0x30) return 0;
    out->digest_algorithm = read_element( out->issuer_and_serial.next, end );
    if (!out->digest_algorithm.ok || out->digest_algorithm.tag != 0x30) return 0;

    item = read_element( out->digest_algorithm.next, end );
    if (!item.ok) return 0;
    if (item.tag == 0xa0)
    {
        out->auth_attributes = item;
        item = read_element( item.next, end );
        if (!item.ok) return 0;
    }
    /* digestEncryptionAlgorithm, then the signature itself */
    if (item.tag != 0x30) return 0;
    out->encrypted_digest = read_element( item.next, end );
    if (!out->encrypted_digest.ok || out->encrypted_digest.tag != 0x04) return 0;
    return 1;
}

/* The value of one authenticated attribute, by OID. */
static int find_attribute( const struct element *attributes, const char *wanted,
                           struct element *value )
{
    const unsigned char *end = attributes->value + attributes->length;
    struct element attribute = first_child( attributes );

    while (attribute.ok)
    {
        struct element oid = first_child( &attribute );
        char text[128];

        if (oid.ok && oid.tag == 0x06 && oid_string( oid.value, oid.length, text, sizeof(text) ) &&
            !strcmp( text, wanted ))
        {
            struct element set = read_element( oid.next, attribute.value + attribute.length );

            if (!set.ok || set.tag != 0x31) return 0;
            *value = first_child( &set );
            return value->ok;
        }
        attribute = read_element( attribute.next, end );
    }
    return 0;
}

/* At most this many certificates out of one signature, which is far more than
 * a signing chain ever carries. */
#define MAX_CERTIFICATES 16

struct certificate_set
{
    gnutls_x509_crt_t items[MAX_CERTIFICATES];
    unsigned int count;
};

static void free_certificates( struct certificate_set *set, const gnutls_x509_crt_t *keep,
                               unsigned int keep_count )
{
    unsigned int i, j;

    for (i = 0; i < set->count; i++)
    {
        int kept = 0;

        for (j = 0; j < keep_count; j++) if (keep[j] == set->items[i]) kept = 1;
        if (!kept) p_gnutls_x509_crt_deinit( set->items[i] );
    }
    set->count = 0;
}

/* Every certificate the signature carries, in the order it carries them. */
static void load_certificates( const struct element *certificates, struct certificate_set *set )
{
    const unsigned char *end = certificates->value + certificates->length;
    struct element certificate = first_child( certificates );

    set->count = 0;
    while (certificate.ok && set->count < MAX_CERTIFICATES)
    {
        gnutls_x509_crt_t crt;
        gnutls_datum_t der;

        der.data = (unsigned char *)certificate.start;
        der.size = (unsigned int)element_total( &certificate );

        if (p_gnutls_x509_crt_init( &crt ) >= 0)
        {
            if (p_gnutls_x509_crt_import( crt, &der, GNUTLS_X509_FMT_DER ) >= 0)
                set->items[set->count++] = crt;
            else
                p_gnutls_x509_crt_deinit( crt );
        }
        certificate = read_element( certificate.next, end );
    }
}

/* The one the signer names, by issuer and serial. */
static int find_signing_certificate( const struct certificate_set *set,
                                     const struct element *issuer_and_serial )
{
    struct element wanted_issuer, wanted_serial;
    unsigned int i;

    wanted_issuer = first_child( issuer_and_serial );
    if (!wanted_issuer.ok) return -1;
    wanted_serial = read_element( wanted_issuer.next,
                                  issuer_and_serial->value + issuer_and_serial->length );
    if (!wanted_serial.ok || wanted_serial.tag != 0x02) return -1;

    for (i = 0; i < set->count; i++)
    {
        unsigned char serial[128];
        size_t serial_size = sizeof(serial);
        gnutls_datum_t issuer = { NULL, 0 };

        if (p_gnutls_x509_crt_get_serial( set->items[i], serial, &serial_size ) < 0) continue;
        if (p_gnutls_x509_crt_get_raw_issuer_dn( set->items[i], &issuer ) < 0) continue;
        /* Both sides are DER, so this compares the names rather than reading them. */
        if (serial_size == wanted_serial.length &&
            !memcmp( serial, wanted_serial.value, serial_size ) &&
            issuer.size == element_total( &wanted_issuer ) &&
            !memcmp( issuer.data, wanted_issuer.start, issuer.size ))
            return (int)i;
    }
    return -1;
}

/* The chain from the leaf upwards, each certificate followed by the one that
 * issued it. gnutls wants the chain itself, where OpenSSL took the leaf and a
 * bag of certificates to pick from, so it is assembled here. The root is left
 * out: that has to come from the trust list, not from the signature. */
static unsigned int build_chain( const struct certificate_set *set, unsigned int leaf,
                                 gnutls_x509_crt_t *chain )
{
    unsigned int length = 0, i;
    int used[MAX_CERTIFICATES] = { 0 };

    chain[length++] = set->items[leaf];
    used[leaf] = 1;

    while (length < MAX_CERTIFICATES)
    {
        gnutls_datum_t issuer = { NULL, 0 };
        int next = -1;

        if (p_gnutls_x509_crt_get_raw_issuer_dn( chain[length - 1], &issuer ) < 0) break;

        for (i = 0; i < set->count && next < 0; i++)
        {
            gnutls_datum_t subject = { NULL, 0 };

            if (used[i]) continue;
            if (p_gnutls_x509_crt_get_raw_dn( set->items[i], &subject ) < 0) continue;
            if (subject.size == issuer.size && !memcmp( subject.data, issuer.data, subject.size ))
                next = (int)i;
        }
        if (next < 0) break;
        used[next] = 1;
        chain[length++] = set->items[next];
    }
    return length;
}

/* Whether the leaf is a certificate for signing code, rather than one that
 * merely chains to a root we trust.
 *
 * The roots shipped are a general set, so without this a certificate issued for
 * something else -- a web server, say -- counts as a signing key, which is not
 * what Windows accepts and not what the signature claims to be.
 */
static int certificate_may_sign_code( gnutls_x509_crt_t leaf )
{
    unsigned int usage = 0, critical = 0, i;
    int found = 0;

    for (i = 0; ; i++)
    {
        char oid[128];
        size_t length = sizeof(oid);

        if (p_gnutls_x509_crt_get_key_purpose_oid( leaf, i, oid, &length, &critical ) < 0) break;
        if (!strcmp( oid, GNUTLS_KP_CODE_SIGNING ) || !strcmp( oid, GNUTLS_KP_ANY )) found = 1;
    }
    if (!found) return 0;

    /* Only when it says so: a certificate listing no usage constrains none. */
    if (p_gnutls_x509_crt_get_key_usage( leaf, &usage, &critical ) >= 0 &&
        !(usage & GNUTLS_KEY_DIGITAL_SIGNATURE))
        return 0;
    return 1;
}

const char *tuxblox_integrity_status_name( enum tuxblox_integrity_status status )
{
    switch (status)
    {
    case TUXBLOX_INTEGRITY_VERIFIED:        return "verified";
    case TUXBLOX_INTEGRITY_NOT_SIGNED:      return "not signed";
    case TUXBLOX_INTEGRITY_MODIFIED:        return "modified";
    case TUXBLOX_INTEGRITY_BAD_SIGNATURE:   return "bad signature";
    case TUXBLOX_INTEGRITY_WRONG_PUBLISHER: return "wrong publisher";
    case TUXBLOX_INTEGRITY_UNAVAILABLE:     return "unavailable";
    default:                                return "unreadable";
    }
}

enum tuxblox_integrity_status tuxblox_verify_authenticode( const char *path, const char *gnutls_path,
                                                           const char *roots_pem, size_t roots_length,
                                                           const char **accepted_publishers,
                                                           unsigned int accepted_count,
                                                           char *publisher, size_t publisher_size )
{
    enum tuxblox_integrity_status status = TUXBLOX_INTEGRITY_UNREADABLE;
    struct pe_layout layout;
    struct signed_data signed_data;
    struct signer_info signer;
    struct element message_digest, content_body;
    gnutls_digest_algorithm_t content_algorithm, signer_algorithm;
    struct certificate_set certificates = { { 0 }, 0 };
    gnutls_x509_crt_t chain[MAX_CERTIFICATES];
    gnutls_x509_crt_t leaf = NULL;
    unsigned int chain_length = 0;
    gnutls_x509_trust_list_t trust = NULL;
    gnutls_pubkey_t key = NULL;
    unsigned char *signature = NULL;
    unsigned char *attributes = NULL;
    unsigned char actual[64], claimed_content[64];
    const unsigned char *signed_digest = NULL;
    size_t signed_digest_length = 0, actual_size = sizeof(actual);
    size_t attributes_length = 0;
    unsigned int verify_result = 0;
    off_t file_size;
    int fd = -1;

    if (publisher && publisher_size) publisher[0] = 0;

    /* Refused rather than taken to mean anybody may sign: the wrong way round for the one check that says who may. */
    if (!accepted_publishers || !accepted_count)
    {
        last_detail = "no publisher is accepted";
        return TUXBLOX_INTEGRITY_WRONG_PUBLISHER;
    }

    if (!load_gnutls( gnutls_path )) return TUXBLOX_INTEGRITY_UNAVAILABLE;

    if ((fd = open( path, O_RDONLY )) < 0) return TUXBLOX_INTEGRITY_UNREADABLE;
    if ((file_size = lseek( fd, 0, SEEK_END )) <= 0) FAIL( "empty file" );

    if (!read_pe_layout( fd, (size_t)file_size, &layout )) FAIL( "not a Windows program" );

    if (!layout.certificate_size || !layout.certificate_offset)
    {
        status = TUXBLOX_INTEGRITY_NOT_SIGNED;
        FAIL( "no signature" );
    }
    if (layout.certificate_offset + layout.certificate_size > layout.file_size ||
        layout.certificate_size <= WIN_CERTIFICATE_HEADER ||
        layout.certificate_size > MAX_SIGNATURE_BYTES)
    {
        status = TUXBLOX_INTEGRITY_BAD_SIGNATURE;
        FAIL( "signature runs past the end of the file" );
    }

    signature = malloc( layout.certificate_size - WIN_CERTIFICATE_HEADER );
    if (!signature) FAIL( "out of memory" );
    if (!read_at( fd, layout.certificate_offset + WIN_CERTIFICATE_HEADER, signature,
                  layout.certificate_size - WIN_CERTIFICATE_HEADER ))
    {
        status = TUXBLOX_INTEGRITY_BAD_SIGNATURE;
        FAIL( "signature could not be read" );
    }

    status = TUXBLOX_INTEGRITY_BAD_SIGNATURE;
    if (!read_signed_data( signature, layout.certificate_size - WIN_CERTIFICATE_HEADER, &signed_data ))
        FAIL( "signature is not readable" );
    if (!read_signer_info( &signed_data.signer_info, &signer )) FAIL( "signer information is not readable" );
    if (!signer.auth_attributes.ok) FAIL( "signature carries no signed attributes" );

    /* What the signature says the image hashes to. */
    content_body = signed_data.content;
    if (!read_signed_digest( content_body.start, element_total( &content_body ),
                             &content_algorithm, &signed_digest, &signed_digest_length ))
        FAIL( "signature does not say what it covers" );

    if (!digest_image( fd, &layout, content_algorithm, actual, &actual_size )) FAIL( "could not read all of the file" );
    if (actual_size != signed_digest_length || memcmp( actual, signed_digest, actual_size ))
    {
        status = TUXBLOX_INTEGRITY_MODIFIED;
        FAIL( "the file does not match its signature" );
    }

    /* The signature covers the authenticated attributes, one of which is the
     * digest of the content checked above, so this ties the two together. */
    signer_algorithm = digest_from_algorithm( &signer.digest_algorithm );
    if (signer_algorithm == GNUTLS_DIG_UNKNOWN) FAIL( "signer uses an unknown digest" );
    if (!find_attribute( &signer.auth_attributes, OID_MESSAGE_DIGEST, &message_digest )) FAIL( "signature does not cover its own content" );
    if (message_digest.tag != 0x04) FAIL( "content digest is not an octet string" );

    /* What the signer digested is the content WITHOUT its outer SEQUENCE
     * header: the header is part of how the content is wrapped, not part of the
     * content. Including it makes every genuine signature look broken. */
    {
        gnutls_datum_t body;
        size_t content_digest_size = sizeof(claimed_content);
        int length = p_gnutls_hash_get_len( signer_algorithm );

        body.data = (unsigned char *)content_body.value;
        body.size = (unsigned int)content_body.length;
        if (length <= 0 || (size_t)length > content_digest_size) FAIL( "digest too large" );
        if (p_gnutls_hash_fast( signer_algorithm, body.data, body.size, claimed_content ) < 0) FAIL( "could not digest the signed content" );
        if (message_digest.length != (size_t)length ||
            memcmp( message_digest.value, claimed_content, (size_t)length ))
        {
            status = TUXBLOX_INTEGRITY_MODIFIED;
            FAIL( "signature does not match its own content" );
        }
    }

    load_certificates( &signed_data.certificates, &certificates );
    {
        int index = find_signing_certificate( &certificates, &signer.issuer_and_serial );

        if (index < 0) FAIL( "the signing certificate is missing" );
        leaf = certificates.items[index];
        chain_length = build_chain( &certificates, (unsigned int)index, chain );
    }

    /* The attributes are signed as a SET OF, not with the implicit [0] tag they
     * carry here, so the tag is swapped before verifying. */
    {
        gnutls_datum_t data, sig;

        /* Only the tag byte changes: the length encoding is the same either way. */
        attributes_length = element_total( &signer.auth_attributes );
        if (!(attributes = malloc( attributes_length ))) FAIL( "out of memory" );
        memcpy( attributes, signer.auth_attributes.start, attributes_length );
        attributes[0] = 0x31;

        if (p_gnutls_pubkey_init( &key ) < 0) FAIL( "no key" );
        if (p_gnutls_pubkey_import_x509( key, leaf, 0 ) < 0) FAIL( "signing certificate has no usable key" );

        data.data = attributes;
        data.size = (unsigned int)attributes_length;
        sig.data = (unsigned char *)signer.encrypted_digest.value;
        sig.size = (unsigned int)signer.encrypted_digest.length;

        if (p_gnutls_pubkey_verify_data2( key, rsa_signature_for( signer_algorithm ), 0,
                                          &data, &sig ) < 0)
            FAIL( "the signature does not verify" );
    }

    /* Signing certificates are deliberately short-lived and the signature
     * carries a timestamp from when they were valid, so a program signed last
     * year is still correctly signed today. Checking dates here would reject
     * every older release the day its certificate lapsed. */
    {
        gnutls_datum_t roots;

        roots.data = (unsigned char *)roots_pem;
        roots.size = (unsigned int)roots_length;
        if (p_gnutls_x509_trust_list_init( &trust, 0 ) < 0) FAIL( "no trust list" );
        if (p_gnutls_x509_trust_list_add_trust_mem( trust, &roots, NULL, GNUTLS_X509_FMT_PEM, 0, 0 ) <= 0)
        {
            status = TUXBLOX_INTEGRITY_UNREADABLE;
            FAIL( "no root certificates to check against" );
        }
        if (p_gnutls_x509_trust_list_verify_crt( trust, chain, chain_length,
                                                 GNUTLS_VERIFY_DISABLE_TIME_CHECKS |
                                                 GNUTLS_VERIFY_DISABLE_CRL_CHECKS,
                                                 &verify_result, NULL ) < 0 ||
            verify_result)
        {
            status = TUXBLOX_INTEGRITY_BAD_SIGNATURE;
            FAIL( "certificate chain is not trusted" );
        }
    }

    if (!certificate_may_sign_code( leaf ))
    {
        status = TUXBLOX_INTEGRITY_BAD_SIGNATURE;
        FAIL( "the signing certificate is not for signing code" );
    }

    /* X.500 compares organisation names without regard to case, and Roblox has
     * signed as both "Roblox Corporation" and "ROBLOX CORPORATION", so an exact
     * match rejects its own binaries. */
    {
        char organisation[256];
        size_t organisation_size = sizeof(organisation);
        unsigned int i;
        int recognised = 0;

        if (p_gnutls_x509_crt_get_dn_by_oid( leaf, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, 0,
                                             organisation, &organisation_size ) < 0)
            FAIL( "certificate names no organisation" );
        if (publisher && publisher_size)
        {
            strncpy( publisher, organisation, publisher_size - 1 );
            publisher[publisher_size - 1] = 0;
        }
        for (i = 0; i < accepted_count; i++)
            if (!strcasecmp( organisation, accepted_publishers[i] )) recognised = 1;
        if (!recognised)
        {
            status = TUXBLOX_INTEGRITY_WRONG_PUBLISHER;
            FAIL( "signed by somebody else" );
        }
    }

    status = TUXBLOX_INTEGRITY_VERIFIED;

done:
    if (key) p_gnutls_pubkey_deinit( key );
    if (trust) p_gnutls_x509_trust_list_deinit( trust, 0 );
    free_certificates( &certificates, NULL, 0 );
    free( attributes );
    free( signature );
    if (fd >= 0) close( fd );
    return status;
}

#else /* __x86_64__ */

const char *tuxblox_integrity_status_name( enum tuxblox_integrity_status status )
{
    return status == TUXBLOX_INTEGRITY_VERIFIED ? "verified" : "unavailable";
}

const char *tuxblox_integrity_detail(void)
{
    return "not built for this architecture";
}

enum tuxblox_integrity_status tuxblox_verify_authenticode( const char *path, const char *gnutls_path,
                                                           const char *roots_pem, size_t roots_length,
                                                           const char **accepted_publishers,
                                                           unsigned int accepted_count,
                                                           char *publisher, size_t publisher_size )
{
    (void)path; (void)gnutls_path; (void)roots_pem; (void)roots_length;
    (void)accepted_publishers; (void)accepted_count;
    if (publisher && publisher_size) publisher[0] = 0;
    return TUXBLOX_INTEGRITY_UNAVAILABLE;
}

#endif /* __x86_64__ */
