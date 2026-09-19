#include "user_store.h"
#include "common/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <time.h>

static User s_users[MAX_USERS];
static size_t s_user_count = 0;
static int s_next_user_id = 1;
static pthread_mutex_t s_user_mutex = PTHREAD_MUTEX_INITIALIZER;

static void get_iso_timestamp(char *out_buf, size_t size) {
    time_t now = time(NULL);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    strftime(out_buf, size, "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
}

void user_store_init(void) {
    pthread_mutex_lock(&s_user_mutex);
    s_user_count = 0;
    s_next_user_id = 1;

    /* Seed default admin user */
    User admin;
    memset(&admin, 0, sizeof(admin));
    admin.id = s_next_user_id++;
    strncpy(admin.email, "admin@example.com", sizeof(admin.email) - 1);
    strncpy(admin.name, "System Administrator", sizeof(admin.name) - 1);
    admin.role = ROLE_ADMIN;
    get_iso_timestamp(admin.created_at, sizeof(admin.created_at));

    if (crypto_hash_password("Admin123!", admin.password_salt, admin.password_hash)) {
        s_users[s_user_count++] = admin;
    }
    pthread_mutex_unlock(&s_user_mutex);
}

void user_store_destroy(void) {
    pthread_mutex_lock(&s_user_mutex);
    s_user_count = 0;
    pthread_mutex_unlock(&s_user_mutex);
}

int user_store_create(const char *email, const char *name, const char *password, Role role, User *out_user) {
    if (!email || !password || strlen(email) == 0 || strlen(password) == 0) {
        return -3;
    }

    pthread_mutex_lock(&s_user_mutex);

    if (s_user_count >= MAX_USERS) {
        pthread_mutex_unlock(&s_user_mutex);
        return -2;
    }

    /* Check duplicate email */
    for (size_t i = 0; i < s_user_count; i++) {
        if (strcasecmp(s_users[i].email, email) == 0) {
            pthread_mutex_unlock(&s_user_mutex);
            return -1;
        }
    }

    User user;
    memset(&user, 0, sizeof(user));
    user.id = s_next_user_id++;
    strncpy(user.email, email, sizeof(user.email) - 1);
    strncpy(user.name, name ? name : "", sizeof(user.name) - 1);
    user.role = role;
    get_iso_timestamp(user.created_at, sizeof(user.created_at));

    if (!crypto_hash_password(password, user.password_salt, user.password_hash)) {
        pthread_mutex_unlock(&s_user_mutex);
        return -3;
    }

    s_users[s_user_count++] = user;

    if (out_user) {
        *out_user = user;
    }

    pthread_mutex_unlock(&s_user_mutex);
    return 0;
}

int user_store_find_by_email(const char *email, User *out_user) {
    if (!email) return 0;

    pthread_mutex_lock(&s_user_mutex);
    for (size_t i = 0; i < s_user_count; i++) {
        if (strcasecmp(s_users[i].email, email) == 0) {
            if (out_user) {
                *out_user = s_users[i];
            }
            pthread_mutex_unlock(&s_user_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&s_user_mutex);
    return 0;
}

int user_store_find_by_id(int id, User *out_user) {
    pthread_mutex_lock(&s_user_mutex);
    for (size_t i = 0; i < s_user_count; i++) {
        if (s_users[i].id == id) {
            if (out_user) {
                *out_user = s_users[i];
            }
            pthread_mutex_unlock(&s_user_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&s_user_mutex);
    return 0;
}

int user_store_update_password(int id, const char *new_password) {
    if (!new_password || strlen(new_password) == 0) return 0;

    pthread_mutex_lock(&s_user_mutex);
    for (size_t i = 0; i < s_user_count; i++) {
        if (s_users[i].id == id) {
            char new_salt[SALT_HEX_LEN * 2 + 1];
            char new_hash[HASH_HEX_LEN + 1];
            if (!crypto_hash_password(new_password, new_salt, new_hash)) {
                pthread_mutex_unlock(&s_user_mutex);
                return 0;
            }
            strncpy(s_users[i].password_salt, new_salt, sizeof(s_users[i].password_salt));
            strncpy(s_users[i].password_hash, new_hash, sizeof(s_users[i].password_hash));
            pthread_mutex_unlock(&s_user_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&s_user_mutex);
    return 0;
}

size_t user_store_count(void) {
    pthread_mutex_lock(&s_user_mutex);
    size_t count = s_user_count;
    pthread_mutex_unlock(&s_user_mutex);
    return count;
}
