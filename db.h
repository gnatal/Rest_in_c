#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include "models.h"

extern sqlite3 *db_conn;

int db_open(const char *path);
void db_close(void);
void db_worker_init(void);

int db_create_user(const char *username, const char *email, const char *password_hash, User *out_user);
int db_get_user_by_email(const char *email, User *out_user);
int db_get_user_by_id(int id, User *out_user);

#endif // DB_H
