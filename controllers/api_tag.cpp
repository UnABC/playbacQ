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
