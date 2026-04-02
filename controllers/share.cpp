#include "share.h"
#include <drogon/orm/Exception.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>
#include <json/json.h>
#include "../models/Videos.h"

drogon::Task<drogon::HttpResponsePtr> share::shareVideo(HttpRequestPtr req, std::string id) {
    const char* frontendUrl = std::getenv("FRONTEND_URL");
    std::string baseUrl = frontendUrl ? frontendUrl : "http://localhost:4200";

    std::string userAgent = req->getHeader("user-agent");
    if (userAgent.find("traq-ogp-fetcher-curl-bot") != std::string::npos) {
        std::string title;
        try {
            drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
            auto video = co_await mapper.findByPrimaryKey(id);
            title = *video.getTitle();
            title = parseSafeUrl(title);
        }
        catch (const drogon::orm::UnexpectedRows& e) {
            // Not found 404
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
            resp->setBody("Video not found");
            co_return resp;
        }
        catch (const std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
            resp->setBody("Failed to retrieve video: " + std::string(e.what()));
            co_return resp;
        }


        std::string embedUrl = baseUrl + "/embed/" + id;
        std::string pageUrl = baseUrl + "/watch/" + id;
        std::string ogpHtml = std::format(R"(<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="utf-8">
    <title>{0}</title>
    <meta property="og:title" content="{0}" />
    <meta property="og:type" content="video.other" />
    <meta property="og:url" content="{2}" />

    <meta property="og:video" content="{1}" />
    <meta property="og:video:url" content="{1}" />
    <meta property="og:video:secure_url" content="{1}" />
    <meta property="og:video:type" content="text/html" />
    <meta property="og:video:width" content="1920" />
    <meta property="og:video:height" content="1080" />
</head>
<body></body>
</html>)", title, embedUrl, pageUrl);

        Json::Value jsonResponse;
        jsonResponse["message"] = "Redirecting to video page";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
        resp->setStatusCode(drogon::HttpStatusCode::k200OK);
        resp->setBody(ogpHtml);
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        co_return resp;
    }
    Json::Value jsonResponse;
    jsonResponse["message"] = "Redirecting to video page";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
    resp->setStatusCode(drogon::HttpStatusCode::k302Found);
    resp->addHeader("Location", baseUrl + "/watch/" + id);
    co_return resp;
}

std::string share::parseSafeUrl(const std::string& title) {
    std::string safeTitle;
    for (const char c : title) {
        if (c == '\"') {
            safeTitle += "&quot;";
        } else if (c == '&') {
            safeTitle += "&amp;";
        } else if (c == '\'') {
            safeTitle += "&apos;";
        } else if (c == '<') {
            safeTitle += "&lt;";
        } else if (c == '>') {
            safeTitle += "&gt;";
        } else if (c == ' ') {
            safeTitle += "&nbsp;";
        } else {
            safeTitle += c;
        }
    }
    return safeTitle;
}