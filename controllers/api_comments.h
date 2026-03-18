#pragma once

#include <drogon/utils/coroutine.h>
#include <drogon/HttpController.h>

using namespace drogon;

namespace api
{
  class comments : public drogon::HttpController<comments>
  {
  public:
    METHOD_LIST_BEGIN;
    ADD_METHOD_TO(comments::getComments, "/api/videos/{1}/comments", Get);
    ADD_METHOD_TO(comments::postComment, "/api/videos/{1}/comments", Post, "AuthFilter");
    ADD_METHOD_TO(comments::deleteComment, "/api/videos/{1}/comments", Delete, "AuthFilter");
    METHOD_LIST_END;

    drogon::Task<drogon::HttpResponsePtr>  getComments(HttpRequestPtr req, std::string videoId);
    drogon::Task<drogon::HttpResponsePtr>  postComment(HttpRequestPtr req, std::string videoId);
    drogon::Task<drogon::HttpResponsePtr>  deleteComment(HttpRequestPtr req, std::string videoId);
  };
}
