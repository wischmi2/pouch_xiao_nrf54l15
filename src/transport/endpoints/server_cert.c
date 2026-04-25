/*
 * Copyright (c) 2026 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <pouch/types.h>
#include <pouch/port.h>
#include <pouch/certificate.h>
#include <pouch/transport/certificate.h>
#include "endpoints.h"

POUCH_LOG_REGISTER(server_cert_endpoint, CONFIG_POUCH_COMMON_LOG_LEVEL);

static struct
{
    uint8_t *buffer;
    size_t size;
} cert;

static int start(void)
{
    cert.buffer = malloc(CONFIG_POUCH_SERVER_CERT_MAX_LEN);
    if (cert.buffer == NULL)
    {
        return -ENOMEM;
    }
    cert.size = 0;

    POUCH_LOG_INF("Starting server cert receive");

    return 0;
}

static int recv(const void *buf, size_t len)
{
    if (cert.size + len > CONFIG_POUCH_SERVER_CERT_MAX_LEN)
    {
        // too large
        POUCH_LOG_ERR("Server cert too large: have %zu, chunk %zu, max %d",
                      cert.size,
                      len,
                      CONFIG_POUCH_SERVER_CERT_MAX_LEN);
        return -EINVAL;
    }

    memcpy(&cert.buffer[cert.size], buf, len);
    cert.size += len;

    return 0;
}

static void end(bool success)
{
    POUCH_LOG_INF("Finished server cert receive: success=%d, size=%zu", (int) success, cert.size);

    if (success)
    {
        struct pouch_cert server_cert = {
            .buffer = cert.buffer,
            .size = cert.size,
        };
        pouch_server_certificate_set(&server_cert);
    }

    free(cert.buffer);
    cert.buffer = NULL;
}

const struct pouch_endpoint pouch_endpoint_server_cert = {
    .start = start,
    .recv = recv,
    .end = end,
};
