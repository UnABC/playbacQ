#include "api_auth.h"

using namespace api;

drogon::Task<drogon::HttpResponsePtr> auth::login(HttpRequestPtr req) {
    auto redirectUrl = req->getOptionalParameter<std::string>("redirect");
    std::string target = redirectUrl.value_or("https://playbacq.trap.show/");
    auto resp = drogon::HttpResponse::newRedirectionResponse(target);
    co_return resp;
}

drogon::Task<drogon::HttpResponsePtr> auth::getUser(HttpRequestPtr req) {
    try {
        std::string userId = req->getAttributes()->get<std::string>("userId");
        Json::Value jsonResponse;
        jsonResponse["userId"] = userId;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
        resp->setStatusCode(drogon::HttpStatusCode::k200OK);
        co_return resp;
    }
    catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setBody("Failed to retrieve user: " + std::string(e.what()));
        co_return resp;
    }
}