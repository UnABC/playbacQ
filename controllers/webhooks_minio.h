#pragma once

#include <drogon/utils/coroutine.h>
#include <drogon/HttpController.h>
using namespace drogon;

namespace webhooks
{
  class minio : public drogon::HttpController<minio>
  {
  public:
    METHOD_LIST_BEGIN;
    ADD_METHOD_TO(minio::asyncHandleHttpRequest, "/webhooks/minio", Post);
    ADD_METHOD_TO(minio::receiveEncodeResult, "/webhooks/encode_result", Post);
    METHOD_LIST_END;
    drogon::Task<drogon::HttpResponsePtr> asyncHandleHttpRequest(HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> receiveEncodeResult(HttpRequestPtr req);
  };
}
