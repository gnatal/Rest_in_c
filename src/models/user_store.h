#ifndef REST_USER_STORE_H
#define REST_USER_STORE_H

#include "types.h"

/* Initialize user repository and seed initial admin user */
void user_store_init(void);

/* Clean up user repository */
void user_store_destroy(void);

/*
 * Create a new user.
 * Returns 0 on success.
 * Returns -1 if email already registered.
 * Returns -2 if maximum user limit reached.
 * Returns -3 on hashing or validation error.
 */
int user_store_create(const char *email, const char *name, const char *password, Role role, User *out_user);

/* Find user by email (case-insensitive). Returns 1 if found, 0 if not. */
int user_store_find_by_email(const char *email, User *out_user);

/* Find user by ID. Returns 1 if found, 0 if not. */
int user_store_find_by_id(int id, User *out_user);

/* Update user password. Returns 1 on success, 0 if user not found or error. */
int user_store_update_password(int id, const char *new_password);

/* Total count of registered users */
size_t user_store_count(void);

#endif /* REST_USER_STORE_H */
