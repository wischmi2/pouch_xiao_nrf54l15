/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cert.h"
#include <errno.h>
#include <mbedtls/error.h>
#include <psa/crypto.h>
#include <pouch/port.h>
#include <pouch/transport/certificate.h>
#include <stdlib.h>
#include <string.h>

POUCH_LOG_REGISTER(cert, CONFIG_POUCH_COMMON_LOG_LEVEL);

#if IS_ENABLED(CONFIG_POUCH_VALIDATE_SERVER_CERT)
#include <pouch/pouch_ca_cert.h>
#else
static const struct pouch_cert empty_cert = {.buffer = NULL, .size = 0};
static const struct pouch_cert *pouch_ca_cert = &empty_cert;
#endif

static struct
{
    struct pouch_cert certificate;
    uint8_t ref[CERT_REF_LEN];
} device;
static struct
{
    struct pubkey pubkey;
    struct
    {
        uint8_t data[CERT_SERIAL_MAXLEN];
        size_t len;
    } serial;
} server_cert;

static inline bool cert_is_valid(const struct pouch_cert *cert)
{
    return cert != NULL && cert->buffer != NULL && cert->size > 0;
}

static void log_cert_info(const char *label, const mbedtls_x509_crt *cert)
{
#if defined(MBEDTLS_X509_REMOVE_INFO)
    ARG_UNUSED(cert);
    POUCH_LOG_INF("%s: certificate parsed", label);
#else
    char info[768];
    int ret = mbedtls_x509_crt_info(info, sizeof(info), "  ", cert);

    if (ret > 0)
    {
        POUCH_LOG_INF("%s:\n%s", label, info);
    }
    else
    {
        POUCH_LOG_WRN("Unable to format %s info: -0x%x", label, -ret);
    }
#endif
}

static void log_chain_summary(const char *label, const mbedtls_x509_crt *chain)
{
    size_t depth = 0;

    for (const mbedtls_x509_crt *crt = chain; crt != NULL; crt = crt->next)
    {
        char subj[128];
        char iss[128];
        int subj_len = mbedtls_x509_dn_gets(subj, sizeof(subj), &crt->subject);
        int iss_len = mbedtls_x509_dn_gets(iss, sizeof(iss), &crt->issuer);

        POUCH_LOG_ERR("%s[%zu]: subject=%s issuer=%s",
                      label,
                      depth,
                      subj_len > 0 ? subj : "(empty)",
                      iss_len > 0 ? iss : "(empty)");
        depth++;
    }

    POUCH_LOG_ERR("%s: %zu certificate(s) in chain", label, depth);
}

static void log_verify_failure_details(mbedtls_x509_crt *chain, mbedtls_x509_crt *ca_cert)
{
    char subj[128];
    char iss[128];

    if (ca_cert != NULL)
    {
        int n = mbedtls_x509_dn_gets(subj, sizeof(subj), &ca_cert->subject);
        POUCH_LOG_ERR("Trust anchor subject=%s", n > 0 ? subj : "(empty)");
    }

    if (chain == NULL)
    {
        return;
    }

    int n = mbedtls_x509_dn_gets(subj, sizeof(subj), &chain->subject);
    int m = mbedtls_x509_dn_gets(iss, sizeof(iss), &chain->issuer);
    POUCH_LOG_ERR("Leaf: subject=%s issuer=%s", n > 0 ? subj : "(empty)", m > 0 ? iss : "(empty)");

    if (chain->next == NULL)
    {
        POUCH_LOG_ERR("No intermediate cert in chain (need E1)");
        return;
    }

    n = mbedtls_x509_dn_gets(subj, sizeof(subj), &chain->next->subject);
    m = mbedtls_x509_dn_gets(iss, sizeof(iss), &chain->next->issuer);
    POUCH_LOG_ERR("Intermediate: subject=%s issuer=%s", n > 0 ? subj : "(empty)", m > 0 ? iss : "(empty)");

    uint32_t e1_flags = 0;
    int e1_ret = mbedtls_x509_crt_verify(chain->next, ca_cert, NULL, NULL, &e1_flags, NULL, NULL);

    POUCH_LOG_ERR("Intermediate vs trust anchor only: ret=0x%x flags=0x%" PRIx32,
                  (unsigned) -e1_ret,
                  e1_flags);
}

