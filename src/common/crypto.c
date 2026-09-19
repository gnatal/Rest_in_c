#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>

#define SALT_BYTE_LEN 16
#define HASH_BYTE_LEN 32
#define PBKDF2_ITERATIONS 10000

static void bytes_to_hex(const unsigned char *bytes, size_t len, char *out_hex) {
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out_hex[i * 2]     = hex_chars[(bytes[i] >> 4) & 0x0F];
        out_hex[i * 2 + 1] = hex_chars[bytes[i] & 0x0F];
    }
    out_hex[len * 2] = '\0';
}

static int hex_to_bytes(const char *hex, unsigned char *out_bytes, size_t expected_bytes) {
    if (strlen(hex) != expected_bytes * 2) return 0;
    for (size_t i = 0; i < expected_bytes; i++) {
        char buf[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
        char *end = NULL;
        long val = strtol(buf, &end, 16);
        if (end != buf + 2) return 0;
        out_bytes[i] = (unsigned char)val;
    }
    return 1;
}

int crypto_random_hex(char *out_hex, size_t num_bytes) {
    unsigned char *buf = malloc(num_bytes);
    if (!buf) return 0;
    if (RAND_bytes(buf, (int)num_bytes) != 1) {
        free(buf);
        return 0;
    }
    bytes_to_hex(buf, num_bytes, out_hex);
    free(buf);
    return 1;
}

int crypto_hash_password(const char *password, char *out_salt_hex, char *out_hash_hex) {
    if (!password || !out_salt_hex || !out_hash_hex) return 0;

    unsigned char salt[SALT_BYTE_LEN];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        return 0;
    }
    bytes_to_hex(salt, sizeof(salt), out_salt_hex);

    unsigned char hash[HASH_BYTE_LEN];
    if (!PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
                           salt, sizeof(salt),
                           PBKDF2_ITERATIONS,
                           EVP_sha256(),
                           sizeof(hash), hash)) {
        return 0;
    }
    bytes_to_hex(hash, sizeof(hash), out_hash_hex);
    return 1;
}

int crypto_verify_password(const char *password, const char *salt_hex, const char *expected_hash_hex) {
    if (!password || !salt_hex || !expected_hash_hex) return 0;

    unsigned char salt[SALT_BYTE_LEN];
    if (!hex_to_bytes(salt_hex, salt, sizeof(salt))) {
        return 0;
    }

    unsigned char computed_hash[HASH_BYTE_LEN];
    if (!PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
                           salt, sizeof(salt),
                           PBKDF2_ITERATIONS,
                           EVP_sha256(),
                           sizeof(computed_hash), computed_hash)) {
        return 0;
    }

    char computed_hash_hex[HASH_BYTE_LEN * 2 + 1];
    bytes_to_hex(computed_hash, sizeof(computed_hash), computed_hash_hex);

    return (CRYPTO_memcmp(computed_hash_hex, expected_hash_hex, HASH_BYTE_LEN * 2) == 0);
}
