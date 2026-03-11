#include <drogon/drogon.h>
#include <filesystem>

int main() {
    //Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", 8080);
    //Load config file
    drogon::app().loadConfigFile("../config.json");

    // CORS: Preflight (OPTIONS) リクエストの処理
    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr& req, drogon::FilterCallback&& defer, drogon::FilterChainCallback&& chain) {
        if (req->method() == drogon::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            defer(resp);
            return;
        }
        chain();
        });

    // CORS: すべてのレスポンスにCORSヘッダを付与
    drogon::app().registerPostHandlingAdvice([]([[maybe_unused]]const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp) {
        // フロントエンドのurlを許可
        resp->addHeader("Access-Control-Allow-Origin", "http://localhost:4200");
        // 許可するHTTPメソッド
        resp->addHeader("Access-Control-Allow-Methods", "OPTIONS, GET, POST, PUT, DELETE");
        // 許可するHTTPヘッダ (フロントエンドからJWTなどを送る場合は Authorization も必要になるかも)
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        });

    drogon::app().run();
    return 0;
}
