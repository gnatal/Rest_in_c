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
