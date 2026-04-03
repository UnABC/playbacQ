#include "share.h"
#include <drogon/orm/Exception.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>
#include <json/json.h>
#include "../models/Videos.h"
#include "../plugins/S3Plugin.h"
#include "api_videos.h"
#include "../plugins/Token.h"

std::vector<std::string> getAllowedBotIps() {
    std::vector<std::string> ips;
    if (const char* envIps = std::getenv("TRUSTED_PROXIES")) {
        auto view = std::string_view(envIps);
        for (const auto& word : std::views::split(view, ',')) {
            ips.emplace_back(word.begin(), word.end());
        }
    }
    return ips;
}

drogon::Task<drogon::HttpResponsePtr> share::shareVideo(HttpRequestPtr req, std::string id) {
    const char* frontendUrl = std::getenv("FRONTEND_URL");
    std::string baseUrl = frontendUrl ? frontendUrl : "http://localhost:4200";

    std::string clientIp = req->getHeader("x-forwarded-for");
    if (clientIp.empty()) {
        clientIp = req->getPeerAddr().toIp();
    } else {
        clientIp = clientIp.substr(0, clientIp.find(','));
    }
    static const std::vector<std::string> trustedProxies = getAllowedBotIps();
    bool isTrustedProxy = std::find(trustedProxies.begin(), trustedProxies.end(), clientIp) != trustedProxies.end();

    std::string userAgent = req->getHeader("user-agent");
    bool isOgpFetcher = userAgent.find("traq-ogp-fetcher-curl-bot") != std::string::npos;
    if (isOgpFetcher && !isTrustedProxy) {
        std::cout << "[[This IP is not trusted]]Blocked OGP fetcher with IP: " << clientIp << std::endl;
    }
    if (isOgpFetcher && isTrustedProxy) {
        std::string title, description;
        try {
            drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
            auto video = co_await mapper.findByPrimaryKey(id);
            title = *video.getTitle();
            description = *video.getDescription();
            title = parseSafeUrl(title);
            description = parseSafeUrl(description);
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
        const char* EMBED_TOKEN_SECRET_KEY = std::getenv("EMBED_TOKEN_SECRET_KEY");
        std::string SEACRET_KEY = EMBED_TOKEN_SECRET_KEY ? EMBED_TOKEN_SECRET_KEY : "default_secret_key";
        std::string Token = Token::generateEmbedToken(id, SEACRET_KEY);
        std::string embedUrl = baseUrl + "/embed/" + id + "?token=" + Token;
        std::string pageUrl = baseUrl + "/watch/" + id;
        std::string thumbnailUrl = baseUrl + "/share/" + id + "/thumbnail";
        std::string ogpHtml = std::format(R"(<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="utf-8">
    <title>{0}</title>
    <meta property="og:title" content="{0}" />
    <meta property="og:type" content="video.other" />
    <meta property="og:url" content="{2}" />
    <meta property="og:image" content="{4}" />
    <meta property="og:description" content="{3}" />

    <meta property="og:video" content="{1}" />
    <meta property="og:video:url" content="{1}" />
    <meta property="og:video:secure_url" content="{1}" />
    <meta property="og:video:type" content="text/html" />
    <meta property="og:video:width" content="1920" />
    <meta property="og:video:height" content="1080" />

    <meta name="twitter:card" content="player" />
    <meta name="twitter:title" content="{0}" />
    <meta name="twitter:image" content="{4}" />
    <meta name="twitter:description" content="{3}" />
    <meta name="twitter:player" content="{1}" />
    <meta name="twitter:player:width" content="1920" />
    <meta name="twitter:player:height" content="1080" />
</head>
<body></body>
</html>)", title, embedUrl, pageUrl, description, thumbnailUrl);

        auto resp = drogon::HttpResponse::newHttpResponse();
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

drogon::Task<drogon::HttpResponsePtr> share::shareThumbnail(HttpRequestPtr req, std::string id) {
    drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
    try {
        // 動画が存在するか確認
        auto video = co_await mapper.findByPrimaryKey(id);

        auto s3Plugin = drogon::app().getPlugin<S3Plugin>();
        std::string presigned_url = s3Plugin->genPresignedGetUrl("hls/" + id + "/thumbnail.jpg", 604800);

        auto resp = drogon::HttpResponse::newRedirectionResponse(presigned_url);
        co_return resp;
    }
    catch (const drogon::orm::UnexpectedRows& e) {
        // Not found 404
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
        resp->setBody("Video not found");
        co_return resp;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to fetch thumbnail from S3: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setBody("Failed to fetch thumbnail from S3: " + std::string(e.what()));
        co_return resp;
    }
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