/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Zephyr's MbedTLS configuration mechanism does not provide sufficient control over the available
 * options for us to use it for pouch. To get around this, this file is included as a user config
 * file, which gets included in the MbedTLS configuration mechanism.
 *
 * Keep verify algorithms (ECDSA, ECP, SHA-384) in prj.conf / Kconfig — not here.
 */

/* Parsing certificates, and its required dependencies: */
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
