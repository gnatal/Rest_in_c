#include "controllers_article.h"
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
    if (json) { res_json(res, json); free(json); }
    yyjson_mut_doc_free(doc);
}

static void build_author_json(yyjson_mut_doc *doc, yyjson_mut_val *parent, int author_id, int current_user_id) {
    User author;
    if (db_get_user_by_id(author_id, &author) == 1) {
        int is_following = current_user_id ? db_is_following(current_user_id, author_id) : 0;
        yyjson_mut_val *author_obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_val(doc, parent, "author", author_obj);
        yyjson_mut_obj_add_str(doc, author_obj, "username", author.username);
        yyjson_mut_obj_add_str(doc, author_obj, "bio", author.bio[0] ? author.bio : "");
        yyjson_mut_obj_add_str(doc, author_obj, "image", author.image[0] ? author.image : "");
        yyjson_mut_obj_add_bool(doc, author_obj, "following", is_following);
    }
}

static void send_article_response(Response *res, int status, const Article *article, int current_user_id) {
    int is_favorited = current_user_id ? db_is_favorited(current_user_id, article->id) : 0;
    int favorites_count = db_favorites_count(article->id);

    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_val *article_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "article", article_obj);
    
    yyjson_mut_obj_add_str(doc, article_obj, "slug", article->slug);
    yyjson_mut_obj_add_str(doc, article_obj, "title", article->title);
    yyjson_mut_obj_add_str(doc, article_obj, "description", article->description);
    yyjson_mut_obj_add_str(doc, article_obj, "body", article->body);
    yyjson_mut_obj_add_str(doc, article_obj, "createdAt", article->created_at);
    yyjson_mut_obj_add_str(doc, article_obj, "updatedAt", article->updated_at);
    
    yyjson_mut_val *tag_arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, article_obj, "tagList", tag_arr);
    yyjson_mut_obj_add_bool(doc, article_obj, "favorited", is_favorited);
    yyjson_mut_obj_add_int(doc, article_obj, "favoritesCount", favorites_count);
    
    build_author_json(doc, article_obj, article->author_id, current_user_id);
    
    res_status(res, status);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (json) { res_json(res, json); free(json); }
    yyjson_mut_doc_free(doc);
}

void handler_create_article(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }

    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_doc *doc = yyjson_read_opts((char *)req->body, strlen(req->body), 0, &alc, NULL);
    if (!doc) { send_error(res, 422, "invalid json"); return; }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *art_obj = yyjson_obj_get(root, "article");
    if (!art_obj) { send_error(res, 422, "missing article object"); return; }
    
    const char *title = yyjson_get_str(yyjson_obj_get(art_obj, "title"));
    const char *description = yyjson_get_str(yyjson_obj_get(art_obj, "description"));
    const char *body = yyjson_get_str(yyjson_obj_get(art_obj, "body"));
    
    if (!title || !description || !body) { send_error(res, 422, "missing fields"); return; }
    
    char slug[256];
    snprintf(slug, sizeof(slug), "%.200s-%d", title, rand() % 10000);
    for (int i = 0; slug[i]; i++) { if (slug[i] == ' ') slug[i] = '-'; }

    Article a;
    if (db_create_article(user_id, title, slug, description, body, &a) < 0) {
        send_error(res, 422, "could not create article"); return;
    }
    
    yyjson_val *tagList = yyjson_obj_get(art_obj, "tagList");
    if (tagList && yyjson_is_arr(tagList)) {
        yyjson_val *tag_val;
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(tagList, &iter);
        while ((tag_val = yyjson_arr_iter_next(&iter))) {
            const char *tag_str = yyjson_get_str(tag_val);
            if (tag_str) {
                db_add_tag_to_article(a.id, tag_str);
            }
        }
    }
    
    send_article_response(res, 201, &a, user_id);
}

