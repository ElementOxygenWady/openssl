/*
 * Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/quic.h>

#include "helpers/ssltestlib.h"
#include "testutil.h"
#include "testutil/output.h"

static OSSL_LIB_CTX *libctx = NULL;
static OSSL_PROVIDER *defctxnull = NULL;

static int is_fips = 0;

#if 0
/* TODO(QUIC): Temporarily disabled during front-end I/O API finalization. */

/*
 * Test that we read what we've written.
 */
static int test_quic_write_read(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientquic = NULL, *serverquic = NULL;
    int j, ret = 0;
    char buf[20];
    static char *msg = "A test message";
    size_t msglen = strlen(msg);
    size_t numbytes = 0;

    if (!TEST_true(create_ssl_ctx_pair(libctx, OSSL_QUIC_server_method(),
                                       OSSL_QUIC_client_method(),
                                       0,
                                       0,
                                       &sctx, &cctx, NULL, NULL))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverquic, &clientquic,
                                             NULL, NULL))
            || !TEST_true(create_bare_ssl_connection(serverquic, clientquic,
                                                     SSL_ERROR_NONE, 0, 0)))
        goto end;

    for (j = 0; j < 2; j++) {
        /* Check that sending and receiving app data is ok */
        if (!TEST_true(SSL_write_ex(clientquic, msg, msglen, &numbytes))
                || !TEST_true(SSL_read_ex(serverquic, buf, sizeof(buf),
                                          &numbytes))
                || !TEST_mem_eq(buf, numbytes, msg, msglen))
            goto end;

        if (!TEST_true(SSL_write_ex(serverquic, msg, msglen, &numbytes))
                || !TEST_true(SSL_read_ex(clientquic, buf, sizeof(buf),
                                          &numbytes))
                || !TEST_mem_eq(buf, numbytes, msg, msglen))
            goto end;
    }

    ret = 1;

 end:
    SSL_free(serverquic);
    SSL_free(clientquic);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return ret;
}
#endif

/* Test that a vanilla QUIC SSL object has the expected ciphersuites available */
static int test_ciphersuites(void)
{
    SSL_CTX *ctx = SSL_CTX_new_ex(libctx, NULL, OSSL_QUIC_client_method());
    SSL *ssl;
    int testresult = 0;
    const STACK_OF(SSL_CIPHER) *ciphers = NULL;
    const SSL_CIPHER *cipher;
    /* We expect this exact list of ciphersuites by default */
    int cipherids[] = {
        TLS1_3_CK_AES_256_GCM_SHA384,
#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
        TLS1_3_CK_CHACHA20_POLY1305_SHA256,
#endif
        TLS1_3_CK_AES_128_GCM_SHA256
    };
    size_t i, j;

    if (!TEST_ptr(ctx))
        return 0;

    ssl = SSL_new(ctx);
    if (!TEST_ptr(ssl))
        goto err;

    ciphers = SSL_get_ciphers(ssl);

    for (i = 0, j = 0; i < OSSL_NELEM(cipherids); i++) {
        if (cipherids[i] == TLS1_3_CK_CHACHA20_POLY1305_SHA256 && is_fips)
            continue;
        cipher = sk_SSL_CIPHER_value(ciphers, j++);
        if (!TEST_ptr(cipher))
            goto err;
        if (!TEST_uint_eq(SSL_CIPHER_get_id(cipher), cipherids[i]))
            goto err;
    }

    /* We should have checked all the ciphers in the stack */
    if (!TEST_int_eq(sk_SSL_CIPHER_num(ciphers), j))
        goto err;

    testresult = 1;
 err:
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return testresult;
}

OPT_TEST_DECLARE_USAGE("provider config\n")

int setup_tests(void)
{
    char *modulename;
    char *configfile;

    libctx = OSSL_LIB_CTX_new();
    if (!TEST_ptr(libctx))
        return 0;

    defctxnull = OSSL_PROVIDER_load(NULL, "null");

    /*
     * Verify that the default and fips providers in the default libctx are not
     * available
     */
    if (!TEST_false(OSSL_PROVIDER_available(NULL, "default"))
            || !TEST_false(OSSL_PROVIDER_available(NULL, "fips")))
        return 0;

    if (!test_skip_common_options()) {
        TEST_error("Error parsing test options\n");
        return 0;
    }

    if (!TEST_ptr(modulename = test_get_argument(0))
            || !TEST_ptr(configfile = test_get_argument(1)))
        return 0;

    if (!TEST_true(OSSL_LIB_CTX_load_config(libctx, configfile)))
        return 0;

    /* Check we have the expected provider available */
    if (!TEST_true(OSSL_PROVIDER_available(libctx, modulename)))
        return 0;

    /* Check the default provider is not available */
    if (strcmp(modulename, "default") != 0
            && !TEST_false(OSSL_PROVIDER_available(libctx, "default")))
        return 0;

    if (strcmp(modulename, "fips") == 0)
        is_fips = 1;

    /* TODO(QUIC): Temporarily disabled during front-end I/O API finalization. */
#if 0
    ADD_TEST(test_quic_write_read);
#endif
    ADD_TEST(test_ciphersuites);

    return 1;
}

void cleanup_tests(void)
{
    OSSL_PROVIDER_unload(defctxnull);
    OSSL_LIB_CTX_free(libctx);
}
