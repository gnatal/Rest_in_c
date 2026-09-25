#ifndef CRYPTO_H
#define CRYPTO_H

// Hashes a password into out_hash (must be at least 65 bytes: 64 hex chars + null)
void hash_password(const char *password, char *out_hash);

// Generates a JWT for the given user ID and username. 
// Returns a dynamically allocated string (caller must free).
char* jwt_generate(int user_id, const char *username);

// Verifies a JWT and extracts the user ID. 
// Returns 1 if valid, 0 otherwise.
int jwt_verify(const char *token, int *out_user_id);

#endif // CRYPTO_H
