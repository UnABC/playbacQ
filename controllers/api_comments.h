#pragma once

#include <drogon/utils/coroutine.h>
#include <drogon/HttpController.h>

using namespace drogon;

namespace api
{
class comments : public drogon::HttpController<comments>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(comments::getVideo, "/api/videos/{1}/comments", Get);
    ADD_METHOD_TO(comments::postVideo, "/api/videos/{1}/comments", Post,"AuthFilter");
    METHOD_LIST_END

    drogon::Task<drogon::HttpResponsePtr>  getVideo(HttpRequestPtr req, std::string videoId);
    drogon::Task<drogon::HttpResponsePtr>  postVideo(HttpRequestPtr req, std::string videoId);
};
}
