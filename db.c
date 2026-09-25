#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sqlite3 *db_conn = NULL;
static const char *db_path_global = NULL;

static const char *SCHEMA =
    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT UNIQUE NOT NULL,"
    "  email TEXT UNIQUE NOT NULL,"
    "  password_hash TEXT NOT NULL,"
    "  bio TEXT,"
    "  image TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS articles ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  slug TEXT UNIQUE NOT NULL,"
    "  title TEXT NOT NULL,"
    "  description TEXT,"
    "  body TEXT,"
    "  created_at DATETIME DEFAULT (strftime('%Y-%m-%dT%H:%M:%S.000Z', 'now')),"
    "  updated_at DATETIME DEFAULT (strftime('%Y-%m-%dT%H:%M:%S.000Z', 'now')),"
    "  author_id INTEGER REFERENCES users(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS tags ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT UNIQUE NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS article_tags ("
    "  article_id INTEGER REFERENCES articles(id) ON DELETE CASCADE,"
    "  tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,"
    "  PRIMARY KEY (article_id, tag_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS comments ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  body TEXT NOT NULL,"
    "  created_at DATETIME DEFAULT (strftime('%Y-%m-%dT%H:%M:%S.000Z', 'now')),"
    "  updated_at DATETIME DEFAULT (strftime('%Y-%m-%dT%H:%M:%S.000Z', 'now')),"
    "  author_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
    "  article_id INTEGER REFERENCES articles(id) ON DELETE CASCADE"
    ");"
    "CREATE TABLE IF NOT EXISTS follows ("
    "  follower_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
    "  followed_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
    "  PRIMARY KEY (follower_id, followed_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS favorites ("
    "  user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
    "  article_id INTEGER REFERENCES articles(id) ON DELETE CASCADE,"
    "  PRIMARY KEY (user_id, article_id)"
    ");";

int db_open(const char *path) {
    db_path_global = path;
    sqlite3 *temp_db;
    if (sqlite3_open(path, &temp_db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(temp_db));
        return 1;
    }
    
    char *err_msg = NULL;
    if (sqlite3_exec(temp_db, SCHEMA, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(temp_db);
        return 1;
    }
    
    sqlite3_close(temp_db);
    return 0;
}

void db_worker_init(void) {
    if (db_path_global == NULL) return;
    if (sqlite3_open(db_path_global, &db_conn) != SQLITE_OK) {
        fprintf(stderr, "Worker failed to open db: %s\n", sqlite3_errmsg(db_conn));
        exit(1);
    }
    // Enable WAL mode for better concurrency
    sqlite3_exec(db_conn, "PRAGMA journal_mode=WAL;", 0, 0, 0);
}

void db_close(void) {
    if (db_conn) {
        sqlite3_close(db_conn);
        db_conn = NULL;
    }
}

static void populate_user(sqlite3_stmt *stmt, User *user) {
    user->id = sqlite3_column_int(stmt, 0);
    const char *username = (const char *)sqlite3_column_text(stmt, 1);
    const char *email = (const char *)sqlite3_column_text(stmt, 2);
    const char *password_hash = (const char *)sqlite3_column_text(stmt, 3);
    const char *bio = (const char *)sqlite3_column_text(stmt, 4);
    const char *image = (const char *)sqlite3_column_text(stmt, 5);

    if (username) strncpy(user->username, username, sizeof(user->username) - 1);
    else user->username[0] = '\0';
    user->username[sizeof(user->username) - 1] = '\0';

    if (email) strncpy(user->email, email, sizeof(user->email) - 1);
    else user->email[0] = '\0';
    user->email[sizeof(user->email) - 1] = '\0';

    if (password_hash) strncpy(user->password_hash, password_hash, sizeof(user->password_hash) - 1);
    else user->password_hash[0] = '\0';
    user->password_hash[sizeof(user->password_hash) - 1] = '\0';

    if (bio) strncpy(user->bio, bio, sizeof(user->bio) - 1);
    else user->bio[0] = '\0';
    user->bio[sizeof(user->bio) - 1] = '\0';

    if (image) strncpy(user->image, image, sizeof(user->image) - 1);
    else user->image[0] = '\0';
    user->image[sizeof(user->image) - 1] = '\0';
}

int db_create_user(const char *username, const char *email, const char *password_hash, User *out_user) {
    const char *sql = "INSERT INTO users (username, email, password_hash, bio, image) VALUES (?, ?, ?, '', '')";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, password_hash, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) return -1;
    
    int id = (int)sqlite3_last_insert_rowid(db_conn);
    return db_get_user_by_id(id, out_user);
}

