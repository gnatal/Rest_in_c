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

#endif // MODELS_H
