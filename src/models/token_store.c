#include "token_store.h"
#include "common/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define SESSION_TTL_SECONDS (24 * 3600)
#define RESET_TTL_SECONDS (3600)

static SessionToken s_sessions[MAX_SESSIONS];
static size_t s_session_count = 0;
static pthread_mutex_t s_session_mutex = PTHREAD_MUTEX_INITIALIZER;

static PasswordResetToken s_reset_tokens[MAX_RESET_TOKENS];
static size_t s_reset_count = 0;
static pthread_mutex_t s_reset_mutex = PTHREAD_MUTEX_INITIALIZER;

void token_store_init(void) {
    pthread_mutex_lock(&s_session_mutex);
    s_session_count = 0;
    pthread_mutex_unlock(&s_session_mutex);

    pthread_mutex_lock(&s_reset_mutex);
    s_reset_count = 0;
    pthread_mutex_unlock(&s_reset_mutex);
}

void token_store_destroy(void) {
    pthread_mutex_lock(&s_session_mutex);
    s_session_count = 0;
    pthread_mutex_unlock(&s_session_mutex);

    pthread_mutex_lock(&s_reset_mutex);
    s_reset_count = 0;
    pthread_mutex_unlock(&s_reset_mutex);
}

int session_create(int user_id, SessionToken *out_session) {
    pthread_mutex_lock(&s_session_mutex);

    if (s_session_count >= MAX_SESSIONS) {
        /* Evict expired sessions */
        time_t now = time(NULL);
        size_t write_idx = 0;
        for (size_t i = 0; i < s_session_count; i++) {
            if (s_sessions[i].expires_at > now) {
                s_sessions[write_idx++] = s_sessions[i];
            }
        }
        s_session_count = write_idx;

        if (s_session_count >= MAX_SESSIONS) {
            pthread_mutex_unlock(&s_session_mutex);
            return 0;
        }
    }

    SessionToken session;
    memset(&session, 0, sizeof(session));
    session.user_id = user_id;
    session.expires_at = time(NULL) + SESSION_TTL_SECONDS;

    if (!crypto_random_hex(session.token, 32)) {
        pthread_mutex_unlock(&s_session_mutex);
        return 0;
    }

    s_sessions[s_session_count++] = session;

    if (out_session) {
        *out_session = session;
    }

    pthread_mutex_unlock(&s_session_mutex);
    return 1;
}

int session_validate(const char *token, int *out_user_id) {
    if (!token) return 0;

    time_t now = time(NULL);
    pthread_mutex_lock(&s_session_mutex);

    for (size_t i = 0; i < s_session_count; i++) {
        if (strcmp(s_sessions[i].token, token) == 0) {
            if (s_sessions[i].expires_at > now) {
                if (out_user_id) {
                    *out_user_id = s_sessions[i].user_id;
                }
                pthread_mutex_unlock(&s_session_mutex);
                return 1;
            } else {
                /* Expired: remove it */
                s_sessions[i] = s_sessions[--s_session_count];
                pthread_mutex_unlock(&s_session_mutex);
                return 0;
            }
        }
    }

    pthread_mutex_unlock(&s_session_mutex);
    return 0;
}

int session_destroy(const char *token) {
    if (!token) return 0;

    pthread_mutex_lock(&s_session_mutex);
    for (size_t i = 0; i < s_session_count; i++) {
        if (strcmp(s_sessions[i].token, token) == 0) {
            s_sessions[i] = s_sessions[--s_session_count];
            pthread_mutex_unlock(&s_session_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&s_session_mutex);
    return 0;
}

int reset_token_create(int user_id, const char *email, PasswordResetToken *out_token) {
    pthread_mutex_lock(&s_reset_mutex);

    if (s_reset_count >= MAX_RESET_TOKENS) {
        /* Evict expired or used tokens */
        time_t now = time(NULL);
        size_t write_idx = 0;
        for (size_t i = 0; i < s_reset_count; i++) {
            if (!s_reset_tokens[i].used && s_reset_tokens[i].expires_at > now) {
                s_reset_tokens[write_idx++] = s_reset_tokens[i];
            }
        }
        s_reset_count = write_idx;

        if (s_reset_count >= MAX_RESET_TOKENS) {
            pthread_mutex_unlock(&s_reset_mutex);
            return 0;
        }
    }

    PasswordResetToken rt;
    memset(&rt, 0, sizeof(rt));
    rt.user_id = user_id;
    strncpy(rt.email, email ? email : "", sizeof(rt.email) - 1);
    rt.expires_at = time(NULL) + RESET_TTL_SECONDS;
    rt.used = 0;

    if (!crypto_random_hex(rt.token, 32)) {
        pthread_mutex_unlock(&s_reset_mutex);
        return 0;
    }

    s_reset_tokens[s_reset_count++] = rt;

    if (out_token) {
        *out_token = rt;
    }

    pthread_mutex_unlock(&s_reset_mutex);
    return 1;
}

int reset_token_validate_and_consume(const char *token, int *out_user_id) {
    if (!token) return 0;

    time_t now = time(NULL);
    pthread_mutex_lock(&s_reset_mutex);

    for (size_t i = 0; i < s_reset_count; i++) {
        if (strcmp(s_reset_tokens[i].token, token) == 0) {
            if (!s_reset_tokens[i].used && s_reset_tokens[i].expires_at > now) {
                s_reset_tokens[i].used = 1;
                if (out_user_id) {
                    *out_user_id = s_reset_tokens[i].user_id;
                }
                pthread_mutex_unlock(&s_reset_mutex);
                return 1;
            } else {
                pthread_mutex_unlock(&s_reset_mutex);
                return 0;
            }
        }
    }

    pthread_mutex_unlock(&s_reset_mutex);
    return 0;
}