void handler_get_article(const Request *req, Response *res) {
    const char *slug = req_get_param(req, "slug");
    if (!slug) { send_error(res, 400, "missing slug"); return; }
    
    Article a;
    if (db_get_article_by_slug(slug, &a) != 1) { send_error(res, 404, "article not found"); return; }
    send_article_response(res, 200, &a, get_current_user_id(req));
}

void handler_update_article(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }
    const char *slug = req_get_param(req, "slug");
    if (!slug) { send_error(res, 400, "missing slug"); return; }
    
    Article a;
    if (db_get_article_by_slug(slug, &a) != 1) { send_error(res, 404, "not found"); return; }
    if (a.author_id != user_id) { send_error(res, 403, "forbidden"); return; }

    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_doc *doc = yyjson_read_opts((char *)req->body, strlen(req->body), 0, &alc, NULL);
    if (!doc) { send_error(res, 422, "invalid json"); return; }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *art_obj = yyjson_obj_get(root, "article");
    if (!art_obj) { send_error(res, 422, "missing article object"); return; }
    
    const char *title = yyjson_get_str(yyjson_obj_get(art_obj, "title"));
    const char *description = yyjson_get_str(yyjson_obj_get(art_obj, "description"));
    const char *body = yyjson_get_str(yyjson_obj_get(art_obj, "body"));
    
    if (db_update_article(slug, title, NULL, description, body, &a) < 0) {
        send_error(res, 422, "could not update article"); return;
    }
    send_article_response(res, 200, &a, user_id);
}

void handler_delete_article(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }
    const char *slug = req_get_param(req, "slug");
    if (!slug) { send_error(res, 400, "missing slug"); return; }
    
    Article a;
    if (db_get_article_by_slug(slug, &a) != 1) { send_error(res, 404, "not found"); return; }
    if (a.author_id != user_id) { send_error(res, 403, "forbidden"); return; }
    
    db_delete_article(slug);
    res_status(res, 204); res_send(res, "");
}

// Favorites
void handler_favorite_article(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }
    const char *slug = req_get_param(req, "slug");
    Article a;
    if (db_get_article_by_slug(slug, &a) != 1) { send_error(res, 404, "not found"); return; }
    
    db_favorite_article(user_id, a.id);
    send_article_response(res, 200, &a, user_id);
}

void handler_unfavorite_article(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }
    const char *slug = req_get_param(req, "slug");
    Article a;
    if (db_get_article_by_slug(slug, &a) != 1) { send_error(res, 404, "not found"); return; }
    
    db_unfavorite_article(user_id, a.id);
    send_article_response(res, 200, &a, user_id);
}

// Comments
static void build_single_comment_json(yyjson_mut_doc *doc, yyjson_mut_val *parent, Comment *comment, int current_user_id) {
    yyjson_mut_val *c_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, parent, "comment", c_obj);
    yyjson_mut_obj_add_int(doc, c_obj, "id", comment->id);
    yyjson_mut_obj_add_str(doc, c_obj, "createdAt", comment->created_at);
    yyjson_mut_obj_add_str(doc, c_obj, "updatedAt", comment->updated_at);
    yyjson_mut_obj_add_str(doc, c_obj, "body", comment->body);
    build_author_json(doc, c_obj, comment->author_id, current_user_id);
}

void handler_add_comment(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }
    const char *slug = req_get_param(req, "slug");
    Article a;
    if (db_get_article_by_slug(slug, &a) != 1) { send_error(res, 404, "article not found"); return; }

    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_doc *doc = yyjson_read_opts((char *)req->body, strlen(req->body), 0, &alc, NULL);
    if (!doc) { send_error(res, 422, "invalid json"); return; }
    yyjson_val *c_obj = yyjson_obj_get(yyjson_doc_get_root(doc), "comment");
    if (!c_obj) { send_error(res, 422, "missing comment"); return; }
    
    const char *body = yyjson_get_str(yyjson_obj_get(c_obj, "body"));
    if (!body) { send_error(res, 422, "missing body"); return; }
    
    Comment comment;
    if (db_create_comment(user_id, a.id, body, &comment) < 0) {
        send_error(res, 500, "failed to create comment"); return;
    }
    
    yyjson_mut_doc *mut_doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(mut_doc);
    yyjson_mut_doc_set_root(mut_doc, root);
    build_single_comment_json(mut_doc, root, &comment, user_id);
    
    res_status(res, 201);
    size_t len;
    char *json = yyjson_mut_write(mut_doc, 0, &len);
    if (json) { res_json(res, json); free(json); }
    yyjson_mut_doc_free(mut_doc);
}

