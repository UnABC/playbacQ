/**
 *
 *  AuthFilter.cc
 *
 */

#include "AuthFilter.h"

using namespace drogon;

void AuthFilter::doFilter(const HttpRequestPtr& req,
    FilterCallback&& fcb,
    FilterChainCallback&& fccb)
{
    std::string userId = req->getHeader("x-forwarded-user");
    if (!userId.empty())
    {
        req->getAttributes()->insert("userId", userId);
        fccb();
        return;
    }
    //Check failed
    auto res = drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(k401Unauthorized);
    res->setBody("Unauthorized: X-Forwarded-User header is missing");

    std::string origin = req->getHeader("Origin");
    if (!origin.empty()) {
        res->addHeader("Access-Control-Allow-Origin", origin);
    } else {
        // 念のためのフォールバック
        auto customConfig = drogon::app().getCustomConfig();
        res->addHeader("Access-Control-Allow-Origin", customConfig["baseUrl"].asString());
    }
    res->addHeader("Access-Control-Allow-Methods", "OPTIONS, GET, POST, PUT, DELETE");
    res->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

    fcb(res);
}
