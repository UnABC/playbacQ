#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace api
{
  class auth : public drogon::HttpController<auth>
  {
  public:
    METHOD_LIST_BEGIN;
    ADD_METHOD_TO(auth::login, "/api/auth/login", Get);
    ADD_METHOD_TO(auth::getUser, "/api/auth/user", Get, "AuthFilter");
    METHOD_LIST_END;
    drogon::Task<drogon::HttpResponsePtr> login(HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> getUser(HttpRequestPtr req);
  };
}
