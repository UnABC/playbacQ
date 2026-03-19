#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>

int main(int argc, char** argv)
{
    using namespace drogon;

    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    // Start the main loop on another thread
    std::thread thr([&]() {
        // Queues the promise to be fulfilled after starting the loop
        drogon::app().addListener("127.0.0.1", 8080);
        const char* dbUserEnv = std::getenv("DB_USER");
        const char* dbPassEnv = std::getenv("DB_PASSWORD");
        std::string dbUser = dbUserEnv ? dbUserEnv : "DEFAULT_USER";
        std::string dbPass = dbPassEnv ? dbPassEnv : "DEFAULT_PASSWORD";
        drogon::orm::MysqlConfig config;
        config.host = "db";
        config.port = 3306;
        config.databaseName = "playbacq";
        config.username = dbUser;
        config.password = dbPass;
        config.connectionNumber = 3;
        config.name = "default";
        drogon::app().addDbClient(config);
        app().loadConfigFile("config.json");
        app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
        app().run();
        });

    // The future is only satisfied after the event loop started
    f1.get();
    int status = test::run(argc, argv);

    // Ask the event loop to shutdown and wait
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return status;
}
