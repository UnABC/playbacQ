#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class share : public drogon::HttpController<share>
{
public:
  METHOD_LIST_BEGIN;
  ADD_METHOD_TO(share::shareVideo, "/share/{1}", Get, "AuthFilter");
  METHOD_LIST_END;
  drogon::Task<drogon::HttpResponsePtr> shareVideo(HttpRequestPtr req, std::string id);
};
