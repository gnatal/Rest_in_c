#include "controllers_user.h"
#include "db.h"
#include "crypto.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "vendor/yyjson/yyjson.h"

static void send_error(Response *res, int status, const char *msg) {
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);
    
    yyjson_mut_val *err_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, obj, "errors", err_obj);
    
    yyjson_mut_val *body_arr = yyjson_mut_arr(doc);
    yyjson_mut_arr_append(body_arr, yyjson_mut_str(doc, msg));
    yyjson_mut_obj_add_val(doc, err_obj, "body", body_arr);
    
    res_status(res, status);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (json) {
        res_json(res, json);
        free(json);
    } else {
        res_json(res, "{\"errors\":{\"body\":[\"internal error\"]}}");
    }
    yyjson_mut_doc_free(doc);
}

static void send_user_response(Response *res, int status, const User *user) {
    char *token = jwt_generate(user->id, user->username);
    
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_val *user_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "user", user_obj);
    
    yyjson_mut_obj_add_str(doc, user_obj, "email", user->email);
    yyjson_mut_obj_add_str(doc, user_obj, "token", token);
    yyjson_mut_obj_add_str(doc, user_obj, "username", user->username);
    yyjson_mut_obj_add_str(doc, user_obj, "bio", user->bio[0] ? user->bio : "");
    yyjson_mut_obj_add_str(doc, user_obj, "image", user->image[0] ? user->image : "");
    
    res_status(res, status);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (json) {
        res_json(res, json);
        free(json);
    } else {
        send_error(res, 500, "internal error");
    }
    
    free(token);
    yyjson_mut_doc_free(doc);
}

void mw_require_auth(const Request *req, Response *res, MiddlewareChain *chain) {
    (void)res;
    const char *auth = req_get_header(req, "Authorization");
    if (!auth || strncmp(auth, "Token ", 6) != 0) {
        chain_error(chain, 401, "Unauthorized");
        return;
    }
    int user_id = 0;
    if (!jwt_verify(auth + 6, &user_id)) {
        chain_error(chain, 401, "Unauthorized");
        return;
    }
    chain_next(chain);
}

void handler_register_user(const Request *req, Response *res) {
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_doc *doc = yyjson_read_opts((char *)req->body, strlen(req->body), 0, &alc, NULL);
    if (!doc) {
        send_error(res, 422, "invalid json");
        return;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *user_obj = yyjson_obj_get(root, "user");
    if (!user_obj) {
        send_error(res, 422, "missing user object");
        return;
    }
    
    const char *username = yyjson_get_str(yyjson_obj_get(user_obj, "username"));
    const char *email = yyjson_get_str(yyjson_obj_get(user_obj, "email"));
    const char *password = yyjson_get_str(yyjson_obj_get(user_obj, "password"));
    
    if (!username || !email || !password) {
        send_error(res, 422, "missing required fields");
        return;
    }
    
    char hashed_pw[128];
    hash_password(password, hashed_pw);
    
    User u;
    if (db_create_user(username, email, hashed_pw, &u) < 0) {
        send_error(res, 422, "email or username taken");
        return;
    }
    
    send_user_response(res, 201, &u);
}

void handler_login_user(const Request *req, Response *res) {
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_doc *doc = yyjson_read_opts((char *)req->body, strlen(req->body), 0, &alc, NULL);
    if (!doc) {
        send_error(res, 422, "invalid json");
        return;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *user_obj = yyjson_obj_get(root, "user");
    if (!user_obj) {
        send_error(res, 422, "missing user object");
        return;
    }
    
    const char *email = yyjson_get_str(yyjson_obj_get(user_obj, "email"));
    const char *password = yyjson_get_str(yyjson_obj_get(user_obj, "password"));
    
    if (!email || !password) {
        send_error(res, 422, "missing required fields");
        return;
    }
    
    User u;
    if (db_get_user_by_email(email, &u) != 1) {
        send_error(res, 422, "invalid credentials");
        return;
    }
    
    char hashed_pw[128];
    hash_password(password, hashed_pw);
    if (strcmp(hashed_pw, u.password_hash) != 0) {
        send_error(res, 422, "invalid credentials");
        return;
    }
    
    send_user_response(res, 200, &u);
}

void handler_get_current_user(const Request *req, Response *res) {
    const char *auth = req_get_header(req, "Authorization");
    int user_id = 0;
    if (!auth || !jwt_verify(auth + 6, &user_id)) {
        send_error(res, 401, "unauthorized");
        return;
    }
    
    User u;
    if (db_get_user_by_id(user_id, &u) != 1) {
        send_error(res, 404, "user not found");
        return;
    }
    
    send_user_response(res, 200, &u);
}

int get_current_user_id(const Request *req) {
    const char *auth = req_get_header(req, "Authorization");
    int user_id = 0;
    if (auth && strncmp(auth, "Token ", 6) == 0) {
        jwt_verify(auth + 6, &user_id);
    }
    return user_id;
}
