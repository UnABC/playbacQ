#pragma once

#include <drogon/HttpController.h>
using namespace drogon;

namespace webhooks
{
class minio : public drogon::HttpController<minio>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(minio::asyncHandleHttpRequest, "/webhooks/minio", Post);
    METHOD_LIST_END
    drogon::Task<drogon::HttpResponsePtr> asyncHandleHttpRequest(const HttpRequestPtr& req);
};
}
