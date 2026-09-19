#ifndef REST_CRYPTO_H
#define REST_CRYPTO_H

#include <stddef.h>

/*
 * Generates secure random hex string of 2 * num_bytes length (plus NUL).
 * For a 64-character token, pass num_bytes = 32.
 */
int crypto_random_hex(char *out_hex, size_t num_bytes);

/*
 * Hashes a password using PBKDF2-HMAC-SHA256 with a unique random salt.
 * out_salt_hex must hold at least 33 bytes (16 bytes = 32 hex chars + NUL).
 * out_hash_hex must hold at least 65 bytes (32 bytes = 64 hex chars + NUL).
 * Returns 1 on success, 0 on failure.
 */
int crypto_hash_password(const char *password, char *out_salt_hex, char *out_hash_hex);

/*
 * Verifies a password against the stored salt and PBKDF2-HMAC-SHA256 hash.
 * Returns 1 if valid, 0 if invalid or on error.
 */
int crypto_verify_password(const char *password, const char *salt_hex, const char *expected_hash_hex);

#endif /* REST_CRYPTO_H */
