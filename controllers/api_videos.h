#pragma once

#include <drogon/utils/coroutine.h>
#include <drogon/HttpController.h>
#include <drogon/HttpClient.h>

using namespace drogon;

namespace api
{
  class videos : public drogon::HttpController<videos>
  {
  public:
    METHOD_LIST_BEGIN;
    // use METHOD_ADD to add your custom processing function here;
    ADD_METHOD_TO(videos::getVideos, "/api/videos", Get); // path is /api/videos
    ADD_METHOD_TO(videos::postVideos, "/api/videos", Post, "AuthFilter"); // path is /api/videos
    ADD_METHOD_TO(videos::getVideo, "/api/videos/{1}", Get); // path is /api/videos/{id}
    ADD_METHOD_TO(videos::getVideoProgress, "/api/videos/{1}/progress", Get);
    ADD_METHOD_TO(videos::getVideoPlayM3u8, "/api/videos/{1}/play", Get);
    ADD_METHOD_TO(videos::incrementVideoViews, "/api/videos/{1}/views", Post);
    ADD_METHOD_TO(videos::getVideoTopThumbnail, "/api/videos/{1}/thumbnail", Get);
    ADD_METHOD_TO(videos::getVideoThumbnails, "/api/videos/{1}/thumbnails/{2}", Get);
    ADD_METHOD_TO(videos::getVideoThumbnailVtt, "/api/videos/{1}/vtt", Get);
    ADD_METHOD_TO(videos::getTags, "/api/videos/{1}/tags", Get);
    ADD_METHOD_TO(videos::addTag, "/api/videos/{1}/tags", Post);
    ADD_METHOD_TO(videos::removeTag, "/api/videos/{1}/tags", Delete);
    // METHOD_ADD(videos::your_method_name, "/{1}/{2}/list", Get); // path is /api/videos/{arg1}/{arg2}/list
    METHOD_LIST_END;
    drogon::Task<drogon::HttpResponsePtr>  getVideos(HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr>  postVideos(HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr>  getVideo(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  getVideoProgress(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  getVideoPlayM3u8(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  incrementVideoViews(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  getVideoTopThumbnail(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  getVideoThumbnails(HttpRequestPtr req, std::string id, std::string filename);
    drogon::Task<drogon::HttpResponsePtr>  getVideoThumbnailVtt(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  getTags(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  addTag(HttpRequestPtr req, std::string id);
    drogon::Task<drogon::HttpResponsePtr>  removeTag(HttpRequestPtr req, std::string id);
  };
}