int db_get_user_by_email(const char *email, User *out_user) {
    const char *sql = "SELECT id, username, email, password_hash, bio, image FROM users WHERE email = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        populate_user(stmt, out_user);
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_get_user_by_id(int id, User *out_user) {
    const char *sql = "SELECT id, username, email, password_hash, bio, image FROM users WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        populate_user(stmt, out_user);
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_update_user(int id, const char *email, const char *username, const char *password_hash, const char *image, const char *bio, User *out_user) {
    const char *sql = "UPDATE users SET "
                      "email = COALESCE(?, email), "
                      "username = COALESCE(?, username), "
                      "password_hash = COALESCE(?, password_hash), "
                      "image = COALESCE(?, image), "
                      "bio = COALESCE(?, bio) "
                      "WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    if (email) sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 1);
    if (username) sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 2);
    if (password_hash) sqlite3_bind_text(stmt, 3, password_hash, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 3);
    if (image) sqlite3_bind_text(stmt, 4, image, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 4);
    if (bio) sqlite3_bind_text(stmt, 5, bio, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 5);
    
    sqlite3_bind_int(stmt, 6, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return -1;
    return db_get_user_by_id(id, out_user);
}

static void populate_article(sqlite3_stmt *stmt, Article *article) {
    article->id = sqlite3_column_int(stmt, 0);
    const char *slug = (const char *)sqlite3_column_text(stmt, 1);
    const char *title = (const char *)sqlite3_column_text(stmt, 2);
    const char *desc = (const char *)sqlite3_column_text(stmt, 3);
    const char *body = (const char *)sqlite3_column_text(stmt, 4);
    const char *created = (const char *)sqlite3_column_text(stmt, 5);
    const char *updated = (const char *)sqlite3_column_text(stmt, 6);
    article->author_id = sqlite3_column_int(stmt, 7);

    if (slug) strncpy(article->slug, slug, sizeof(article->slug)-1); else article->slug[0] = '\0';
    if (title) strncpy(article->title, title, sizeof(article->title)-1); else article->title[0] = '\0';
    if (desc) strncpy(article->description, desc, sizeof(article->description)-1); else article->description[0] = '\0';
    if (body) strncpy(article->body, body, sizeof(article->body)-1); else article->body[0] = '\0';
    if (created) strncpy(article->created_at, created, sizeof(article->created_at)-1); else article->created_at[0] = '\0';
    if (updated) strncpy(article->updated_at, updated, sizeof(article->updated_at)-1); else article->updated_at[0] = '\0';
}

int db_create_article(int author_id, const char *title, const char *slug, const char *description, const char *body, Article *out_article) {
    const char *sql = "INSERT INTO articles (slug, title, description, body, author_id) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, slug, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, body, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, author_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) return -1;
    
    return db_get_article_by_slug(slug, out_article);
}

int db_get_article_by_slug(const char *slug, Article *out_article) {
    const char *sql = "SELECT id, slug, title, description, body, created_at, updated_at, author_id FROM articles WHERE slug = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, slug, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        populate_article(stmt, out_article);
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_update_article(const char *slug, const char *title, const char *new_slug, const char *description, const char *body, Article *out_article) {
    const char *sql = "UPDATE articles SET title = COALESCE(?, title), slug = COALESCE(?, slug), description = COALESCE(?, description), body = COALESCE(?, body), updated_at = strftime('%Y-%m-%dT%H:%M:%S.000Z', 'now') WHERE slug = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_slug, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, body, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, slug, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    
    return db_get_article_by_slug(new_slug ? new_slug : slug, out_article);
}
int db_get_articles(const char *tag, const char *author, const char *favorited, int limit, int offset, Article **out_articles, int *out_count, int *out_total) {
    char base_where[512] = "WHERE 1=1 ";
    if (tag && tag[0]) strcat(base_where, "AND a.id IN (SELECT at.article_id FROM article_tags at JOIN tags t ON at.tag_id = t.id WHERE t.name = ?) ");
    if (author && author[0]) strcat(base_where, "AND a.author_id IN (SELECT id FROM users WHERE username = ?) ");
    if (favorited && favorited[0]) strcat(base_where, "AND a.id IN (SELECT f.article_id FROM favorites f JOIN users fu ON f.user_id = fu.id WHERE fu.username = ?) ");
    
    char count_sql[1024];
    snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) FROM articles a %s", base_where);
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db_conn, count_sql, -1, &stmt, NULL);
    int bind_idx = 1;
    if (tag && tag[0]) sqlite3_bind_text(stmt, bind_idx++, tag, -1, SQLITE_STATIC);
    if (author && author[0]) sqlite3_bind_text(stmt, bind_idx++, author, -1, SQLITE_STATIC);
    if (favorited && favorited[0]) sqlite3_bind_text(stmt, bind_idx++, favorited, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) *out_total = sqlite3_column_int(stmt, 0);
    else *out_total = 0;
    sqlite3_finalize(stmt);
    
    char sql[1024];
    snprintf(sql, sizeof(sql), "SELECT id, slug, title, description, body, created_at, updated_at, author_id FROM articles a %s ORDER BY created_at DESC LIMIT ? OFFSET ?", base_where);
    
    sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL);
    bind_idx = 1;
    if (tag && tag[0]) sqlite3_bind_text(stmt, bind_idx++, tag, -1, SQLITE_STATIC);
    if (author && author[0]) sqlite3_bind_text(stmt, bind_idx++, author, -1, SQLITE_STATIC);
    if (favorited && favorited[0]) sqlite3_bind_text(stmt, bind_idx++, favorited, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, bind_idx++, limit);
    sqlite3_bind_int(stmt, bind_idx++, offset);
    
    Article *articles = calloc(limit > 0 ? limit : 20, sizeof(Article));
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        populate_article(stmt, &articles[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    
    *out_articles = articles;
    *out_count = count;
    return 1;
}

int db_get_articles_fast_json(int user_id, int limit, int offset, char **out_json) {
    const char *sql = 
        "SELECT json_object("
        "  'articles', COALESCE(("
        "    SELECT json_group_array("
        "      json_object("
        "        'slug', a.slug,"
        "        'title', a.title,"
        "        'description', a.description,"
        "        'body', a.body,"
        "        'createdAt', a.created_at,"
        "        'updatedAt', a.updated_at,"
        "        'favorited', EXISTS(SELECT 1 FROM favorites f WHERE f.article_id = a.id AND f.user_id = ?1),"
        "        'favoritesCount', (SELECT COUNT(*) FROM favorites f WHERE f.article_id = a.id),"
        "        'author', json_object("
        "          'username', u.username,"
        "          'bio', u.bio,"
        "          'image', u.image,"
        "          'following', EXISTS(SELECT 1 FROM follows fw WHERE fw.follower_id = ?1 AND fw.followed_id = u.id)"
        "        ),"
        "        'tagList', COALESCE(("
        "          SELECT json_group_array(t.name)"
        "          FROM article_tags at"
        "          JOIN tags t ON t.id = at.tag_id"
        "          WHERE at.article_id = a.id"
        "        ), json_array())"
        "      )"
        "    )"
        "    FROM ("
        "      SELECT * FROM articles"
        "      ORDER BY created_at DESC"
        "      LIMIT ?2 OFFSET ?3"
        "    ) a"
        "    JOIN users u ON u.id = a.author_id"
        "  ), json_array()),"
        "  'articlesCount', (SELECT COUNT(*) FROM articles)"
        ");";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, limit);
    sqlite3_bind_int(stmt, 3, offset);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *result = (const char *)sqlite3_column_text(stmt, 0);
        *out_json = strdup(result ? result : "{}");
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_get_feed(int user_id, int limit, int offset, Article **out_articles, int *out_count, int *out_total) {
    const char *count_sql = "SELECT COUNT(*) FROM articles WHERE author_id IN (SELECT followed_id FROM follows WHERE follower_id = ?)";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db_conn, count_sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) *out_total = sqlite3_column_int(stmt, 0);
    else *out_total = 0;
    sqlite3_finalize(stmt);
    
    const char *sql = "SELECT id, slug, title, description, body, created_at, updated_at, author_id FROM articles WHERE author_id IN (SELECT followed_id FROM follows WHERE follower_id = ?) ORDER BY created_at DESC LIMIT ? OFFSET ?";
    sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, limit);
    sqlite3_bind_int(stmt, 3, offset);
    
    Article *articles = calloc(limit > 0 ? limit : 20, sizeof(Article));
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        populate_article(stmt, &articles[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    
    *out_articles = articles;
    *out_count = count;
    return 1;
}

int db_get_tags(char ***out_tags, int *out_count) {
    printf("DEBUG: Entering db_get_tags\n"); fflush(stdout);
    const char *sql = "SELECT name FROM tags";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL);
    printf("DEBUG: sqlite3_prepare_v2 returned %d\n", rc); fflush(stdout);
    
    char **tags = calloc(100, sizeof(char *));
    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < 100) {
        tags[count] = strdup((const char *)sqlite3_column_text(stmt, 0));
        count++;
    }
    printf("DEBUG: sqlite3_step finished with %d, count=%d\n", rc, count); fflush(stdout);
    sqlite3_finalize(stmt);
    
    *out_tags = tags;
    *out_count = count;
    printf("DEBUG: Exiting db_get_tags\n"); fflush(stdout);
    return 1;
}

