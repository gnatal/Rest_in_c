#ifndef CONTROLLERS_ARTICLE_H
#define CONTROLLERS_ARTICLE_H

#include "cexpress.h"

void handler_create_article(const Request *req, Response *res);
void handler_get_article(const Request *req, Response *res);
void handler_update_article(const Request *req, Response *res);
void handler_delete_article(const Request *req, Response *res);

void handler_favorite_article(const Request *req, Response *res);
void handler_unfavorite_article(const Request *req, Response *res);

void handler_add_comment(const Request *req, Response *res);
void handler_get_comments(const Request *req, Response *res);
void handler_delete_comment(const Request *req, Response *res);

void handler_get_articles(const Request *req, Response *res);
void handler_get_feed(const Request *req, Response *res);
void handler_get_tags(const Request *req, Response *res);

#endif // CONTROLLERS_ARTICLE_H
