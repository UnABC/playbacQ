#include "api_comments.h"
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>
#include <drogon/PubSubService.h>
#include <json/json.h>
#include <string>
#include "../models/Comments.h"
#include "../models/Videos.h"
#include "websocket_comments.h"

using namespace api;

drogon::Task<drogon::HttpResponsePtr> comments::getComments([[maybe_unused]] HttpRequestPtr req, std::string videoId) {
    drogon::orm::CoroMapper<drogon_model::playbacq::Comments> mapper(drogon::app().getDbClient());
    drogon::orm::Criteria criteria;
    criteria = criteria && drogon::orm::Criteria(drogon_model::playbacq::Comments::Cols::_video_id, drogon::orm::CompareOperator::EQ, videoId);
    try {
        auto comments = co_await mapper.findBy(criteria);
        if (comments.empty()) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
            resp->setBody("Video not found");
            co_return resp;
        }
        Json::Value jsonResponse(Json::arrayValue);
        for (const auto& comment : comments) {
            jsonResponse.append(comment.toJson());
        }
        co_return drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
    }
    catch (const std::exception& e) {
        std::cerr << "DB Error: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setBody("Failed to retrieve video: " + std::string(e.what()));
        co_return resp;
    }
}

drogon::Task<drogon::HttpResponsePtr> comments::postComment([[maybe_unused]] HttpRequestPtr req, std::string videoId) {
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("Invalid JSON format");
        co_return resp;
    }
    try {
        drogon_model::playbacq::Comments newComment;
        newComment.setVideoId(videoId);
        newComment.setComment(jsonPtr->get("content", "").asString());
        newComment.setUserId(req->getAttributes()->get<std::string>("userId"));
        newComment.setCreatedAt(trantor::Date::now());
        newComment.setTimestamp(jsonPtr->get("timestamp", 0.0).asDouble());
        newComment.setCommand(jsonPtr->get("command", "").asString());

        drogon::orm::CoroMapper<drogon_model::playbacq::Comments> mapper(drogon::app().getDbClient());
        auto insertedComment = co_await mapper.insert(newComment);
        // Websocketでリアルタイムにコメントを配信
        CommentController::broadcastToRoom(videoId, insertedComment.toJson().toStyledString());

        auto resp = drogon::HttpResponse::newHttpJsonResponse(insertedComment.toJson());
        resp->setStatusCode(drogon::HttpStatusCode::k201Created);
        co_return resp;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setBody("Failed to create comment: " + std::string(e.what()));
        co_return resp;
    }
}

drogon::Task<drogon::HttpResponsePtr> comments::deleteComment(HttpRequestPtr req, std::string videoId) {
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("Invalid JSON format");
        co_return resp;
    }
    try {
        int32_t commentId = jsonPtr->get("comment_id", -1).asInt();
        if (commentId == -1) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
            resp->setBody("comment_id is required");
            co_return resp;
        }
        drogon::orm::CoroMapper<drogon_model::playbacq::Comments> mapper(drogon::app().getDbClient());
        // コメントが存在するか確認
        auto comments = co_await mapper.findBy(drogon::orm::Criteria(drogon_model::playbacq::Comments::Cols::_comment_id, drogon::orm::CompareOperator::EQ, commentId));
        if (comments.empty()) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
            resp->setBody("Comment not found");
            co_return resp;
        }
        // コメントが動画に紐づいているか確認
        if (*comments[0].getVideoId() != videoId) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
            resp->setBody("Comment not found for this video");
            co_return resp;
        }
        // コメントしたユーザーまたは動画の投稿者のみが削除できるか確認
        std::string userId = req->getAttributes()->get<std::string>("userId");
        if (*comments[0].getUserId() != userId) {
            // 動画の投稿者か確認
            drogon::orm::CoroMapper<drogon_model::playbacq::Videos> videoMapper(drogon::app().getDbClient());
            auto videos = co_await videoMapper.findBy(drogon::orm::Criteria(drogon_model::playbacq::Videos::Cols::_video_id, drogon::orm::CompareOperator::EQ, videoId));
            if (videos.empty() || *videos[0].getUserId() != userId) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::HttpStatusCode::k403Forbidden);
                resp->setBody("You do not have permission to delete this comment");
                co_return resp;
            }
        }
        // コメントを削除
        size_t deletedCount = co_await mapper.deleteByPrimaryKey(commentId);
        if (deletedCount == 0) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
            resp->setBody("Failed to delete comment");
            co_return resp;
        }
    }
    catch (const drogon::orm::UnexpectedRows& e) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
        resp->setBody("Comment not found");
        co_return resp;
    }
    catch (const std::exception& e) {
        std::cerr << "DB Error: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setBody("Failed to delete comment: " + std::string(e.what()));
        co_return resp;
    }
}