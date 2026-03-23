#include <drogon/drogon.h>
#include <sw/redis++/redis++.h>
#include <filesystem>

int main() {
    //Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", 8080);
    //Load config file
    // DB情報を取得
    const char* dbUserEnv = std::getenv("NS_MARIADB_USER");
    const char* dbPassEnv = std::getenv("NS_MARIADB_PASSWORD");
    const char* dbHostEnv = std::getenv("NS_MARIADB_HOSTNAME");
    const char* dbPortEnv = std::getenv("NS_MARIADB_PORT");
    const char* frontendUrlEnv = std::getenv("FRONTEND_URL");
    std::string dbUser = dbUserEnv ? dbUserEnv : "DEFAULT_USER";
    std::string dbPass = dbPassEnv ? dbPassEnv : "DEFAULT_PASSWORD";
    std::string dbHost = dbHostEnv ? dbHostEnv : "db";
    std::string dbPort = dbPortEnv ? dbPortEnv : "3306";
    std::string frontendUrl = frontendUrlEnv ? frontendUrlEnv : "http://localhost:4200";
    // Redis情報を取得
    const char* redisHostEnv = std::getenv("REDIS_HOST");
    const char* redisPortEnv = std::getenv("REDIS_PORT");
    const char* redisPassEnv = std::getenv("REDIS_PASSWORD");
    const char* redisUserEnv = std::getenv("REDIS_USER");
    std::string redisHost = redisHostEnv ? redisHostEnv : "redis";
    std::string redisPort = redisPortEnv ? redisPortEnv : "6379";
    std::string redisPass = redisPassEnv ? redisPassEnv : "";
    std::string redisUser = redisUserEnv ? redisUserEnv : "default";
    // DBクライアントの設定
    drogon::orm::MysqlConfig config;
    config.host = dbHost;
    config.port = std::stoi(dbPort);
    config.databaseName = "playbacq";
    config.username = dbUser;
    config.password = dbPass;
    config.connectionNumber = 3;
    config.name = "default";
    config.characterSet = "utf8mb4";
    config.isFast = false;
    config.timeout = 15000; // タイムアウトを15秒に設定
    drogon::app().addDbClient(config);
    // Redisクライアントの設定
    try {
        sw::redis::ConnectionOptions connection_options;
        connection_options.host = redisHost;
        connection_options.port = std::stoi(redisPort);
        connection_options.password = redisPass;
        connection_options.user = redisUser;
        auto redis = sw::redis::Redis(connection_options);
        std::cout << "Connected to Redis successfully." << std::endl;
    }
    catch (const sw::redis::Error& e) {
        std::cerr << "Failed to connect to Redis: " << e.what() << std::endl;
        return 1;
    }
    drogon::app().loadConfigFile("config.json");

    drogon::app().getLoop()->runEvery(60.0, []() {
        drogon::async_run([]() -> drogon::Task<void> {
            auto client = drogon::app().getDbClient();
            auto redis = drogon::app().getRedisClient();
            try {
                long long cursor = 0;
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
    drogon::app().registerPostHandlingAdvice([&frontendUrl]([[maybe_unused]] const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp) {
        // フロントエンドのurlを許可
        resp->addHeader("Access-Control-Allow-Origin", frontendUrl);
        // 許可するHTTPメソッド
        resp->addHeader("Access-Control-Allow-Methods", "OPTIONS, GET, POST, PUT, DELETE");
        // 許可するHTTPヘッダ (フロントエンドからJWTなどを送る場合は Authorization も必要になるかも)
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        });

    drogon::app().run();
    return 0;
}
