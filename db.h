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
int db_get_user_by_username(const char *username, User *out_user);
int db_get_user_by_id(int id, User *out_user);

int db_create_article(int author_id, const char *title, const char *slug, const char *description, const char *body, Article *out_article);
int db_get_article_by_slug(const char *slug, Article *out_article);
int db_update_article(const char *slug, const char *title, const char *new_slug, const char *description, const char *body, Article *out_article);
int db_delete_article(const char *slug);

int db_create_comment(int author_id, int article_id, const char *body, Comment *out_comment);
int db_get_comments_by_article(int article_id, Comment **out_comments, int *out_count);
int db_get_comment_by_id(int id, Comment *out_comment);
int db_delete_comment(int id);

int db_follow_user(int follower_id, int followed_id);
int db_unfollow_user(int follower_id, int followed_id);
int db_is_following(int follower_id, int followed_id);

int db_favorite_article(int user_id, int article_id);
int db_unfavorite_article(int user_id, int article_id);
int db_is_favorited(int user_id, int article_id);
int db_favorites_count(int article_id);

extern sqlite3 *db_conn;

#endif // DB_H
