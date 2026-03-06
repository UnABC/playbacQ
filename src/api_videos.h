#pragma once

#include <drogon/utils/coroutine.h>
#include <drogon/HttpController.h>

using namespace drogon;

namespace api
{
class videos : public drogon::HttpController<videos>
{
  public:
    METHOD_LIST_BEGIN
    // use METHOD_ADD to add your custom processing function here;
    ADD_METHOD_TO(videos::getVideos, "/api/videos", Get); // path is /api/videos
    ADD_METHOD_TO(videos::postVideos, "/api/videos", Post,"AuthFilter"); // path is /api/videos
    ADD_METHOD_TO(videos::getVideo, "/api/videos/{1}", Get); // path is /api/videos/{id}
    // METHOD_ADD(videos::your_method_name, "/{1}/{2}/list", Get); // path is /api/videos/{arg1}/{arg2}/list
    // ADD_METHOD_TO(videos::your_method_name, "/absolute/path/{1}/{2}/list", Get); // path is /absolute/path/{arg1}/{arg2}/list

    METHOD_LIST_END
    // your declaration of processing function maybe like this:
    drogon::Task<drogon::HttpResponsePtr>  getVideos(HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr>  postVideos(HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr>  getVideo(HttpRequestPtr req, std::string id);
    // void your_method_name(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, double p1, int p2) const;
};
}