int db_get_article_tags(int article_id, char ***out_tags, int *out_count) {
    const char *sql = "SELECT t.name FROM tags t JOIN article_tags at ON t.id = at.tag_id WHERE at.article_id = ?";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, article_id);
    
    char **tags = calloc(20, sizeof(char *));
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < 20) {
        tags[count] = strdup((const char *)sqlite3_column_text(stmt, 0));
        count++;
    }
    sqlite3_finalize(stmt);
    
    *out_tags = tags;
    *out_count = count;
    return 1;
}

int db_add_tag_to_article(int article_id, const char *tag) {
    const char *sql1 = "INSERT OR IGNORE INTO tags (name) VALUES (?)";
    sqlite3_stmt *stmt1;
    if (sqlite3_prepare_v2(db_conn, sql1, -1, &stmt1, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt1, 1, tag, -1, SQLITE_STATIC);
        sqlite3_step(stmt1);
        sqlite3_finalize(stmt1);
    }
    
    const char *sql2 = "INSERT OR IGNORE INTO article_tags (article_id, tag_id) VALUES (?, (SELECT id FROM tags WHERE name = ?))";
    sqlite3_stmt *stmt2;
    if (sqlite3_prepare_v2(db_conn, sql2, -1, &stmt2, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt2, 1, article_id);
        sqlite3_bind_text(stmt2, 2, tag, -1, SQLITE_STATIC);
        sqlite3_step(stmt2);
        sqlite3_finalize(stmt2);
    }
    return 0;
}

