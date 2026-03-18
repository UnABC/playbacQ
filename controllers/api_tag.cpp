#include "api_tag.h"
#include "../models/Tags.h"

using namespace api;

drogon::Task<drogon::HttpResponsePtr> tag::getTags(HttpRequestPtr req) {
    if (req->getParameter("query").empty()) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("Query parameter is required");
        co_return resp;
    }
    std::string query = req->getParameter("query");

    drogon::orm::CoroMapper<drogon_model::playbacq::Tags> mapper(drogon::app().getDbClient());
    try {
        auto tags = co_await mapper.findBy(drogon::orm::Criteria(drogon_model::playbacq::Tags::Cols::_name, drogon::orm::CompareOperator::Like, "%" + query + "%"));
        Json::Value jsonResponse(Json::arrayValue);
        for (const auto& tag : tags) {
            jsonResponse.append(tag.toJson());
        }
        co_return drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
    }
    catch (const std::exception& e) {
        std::cerr << "DB Error: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setBody("Failed to retrieve tags: " + std::string(e.what()));
        co_return resp;
    }
}

drogon::Task<drogon::HttpResponsePtr> tag::deleteTag(HttpRequestPtr req) {
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("Invalid JSON format");
        co_return resp;
    }
    if (!jsonPtr->isMember("tag_id") || !(*jsonPtr)["tag_id"].isInt()) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("tag_id is required and must be an integer");
        co_return resp;
    }
    int32_t tagId = (*jsonPtr)["tag_id"].asInt();
    drogon::orm::CoroMapper<drogon_model::playbacq::Tags> mapper(drogon::app().getDbClient());
    try {
        size_t deletedCount = co_await mapper.deleteByPrimaryKey(tagId);
        if (deletedCount == 0) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
            resp->setBody("Tag not found");
            co_return resp;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k200OK);
        resp->setBody("Tag deleted successfully");
        co_return resp;
    }
    catch (const std::exception& e) {
        std::cerr << "DB Error: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setBody("Failed to delete tag: " + std::string(e.what()));
        co_return resp;
    }
}