static void log_verify_flags(uint32_t flags)
{
#if defined(MBEDTLS_X509_REMOVE_INFO)
    POUCH_LOG_ERR("Server cert verify flags: 0x%" PRIx32, flags);
#else
    char info[512];
    int ret = mbedtls_x509_crt_verify_info(info, sizeof(info), "  ! ", flags);

    if (ret > 0)
    {
        POUCH_LOG_ERR("Server cert verify flags:\n%s", info);
    }
    else
    {
        POUCH_LOG_ERR("Server cert verify flags: 0x%" PRIx32, flags);
    }
#endif
}

static int parse_x509_cert(const struct pouch_cert *cert, mbedtls_x509_crt *out)
{
    if (!cert_is_valid(cert))
    {
        return -EINVAL;
    }

    mbedtls_x509_crt_init(out);

    int ret;

    /*
     * mbedtls_x509_crt_parse() expects PEM buffers to include a trailing NUL.
     * The generated pouch_ca_cert bytes are raw file contents, so add one when
     * parsing PEM CA bundles.
     */
    if (cert->size >= 11 && memcmp(cert->buffer, "-----BEGIN ", 11) == 0)
    {
        uint8_t *pem = malloc(cert->size + 1);
        if (pem == NULL)
        {
            POUCH_LOG_ERR("Failed allocating PEM parse buffer");
            return -ENOMEM;
        }

        memcpy(pem, cert->buffer, cert->size);
        pem[cert->size] = '\0';

        ret = mbedtls_x509_crt_parse(out, pem, cert->size + 1);
        free(pem);
    }
    else
    {
        ret = mbedtls_x509_crt_parse(out, cert->buffer, cert->size);
    }

    if (ret != 0)
    {
        POUCH_LOG_ERR("Failed to parse certificate: 0x%x", -ret);
        return -EIO;
    }

    return 0;
}

static mbedtls_x509_crt *load_ca_cert(void)
{
    static mbedtls_x509_crt ca_cert;
    static bool loaded;

    // lazy load the CA cert:
    if (loaded)
    {
        return &ca_cert;
    }

    int err = parse_x509_cert(pouch_ca_cert, &ca_cert);
    if (err)
    {
        return NULL;
    }

    POUCH_LOG_INF("Loaded Pouch CA cert (%zu bytes)", pouch_ca_cert->size);
    log_cert_info("Pouch CA cert", &ca_cert);

    loaded = true;

    return &ca_cert;
}

static int generate_ref(const struct pouch_cert *cert, uint8_t cert_ref[CERT_REF_LEN])
{
    size_t hash_length;
    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256,
                                           cert->buffer,
                                           cert->size,
                                           cert_ref,
                                           CERT_REF_LEN,
                                           &hash_length);
    if (status != PSA_SUCCESS || hash_length != CERT_REF_LEN)
    {
        return -EIO;
    }

    return 0;
}

static int authenticate_server_cert(mbedtls_x509_crt *cert)
{
    mbedtls_x509_crt *ca_cert = load_ca_cert();
    if (ca_cert == NULL)
    {
        POUCH_LOG_ERR("Failed loading server CA cert");
        return -EIO;
    }

    uint32_t flags = 0;
    int ret = mbedtls_x509_crt_verify(cert,
                                      ca_cert,
                                      NULL,
                                      CONFIG_POUCH_SERVER_CERT_CN,
                                      &flags,
                                      NULL,
                                      NULL);
    if (ret != 0)
    {
        POUCH_LOG_ERR("Failed verifying server cert: 0x%" PRIx32 ", %" PRIx32,
                      (uint32_t) -ret,
                      flags);
        log_verify_flags(flags);
        log_verify_failure_details(cert, ca_cert);
        return -EPERM;
    }

    POUCH_LOG_INF("Server cert verified against CN %s", CONFIG_POUCH_SERVER_CERT_CN);

    return 0;
}

