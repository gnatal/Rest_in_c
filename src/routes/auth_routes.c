#include "auth_routes.h"
#include "common/json_util.h"
#include "common/crypto.h"
#include "models/types.h"
#include "models/user_store.h"
#include "models/token_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* POST /api/signup */
static void handle_signup(const Request *req, Response *res) {
    if (!require_json_content_type(req, res)) return;

    char err[128];
    JsonValue *root = json_parse(req->body, err, sizeof(err));
    if (!root) {
        send_error(res, 400, err);
        return;
    }

    const char *email_str = json_as_string(json_object_get(root, "email"), "");
    const char *name_str  = json_as_string(json_object_get(root, "name"), "");
    const char *pass_str  = json_as_string(json_object_get(root, "password"), "");
    const char *role_str  = json_as_string(json_object_get(root, "role"), "user");

    char email[128];
    char name[64];
    char pass[128];
    Role role = role_from_string(role_str);

    strncpy(email, email_str, sizeof(email) - 1); email[sizeof(email) - 1] = '\0';
    strncpy(name, name_str, sizeof(name) - 1);   name[sizeof(name) - 1] = '\0';
    strncpy(pass, pass_str, sizeof(pass) - 1);   pass[sizeof(pass) - 1] = '\0';

    json_free(root);

    if (strlen(email) == 0 || strchr(email, '@') == NULL) {
        send_error(res, 400, "valid email address is required");
        return;
    }
    if (strlen(pass) < 6) {
        send_error(res, 400, "password must be at least 6 characters");
        return;
    }

    User user;
    int rc = user_store_create(email, name, pass, role, &user);
    if (rc == -1) {
        send_error(res, 409, "email is already registered");
        return;
    }
    if (rc != 0) {
        send_error(res, 500, "failed to create user account");
        return;
    }

    SessionToken session;
    session_create(user.id, &session);

    CookieOptions opts = {
        .max_age = 86400,
        .path = "/",
        .http_only = 1,
        .same_site = COOKIE_SAMESITE_LAX
    };
    res_set_cookie(res, "session", session.token, &opts);

    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "message");
    jw_string(&w, "user registered successfully");
    jw_key(&w, "token");
    jw_string(&w, session.token);
    jw_key(&w, "user");
    jw_object_begin(&w);
    jw_key(&w, "id");
    jw_int(&w, user.id);
    jw_key(&w, "email");
    jw_string(&w, user.email);
    jw_key(&w, "name");
    jw_string(&w, user.name);
    jw_key(&w, "role");
    jw_string(&w, role_to_string(user.role));
    jw_key(&w, "created_at");
    jw_string(&w, user.created_at);
    jw_object_end(&w);
    jw_object_end(&w);

    send_json(res, 201, &w);
}

/* POST /api/signin */
static void handle_signin(const Request *req, Response *res) {
    if (!require_json_content_type(req, res)) return;

    char err[128];
    JsonValue *root = json_parse(req->body, err, sizeof(err));
    if (!root) {
        send_error(res, 400, err);
        return;
    }

    const char *email_str = json_as_string(json_object_get(root, "email"), "");
    const char *pass_str  = json_as_string(json_object_get(root, "password"), "");

    char email[128];
    char pass[128];
    strncpy(email, email_str, sizeof(email) - 1); email[sizeof(email) - 1] = '\0';
    strncpy(pass, pass_str, sizeof(pass) - 1);   pass[sizeof(pass) - 1] = '\0';

    json_free(root);

    if (strlen(email) == 0 || strlen(pass) == 0) {
        send_error(res, 400, "email and password are required");
        return;
    }

    User user;
    if (!user_store_find_by_email(email, &user) ||
        !crypto_verify_password(pass, user.password_salt, user.password_hash)) {
        send_error(res, 401, "invalid email or password");
        return;
    }

    SessionToken session;
    session_create(user.id, &session);

    CookieOptions opts = {
        .max_age = 86400,
        .path = "/",
        .http_only = 1,
        .same_site = COOKIE_SAMESITE_LAX
    };
    res_set_cookie(res, "session", session.token, &opts);

    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "message");
    jw_string(&w, "signin successful");
    jw_key(&w, "token");
    jw_string(&w, session.token);
    jw_key(&w, "user");
    jw_object_begin(&w);
    jw_key(&w, "id");
    jw_int(&w, user.id);
    jw_key(&w, "email");
    jw_string(&w, user.email);
    jw_key(&w, "name");
    jw_string(&w, user.name);
    jw_key(&w, "role");
    jw_string(&w, role_to_string(user.role));
    jw_object_end(&w);
    jw_object_end(&w);

    send_json(res, 200, &w);
}

/* POST /api/forgot_password */
static void handle_forgot_password(const Request *req, Response *res) {
    if (!require_json_content_type(req, res)) return;

    char err[128];
    JsonValue *root = json_parse(req->body, err, sizeof(err));
    if (!root) {
        send_error(res, 400, err);
        return;
    }

    const char *email_str = json_as_string(json_object_get(root, "email"), "");
    char email[128];
    strncpy(email, email_str, sizeof(email) - 1); email[sizeof(email) - 1] = '\0';
    json_free(root);

    if (strlen(email) == 0) {
        send_error(res, 400, "email is required");
        return;
    }

    User user;
    PasswordResetToken rt;
    memset(&rt, 0, sizeof(rt));
    int has_token = 0;

    if (user_store_find_by_email(email, &user)) {
        if (reset_token_create(user.id, user.email, &rt)) {
            has_token = 1;
        }
    }

    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "message");
    jw_string(&w, "if that email exists, a password reset token has been issued");
    if (has_token) {
        jw_key(&w, "reset_token");
        jw_string(&w, rt.token);
    }
    jw_object_end(&w);

    send_json(res, 200, &w);
}

/* POST /api/reset_password */
static void handle_reset_password(const Request *req, Response *res) {
    if (!require_json_content_type(req, res)) return;

    char err[128];
    JsonValue *root = json_parse(req->body, err, sizeof(err));
    if (!root) {
        send_error(res, 400, err);
        return;
    }

    const char *token_str = json_as_string(json_object_get(root, "token"), "");
    const char *pass_str  = json_as_string(json_object_get(root, "new_password"), "");

    char token[TOKEN_HEX_LEN + 1];
    char new_password[128];
    strncpy(token, token_str, sizeof(token) - 1); token[sizeof(token) - 1] = '\0';
    strncpy(new_password, pass_str, sizeof(new_password) - 1); new_password[sizeof(new_password) - 1] = '\0';

    json_free(root);

    if (strlen(token) == 0 || strlen(new_password) == 0) {
        send_error(res, 400, "token and new_password are required");
        return;
    }
    if (strlen(new_password) < 6) {
        send_error(res, 400, "new_password must be at least 6 characters");
        return;
    }

    int user_id = 0;
    if (!reset_token_validate_and_consume(token, &user_id)) {
        send_error(res, 400, "invalid or expired reset token");
        return;
    }

    if (!user_store_update_password(user_id, new_password)) {
        send_error(res, 500, "failed to update password");
        return;
    }

    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "message");
    jw_string(&w, "password reset successful, you can now sign in with your new password");
    jw_object_end(&w);

    send_json(res, 200, &w);
}

void register_auth_routes(App *app) {
    app_post(app, "/api/signup", handle_signup);
    app_post(app, "/api/signin", handle_signin);
    app_post(app, "/api/forgot_password", handle_forgot_password);
    app_post(app, "/api/reset_password", handle_reset_password);
}