void handler_get_comments(const Request *req, Response *res) {
    const char *slug = req_get_param(req, "slug");
    Article a;
    if (db_get_article_by_slug(slug, &a) != 1) { send_error(res, 404, "article not found"); return; }
    
    Comment *comments = NULL;
    int count = 0;
    if (db_get_comments_by_article(a.id, &comments, &count) < 0) {
        send_error(res, 500, "db error"); return;
    }
    
    int current_user_id = get_current_user_id(req);
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "comments", arr);
    
    for (int i = 0; i < count; i++) {
        yyjson_mut_val *c_obj = yyjson_mut_obj(doc);
        yyjson_mut_arr_append(arr, c_obj);
        yyjson_mut_obj_add_int(doc, c_obj, "id", comments[i].id);
        yyjson_mut_obj_add_str(doc, c_obj, "createdAt", comments[i].created_at);
        yyjson_mut_obj_add_str(doc, c_obj, "updatedAt", comments[i].updated_at);
        yyjson_mut_obj_add_str(doc, c_obj, "body", comments[i].body);
        build_author_json(doc, c_obj, comments[i].author_id, current_user_id);
    }
    
    res_status(res, 200);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (comments) free(comments);
    
    if (json) { res_json(res, json); free(json); }
    yyjson_mut_doc_free(doc);
}

void handler_get_articles(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    
    const char *tag = req_get_query(req, "tag");
    const char *author = req_get_query(req, "author");
    const char *favorited = req_get_query(req, "favorited");
    const char *limit_str = req_get_query(req, "limit");
    const char *offset_str = req_get_query(req, "offset");
    
    int limit = limit_str ? atoi(limit_str) : 20;
    int offset = offset_str ? atoi(offset_str) : 0;
    
    Article *articles = NULL;
    int count = 0, total = 0;
    db_get_articles(tag, author, favorited, limit, offset, &articles, &count, &total);
    
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "articles", arr);
    yyjson_mut_obj_add_int(doc, root, "articlesCount", total);
    
    for (int i = 0; i < count; i++) {
        yyjson_mut_val *art_obj = yyjson_mut_obj(doc);
        yyjson_mut_arr_append(arr, art_obj);
        
        yyjson_mut_obj_add_str(doc, art_obj, "slug", articles[i].slug);
        yyjson_mut_obj_add_str(doc, art_obj, "title", articles[i].title);
        yyjson_mut_obj_add_str(doc, art_obj, "description", articles[i].description);
        yyjson_mut_obj_add_str(doc, art_obj, "body", articles[i].body);
        yyjson_mut_obj_add_str(doc, art_obj, "createdAt", articles[i].created_at);
        yyjson_mut_obj_add_str(doc, art_obj, "updatedAt", articles[i].updated_at);
        
        yyjson_mut_val *tag_arr = yyjson_mut_arr(doc);
        yyjson_mut_obj_add_val(doc, art_obj, "tagList", tag_arr);
        
        char **tags = NULL;
        int tags_count = 0;
        db_get_article_tags(articles[i].id, &tags, &tags_count);
        for (int j = 0; j < tags_count; j++) {
            yyjson_mut_arr_append(tag_arr, yyjson_mut_str(doc, tags[j]));
            free(tags[j]);
        }
        if (tags) free(tags);
        
        int is_favorited = user_id ? db_is_favorited(user_id, articles[i].id) : 0;
        yyjson_mut_obj_add_bool(doc, art_obj, "favorited", is_favorited);
        yyjson_mut_obj_add_int(doc, art_obj, "favoritesCount", db_favorites_count(articles[i].id));
        
        build_author_json(doc, art_obj, articles[i].author_id, user_id);
    }
    
    if (articles) free(articles);
    
    res_status(res, 200);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (json) { res_json(res, json); free(json); }
    yyjson_mut_doc_free(doc);
}

