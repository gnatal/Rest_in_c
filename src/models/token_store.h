#ifndef REST_TOKEN_STORE_H
#define REST_TOKEN_STORE_H

#include "types.h"

/* Initialize token repositories */
void token_store_init(void);

/* Destroy token repositories */
void token_store_destroy(void);

/* Create an active session token for user (valid for 24 hours) */
int session_create(int user_id, SessionToken *out_session);

/* Validate a session token. Returns 1 if valid, 0 if invalid or expired. */
int session_validate(const char *token, int *out_user_id);

/* Revoke a session token */
int session_destroy(const char *token);

/* Create a password reset token for user (valid for 1 hour) */
int reset_token_create(int user_id, const char *email, PasswordResetToken *out_token);

/* Validate and mark a password reset token as consumed. Returns 1 if valid, 0 if invalid/used. */
int reset_token_validate_and_consume(const char *token, int *out_user_id);

#endif /* REST_TOKEN_STORE_H */