static int extract_pubkey(mbedtls_x509_crt *cert, struct pubkey *out)
{
    mbedtls_ecp_keypair *key = mbedtls_pk_ec(cert->pk);
    if (key == NULL)
    {
        POUCH_LOG_ERR("Extract PK: Invalid key type");
        return -EIO;
    }

    int err = mbedtls_ecp_write_public_key(key,
                                           MBEDTLS_ECP_PF_UNCOMPRESSED,
                                           &out->len,
                                           out->data,
                                           sizeof(out->data));
    if (err)
    {
        POUCH_LOG_ERR("Extract PK: write error 0x%x", -err);
        return -EIO;
    }

    return 0;
}

int cert_device_set(const struct pouch_cert *cert)
{
    if (!cert_is_valid(cert))
    {
        return -EINVAL;
    }

    int err = generate_ref(cert, device.ref);
    if (err)
    {
        return err;
    }

    device.certificate = *cert;

    return 0;
}

int cert_server_set(const struct pouch_cert *certbuf)
{
    int err;

    if (!cert_is_valid(certbuf))
    {
        return -EINVAL;
    }

    const bool is_pem = certbuf->size >= 11 && memcmp(certbuf->buffer, "-----BEGIN ", 11) == 0;

    POUCH_LOG_ERR("Received server cert chain (%zu bytes, %s)",
                  certbuf->size,
                  is_pem ? "PEM" : "DER");

    mbedtls_x509_crt cert_chain;
    err = parse_x509_cert(certbuf, &cert_chain);
    if (err)
    {
        POUCH_LOG_ERR("Failed loading server cert");
        goto exit;
    }

    log_cert_info("Received server cert chain", &cert_chain);
    log_chain_summary("Server chain", &cert_chain);

    if (IS_ENABLED(CONFIG_POUCH_VALIDATE_SERVER_CERT))
    {
        err = authenticate_server_cert(&cert_chain);
        if (err)
        {
            goto exit;
        }
    }

    if (cert_chain.serial.len > sizeof(server_cert.serial.data))
    {
        POUCH_LOG_ERR("Unexpected server certificate serial number size: %u",
                      cert_chain.serial.len);
        goto exit;
    }

    err = extract_pubkey(&cert_chain, &server_cert.pubkey);
    if (err)
    {
        goto exit;
    }

    memcpy(server_cert.serial.data, cert_chain.serial.p, cert_chain.serial.len);
    server_cert.serial.len = cert_chain.serial.len;

    POUCH_LOG_DBG("Server key stored");

exit:
    mbedtls_x509_crt_free(&cert_chain);
    return err;
}

const uint8_t *cert_ref_get(void)
{
    if (!cert_is_valid(&device.certificate))
    {
        return NULL;
    }

    return device.ref;
}

void cert_server_key_get(struct pubkey *out)
{
    *out = server_cert.pubkey;
}

bool cert_has_server_info(void)
{
    return server_cert.pubkey.len != 0;
}

/*
 * Transport API
 */

int pouch_server_certificate_set(const struct pouch_cert *cert)
{
    return cert_server_set(cert);
}

ssize_t pouch_server_certificate_serial_get(uint8_t *serial, size_t len)
{
    if (len < server_cert.serial.len)
    {
        return -EINVAL;
    }

    memcpy(serial, server_cert.serial.data, server_cert.serial.len);

    return server_cert.serial.len;
}

int pouch_device_certificate_ref_get(uint8_t *cert_ref, size_t len)
{
    if (!cert_is_valid(&device.certificate) || len > sizeof(device.ref))
    {
        return -EINVAL;
    }

    memcpy(cert_ref, device.ref, len);

    return 0;
}

int pouch_device_certificate_get(struct pouch_cert *out)
{
    if (!cert_is_valid(&device.certificate))
    {
        return -ENOENT;
    }

    *out = device.certificate;

    return 0;
}
