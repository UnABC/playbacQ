#include <drogon/drogon.h>
#include <sw/redis++/redis++.h>
#include <filesystem>

int main() {
    //Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", 8080);
    //Load config file
    drogon::app().loadConfigFile("../config.json");

    drogon::app().getLoop()->runEvery(60.0, []() {
        drogon::async_run([]() -> drogon::Task<void> {
            auto client = drogon::app().getDbClient();
            auto redis = drogon::app().getRedisClient();
            try {
                int cursor = 0;
                do {
                    // 100件ずつ処理
                    auto scan_res = co_await redis->execCommandCoro(
                        "SCAN %lld MATCH pending_views:* COUNT 100", cursor
                    );
                    auto res_array = scan_res.asArray();
                    cursor = std::stoll(res_array[0].asString());
                    auto keys_array = res_array[1].asArray();

                    for (const auto& k_res : keys_array) {
                        std::string key = k_res.asString();
                        auto getdel_res = co_await redis->execCommandCoro("GETDEL %s", key.c_str());

                        if (getdel_res.type() != drogon::nosql::RedisResultType::kNil) {
                            int new_views = std::stoi(getdel_res.asString());
                            if (new_views > 0) {
                                // "pending_views:" を削る
                                std::string video_id = key.substr(14);
                                // DBに書き込む
                                co_await client->execSqlCoro(
                                    "UPDATE videos SET view_count = view_count + ? WHERE video_id = ?",
                                    new_views, video_id
                                );
                            }
                        }
                    }
                } while (cursor != 0);
            }
            catch (const std::exception& e) {
                std::cerr << "Error while updating view counts: " << e.what() << std::endl;
            }
            });
        });

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
    drogon::app().registerPostHandlingAdvice([]([[maybe_unused]] const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp) {
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
