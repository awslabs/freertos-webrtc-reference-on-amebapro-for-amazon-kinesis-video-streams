/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MBEDTLS_CUSTOM_CONFIG_H
#define MBEDTLS_CUSTOM_CONFIG_H

/* RTL_CRYPTO_FRAGMENT should be 16bytes-aligned */
#if defined(CONFIG_PLATFORM_8735B)
#define RTL_CRYPTO_FRAGMENT               65536 // 64k bytes
#else
#define RTL_CRYPTO_FRAGMENT               15360
#endif

#if defined(CONFIG_PLATFORM_8735B)
#define CONFIG_SSL_RESUME                 1
#endif

#include "mbedtls/mbedtls_config.h"

/* Enable debug functionality */
#define MBEDTLS_DEBUG_C
#define MBEDTLS_DTLS_DEBUG_C

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_ERROR_C

/* Enable file I/O operations for certificate parsing */
#define MBEDTLS_FS_IO
#endif /* MBEDTLS_CUSTOM_CONFIG_H */
