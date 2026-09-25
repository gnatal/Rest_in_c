#ifndef MODELS_H
#define MODELS_H

typedef struct {
    int id;
    char username[256];
    char email[256];
    char bio[1024];
    char image[1024];
    char password_hash[128];
} User;

typedef struct {
    int id;
    char slug[256];
    char title[256];
    char description[1024];
    char body[4096];
    char created_at[64];
    char updated_at[64];
    int author_id;
} Article;

typedef struct {
    int id;
    char body[4096];
    char created_at[64];
    char updated_at[64];
    int author_id;
    int article_id;
} Comment;

#endif // MODELS_H
