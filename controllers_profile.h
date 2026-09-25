#ifndef CONTROLLERS_PROFILE_H
#define CONTROLLERS_PROFILE_H

#include "cexpress.h"

void handler_get_profile(const Request *req, Response *res);
void handler_follow_user(const Request *req, Response *res);
void handler_unfollow_user(const Request *req, Response *res);

#endif // CONTROLLERS_PROFILE_H
