#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

using namespace drogon;

namespace api
{
class tag : public drogon::HttpController<tag>
{
  public:
    METHOD_LIST_BEGIN;
    ADD_METHOD_TO(tag::getTags, "/api/tag", Get); 
    METHOD_LIST_END;
    drogon::Task<drogon::HttpResponsePtr>  getTags(HttpRequestPtr req);
};
}