void handler_get_feed(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }
    
    const char *limit_str = req_get_query(req, "limit");
    const char *offset_str = req_get_query(req, "offset");
    
    int limit = limit_str ? atoi(limit_str) : 20;
    int offset = offset_str ? atoi(offset_str) : 0;
    
    Article *articles = NULL;
    int count = 0, total = 0;
    db_get_feed(user_id, limit, offset, &articles, &count, &total);
    
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "articles", arr);
    yyjson_mut_obj_add_int(doc, root, "articlesCount", total);
    
    for (int i = 0; i < count; i++) {
        yyjson_mut_val *art_obj = yyjson_mut_obj(doc);
        yyjson_mut_arr_append(arr, art_obj);
        
        yyjson_mut_obj_add_str(doc, art_obj, "slug", articles[i].slug);
        yyjson_mut_obj_add_str(doc, art_obj, "title", articles[i].title);
        yyjson_mut_obj_add_str(doc, art_obj, "description", articles[i].description);
        yyjson_mut_obj_add_str(doc, art_obj, "body", articles[i].body);
        yyjson_mut_obj_add_str(doc, art_obj, "createdAt", articles[i].created_at);
        yyjson_mut_obj_add_str(doc, art_obj, "updatedAt", articles[i].updated_at);
        
        yyjson_mut_val *tag_arr = yyjson_mut_arr(doc);
        yyjson_mut_obj_add_val(doc, art_obj, "tagList", tag_arr);
        
        char **tags = NULL;
        int tags_count = 0;
        db_get_article_tags(articles[i].id, &tags, &tags_count);
        for (int j = 0; j < tags_count; j++) {
            yyjson_mut_arr_append(tag_arr, yyjson_mut_str(doc, tags[j]));
            free(tags[j]);
        }
        if (tags) free(tags);
        
        int is_favorited = db_is_favorited(user_id, articles[i].id);
        yyjson_mut_obj_add_bool(doc, art_obj, "favorited", is_favorited);
        yyjson_mut_obj_add_int(doc, art_obj, "favoritesCount", db_favorites_count(articles[i].id));
        
        build_author_json(doc, art_obj, articles[i].author_id, user_id);
    }
    
    if (articles) free(articles);
    
    res_status(res, 200);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (json) { res_json(res, json); free(json); }
    yyjson_mut_doc_free(doc);
}

void handler_get_tags(const Request *req, Response *res) {
    (void)req;
    char **tags = NULL;
    int tags_count = 0;
    db_get_tags(&tags, &tags_count);
    
    yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "tags", arr);
    
    for (int i = 0; i < tags_count; i++) {
        yyjson_mut_arr_append(arr, yyjson_mut_str(doc, tags[i]));
        free(tags[i]);
    }
    if (tags) free(tags);
    
    res_status(res, 200);
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (json) { res_json(res, json); free(json); }
    yyjson_mut_doc_free(doc);
}

void handler_delete_comment(const Request *req, Response *res) {
    int user_id = get_current_user_id(req);
    if (!user_id) { send_error(res, 401, "unauthorized"); return; }
    const char *id_str = req_get_param(req, "id");
    if (!id_str) { send_error(res, 400, "missing id"); return; }
    int comment_id = atoi(id_str);
    
    Comment c;
    if (db_get_comment_by_id(comment_id, &c) != 1) {
        send_error(res, 404, "not found"); return;
    }
    if (c.author_id != user_id) {
        send_error(res, 403, "forbidden"); return;
    }
    
    db_delete_comment(comment_id);
    res_status(res, 204); res_send(res, "");
}
