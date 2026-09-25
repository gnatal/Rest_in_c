#include "controllers_profile.h"
#include "controllers_user.h"
#include "db.h"
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
    }
    yyjson_mut_doc_free(doc);
}

static void send_profile_response(Response *res, int status, const User *profile_user, int is_following) {
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_val *profile_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "profile", profile_obj);
    
    yyjson_mut_obj_add_str(doc, profile_obj, "username", profile_user->username);
    yyjson_mut_obj_add_str(doc, profile_obj, "bio", profile_user->bio[0] ? profile_user->bio : "");
    yyjson_mut_obj_add_str(doc, profile_obj, "image", profile_user->image[0] ? profile_user->image : "");
    yyjson_mut_obj_add_bool(doc, profile_obj, "following", is_following);
    
    res_status(res, status);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (json) {
        res_json(res, json);
        free(json);
    } else {
        send_error(res, 500, "internal error");
    }
    yyjson_mut_doc_free(doc);
}

void handler_get_profile(const Request *req, Response *res) {
    const char *username = req_get_param(req, "username");
    if (!username) { send_error(res, 400, "missing username"); return; }
    
    User target;
    if (db_get_user_by_username(username, &target) != 1) {
        send_error(res, 404, "profile not found");
        return;
    }
    
    int current_user_id = get_current_user_id(req);
    int is_following = current_user_id ? db_is_following(current_user_id, target.id) : 0;
    
    send_profile_response(res, 200, &target, is_following);
}

void handler_follow_user(const Request *req, Response *res) {
    int current_user_id = get_current_user_id(req);
    if (!current_user_id) { send_error(res, 401, "unauthorized"); return; }
    
    const char *username = req_get_param(req, "username");
    if (!username) { send_error(res, 400, "missing username"); return; }
    
    User target;
    if (db_get_user_by_username(username, &target) != 1) {
        send_error(res, 404, "profile not found");
        return;
    }
    
    db_follow_user(current_user_id, target.id);
    send_profile_response(res, 200, &target, 1);
}

void handler_unfollow_user(const Request *req, Response *res) {
    int current_user_id = get_current_user_id(req);
    if (!current_user_id) { send_error(res, 401, "unauthorized"); return; }
    
    const char *username = req_get_param(req, "username");
    if (!username) { send_error(res, 400, "missing username"); return; }
    
    User target;
    if (db_get_user_by_username(username, &target) != 1) {
        send_error(res, 404, "profile not found");
        return;
    }
    
    db_unfollow_user(current_user_id, target.id);
    send_profile_response(res, 200, &target, 0);
}