int db_delete_article(const char *slug) {
    const char *sql = "DELETE FROM articles WHERE slug = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, slug, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 1 : -1;
}

static void populate_comment(sqlite3_stmt *stmt, Comment *comment) {
    comment->id = sqlite3_column_int(stmt, 0);
    const char *body = (const char *)sqlite3_column_text(stmt, 1);
    const char *created = (const char *)sqlite3_column_text(stmt, 2);
    const char *updated = (const char *)sqlite3_column_text(stmt, 3);
    comment->author_id = sqlite3_column_int(stmt, 4);
    comment->article_id = sqlite3_column_int(stmt, 5);

    if (body) strncpy(comment->body, body, sizeof(comment->body)-1); else comment->body[0] = '\0';
    if (created) strncpy(comment->created_at, created, sizeof(comment->created_at)-1); else comment->created_at[0] = '\0';
    if (updated) strncpy(comment->updated_at, updated, sizeof(comment->updated_at)-1); else comment->updated_at[0] = '\0';
}

int db_create_comment(int author_id, int article_id, const char *body, Comment *out_comment) {
    const char *sql = "INSERT INTO comments (body, author_id, article_id) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, body, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, author_id);
    sqlite3_bind_int(stmt, 3, article_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    int id = (int)sqlite3_last_insert_rowid(db_conn);
    return db_get_comment_by_id(id, out_comment);
}

int db_get_comment_by_id(int id, Comment *out_comment) {
    const char *sql = "SELECT id, body, created_at, updated_at, author_id, article_id FROM comments WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        populate_comment(stmt, out_comment);
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_get_comments_by_article(int article_id, Comment **out_comments, int *out_count) {
    const char *count_sql = "SELECT COUNT(*) FROM comments WHERE article_id = ?";
    sqlite3_stmt *cstmt;
    if (sqlite3_prepare_v2(db_conn, count_sql, -1, &cstmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(cstmt, 1, article_id);
    int count = 0;
    if (sqlite3_step(cstmt) == SQLITE_ROW) count = sqlite3_column_int(cstmt, 0);
    sqlite3_finalize(cstmt);

    *out_count = count;
    if (count == 0) {
        *out_comments = NULL;
        return 0;
    }

    *out_comments = malloc(count * sizeof(Comment));
    if (!*out_comments) return -1;

    const char *sql = "SELECT id, body, created_at, updated_at, author_id, article_id FROM comments WHERE article_id = ? ORDER BY created_at DESC";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(*out_comments);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, article_id);
    
    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < count) {
        populate_comment(stmt, &(*out_comments)[i]);
        i++;
    }
    sqlite3_finalize(stmt);
    return 1;
}

int db_delete_comment(int id) {
    const char *sql = "DELETE FROM comments WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 1 : -1;
}

int db_follow_user(int follower_id, int followed_id) {
    const char *sql = "INSERT OR IGNORE INTO follows (follower_id, followed_id) VALUES (?, ?)";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, follower_id);
    sqlite3_bind_int(stmt, 2, followed_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 1;
}

int db_unfollow_user(int follower_id, int followed_id) {
    const char *sql = "DELETE FROM follows WHERE follower_id = ? AND followed_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, follower_id);
    sqlite3_bind_int(stmt, 2, followed_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 1;
}

int db_is_following(int follower_id, int followed_id) {
    const char *sql = "SELECT 1 FROM follows WHERE follower_id = ? AND followed_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, follower_id);
    sqlite3_bind_int(stmt, 2, followed_id);
    int is_following = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return is_following;
}

int db_favorite_article(int user_id, int article_id) {
    const char *sql = "INSERT OR IGNORE INTO favorites (user_id, article_id) VALUES (?, ?)";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, article_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 1;
}

int db_unfavorite_article(int user_id, int article_id) {
    const char *sql = "DELETE FROM favorites WHERE user_id = ? AND article_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, article_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 1;
}

int db_is_favorited(int user_id, int article_id) {
    const char *sql = "SELECT 1 FROM favorites WHERE user_id = ? AND article_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, article_id);
    int is_favorited = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return is_favorited;
}

int db_favorites_count(int article_id) {
    const char *sql = "SELECT COUNT(*) FROM favorites WHERE article_id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, article_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int db_get_user_by_username(const char *username, User *out_user) {
    const char *sql = "SELECT id, username, email, password_hash, bio, image FROM users WHERE username = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db_conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        populate_user(stmt, out_user);
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}
