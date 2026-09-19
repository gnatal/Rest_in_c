#ifndef REST_TYPES_H
#define REST_TYPES_H

#include <stddef.h>
#include <string.h>
#include <time.h>

#define MAX_USERS 256
#define MAX_SESSIONS 1024
#define MAX_RESET_TOKENS 256
#define MAX_TODOS 1024

#define TOKEN_HEX_LEN 64
#define SALT_HEX_LEN 32
#define HASH_HEX_LEN 64

/* User Roles */
typedef enum {
    ROLE_USER = 0,
    ROLE_ADMIN = 1
} Role;

static inline const char *role_to_string(Role role) {
    switch (role) {
        case ROLE_ADMIN: return "admin";
        case ROLE_USER:
        default: return "user";
    }
}

static inline Role role_from_string(const char *str) {
    if (str != NULL && (strcmp(str, "admin") == 0 || strcmp(str, "ADMIN") == 0)) {
        return ROLE_ADMIN;
    }
    return ROLE_USER;
}

/* User Model */
typedef struct {
    int id;
    char email[128];
    char name[64];
    char password_salt[SALT_HEX_LEN * 2 + 1];
    char password_hash[HASH_HEX_LEN + 1];
    Role role;
    char created_at[32];
} User;

/* Active Session / Auth Token */
typedef struct {
    char token[TOKEN_HEX_LEN + 1];
    int user_id;
    time_t expires_at;
} SessionToken;

/* Password Reset Token */
typedef struct {
    char token[TOKEN_HEX_LEN + 1];
    int user_id;
    char email[128];
    time_t expires_at;
    int used;
} PasswordResetToken;

/* Todo Model */
typedef struct {
    int id;
    int user_id;
    char title[128];
    char description[512];
    int completed;
    char created_at[32];
} Todo;

#endif /* REST_TYPES_H */
