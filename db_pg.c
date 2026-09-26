#include "db.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PGconn *db_conn = NULL;

int db_open(const char *path) {
    (void)path;
    return 0; // Postgres schema created externally
}

void db_worker_init(void) {
    db_conn = PQconnectdb("dbname=realworld user=postgres password=postgres host=localhost");
    if (PQstatus(db_conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(db_conn));
        PQfinish(db_conn);
        exit(1);
    }
}

void db_close(void) {
    if (db_conn) {
        PQfinish(db_conn);
        db_conn = NULL;
    }
}

int db_get_tags(char ***out_tags, int *out_count) {
    const char *sql = "SELECT name FROM tags";
    PGresult *res = PQexec(db_conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    int rows = PQntuples(res);
    int count = rows > 100 ? 100 : rows;
    char **tags = calloc(count, sizeof(char *));
    
    for (int i = 0; i < count; i++) {
        tags[i] = strdup(PQgetvalue(res, i, 0));
    }
    
    *out_tags = tags;
    *out_count = count;
    PQclear(res);
    return 1;
}

int db_get_articles_fast_json(int user_id, int limit, int offset, char **out_json) {
    const char *sql = 
        "SELECT json_build_object("
        "  'articles', COALESCE(("
        "    SELECT json_agg(json_build_object("
        "      'slug', a.slug,"
        "      'title', a.title,"
        "      'description', a.description,"
        "      'body', a.body,"
        "      'createdAt', to_char(a.created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'),"
        "      'updatedAt', to_char(a.updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'),"
        "      'favorited', EXISTS(SELECT 1 FROM favorites f WHERE f.article_id = a.id AND f.user_id = $1),"
        "      'favoritesCount', (SELECT COUNT(*) FROM favorites f WHERE f.article_id = a.id),"
        "      'author', json_build_object("
        "        'username', u.username,"
        "        'bio', COALESCE(u.bio, ''),"
        "        'image', COALESCE(u.image, ''),"
        "        'following', EXISTS(SELECT 1 FROM follows fw WHERE fw.follower_id = $1 AND fw.followed_id = u.id)"
        "      ),"
        "      'tagList', COALESCE(("
        "        SELECT json_agg(t.name)"
        "        FROM article_tags at"
        "        JOIN tags t ON t.id = at.tag_id"
        "        WHERE at.article_id = a.id"
        "      ), '[]'::json)"
        "    ))"
        "    FROM ("
        "      SELECT * FROM articles"
        "      ORDER BY created_at DESC"
        "      LIMIT $2 OFFSET $3"
        "    ) a"
        "    JOIN users u ON u.id = a.author_id"
        "  ), '[]'::json),"
        "  'articlesCount', (SELECT article_count FROM article_stats)" /* trigger-maintained, see counts_pg.sql */
        ")::text;";
    
    char uid_str[32], limit_str[32], offset_str[32];
    snprintf(uid_str, sizeof(uid_str), "%d", user_id);
    snprintf(limit_str, sizeof(limit_str), "%d", limit);
    snprintf(offset_str, sizeof(offset_str), "%d", offset);
    const char *paramValues[3] = {uid_str, limit_str, offset_str};
    
    PGresult *res = PQexecParams(db_conn, sql, 3, NULL, paramValues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Query failed: %s\n", PQerrorMessage(db_conn));
        PQclear(res);
        return -1;
    }
    
    const char *result = PQgetvalue(res, 0, 0);
    *out_json = strdup(result ? result : "{}");
    PQclear(res);
    return 1;
}

// Stubs for the rest to make it compile
int db_create_user(const char *u, const char *e, const char *p, User *o) { (void)u; (void)e; (void)p; (void)o; return -1; }
int db_get_user_by_email(const char *e, User *o) { (void)e; (void)o; return -1; }
int db_get_user_by_username(const char *u, User *o) { (void)u; (void)o; return -1; }
int db_get_user_by_id(int id, User *o) { (void)id; (void)o; return -1; }
int db_update_user(int id, const char *e, const char *u, const char *p, const char *i, const char *b, User *o) { (void)id; (void)e; (void)u; (void)p; (void)i; (void)b; (void)o; return -1; }
int db_create_article(int aid, const char *t, const char *s, const char *d, const char *b, Article *o) { (void)aid; (void)t; (void)s; (void)d; (void)b; (void)o; return -1; }
int db_get_article_by_slug(const char *s, Article *o) { (void)s; (void)o; return -1; }
int db_update_article(const char *s, const char *t, const char *ns, const char *d, const char *b, Article *o) { (void)s; (void)t; (void)ns; (void)d; (void)b; (void)o; return -1; }
int db_delete_article(const char *s) { (void)s; return -1; }
int db_get_articles(const char *tg, const char *a, const char *f, int l, int o, Article **oa, int *oc, int *ot) { (void)tg; (void)a; (void)f; (void)l; (void)o; (void)oa; (void)oc; (void)ot; return -1; }
int db_get_feed(int uid, int l, int o, Article **oa, int *oc, int *ot) { (void)uid; (void)l; (void)o; (void)oa; (void)oc; (void)ot; return -1; }
int db_get_article_tags(int aid, char ***ot, int *oc) { (void)aid; (void)ot; (void)oc; return -1; }
int db_add_tag_to_article(int aid, const char *t) { (void)aid; (void)t; return -1; }
int db_create_comment(int uid, int aid, const char *b, Comment *o) { (void)uid; (void)aid; (void)b; (void)o; return -1; }
int db_get_comments_by_article(int aid, Comment **oc, int *ocnt) { (void)aid; (void)oc; (void)ocnt; return -1; }
int db_get_comment_by_id(int id, Comment *o) { (void)id; (void)o; return -1; }
int db_delete_comment(int id) { (void)id; return -1; }
int db_follow_user(int fid, int foid) { (void)fid; (void)foid; return -1; }
int db_unfollow_user(int fid, int foid) { (void)fid; (void)foid; return -1; }
int db_is_following(int fid, int foid) { (void)fid; (void)foid; return 0; }
int db_favorite_article(int uid, int aid) { (void)uid; (void)aid; return -1; }
int db_unfavorite_article(int uid, int aid) { (void)uid; (void)aid; return -1; }
int db_is_favorited(int uid, int aid) { (void)uid; (void)aid; return 0; }
int db_favorites_count(int aid) { (void)aid; return 0; }
