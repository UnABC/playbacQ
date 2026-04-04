/**
 *
 *  AuthFilter.cc
 *
 */

#include "AuthFilter.h"
#include "../plugins/Token.h"

using namespace drogon;

void AuthFilter::doFilter(const HttpRequestPtr& req,
    FilterCallback&& fcb,
    FilterChainCallback&& fccb)
{
    if (req->path().starts_with("/unauthApi/embed/")) {
        auto token = req->getOptionalParameter<std::string>("token").value_or("");
        std::string videoId = req->path().substr(req->path().find_last_of('/') + 1);
        if (videoId.find("/comments") != std::string::npos) {
            videoId = videoId.substr(0, videoId.find("/comments"));
        }
        if (Token::validateToken(videoId, token)) {
            fccb();
            return;
        }
        auto res = drogon::HttpResponse::newHttpResponse();
        res->setStatusCode(k403Forbidden);
        res->setBody("Forbidden: Invalid token for embed URL");
        res->addHeader("Access-Control-Allow-Methods", "OPTIONS, GET, POST, PUT, DELETE");
        res->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res->addHeader("Access-Control-Allow-Credentials", "true");
        fcb(res);
        return;
    }

    if (std::string userAgent = req->getHeader("user-agent");
        userAgent.find("traq-ogp-fetcher-curl-bot") != std::string::npos) {
        fccb();
        return;
    }
    if (req->path().find("/share/") != std::string::npos) {
        fccb();
        return;
    }
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

    if (std::string origin = req->getHeader("Origin");!origin.empty()) {
        res->addHeader("Access-Control-Allow-Origin", origin);
    } else {
        // 念のためのフォールバック
        auto customConfig = drogon::app().getCustomConfig();
        res->addHeader("Access-Control-Allow-Origin", customConfig["baseUrl"].asString());
    }
    res->addHeader("Access-Control-Allow-Methods", "OPTIONS, GET, POST, PUT, DELETE");
    res->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res->addHeader("Access-Control-Allow-Credentials", "true");

    fcb(res);
}
