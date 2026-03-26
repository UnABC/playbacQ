#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>

int main(int argc, char** argv)
{
    using namespace drogon;

    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

#ifndef USE_INTERNAL_S3
    std::cerr << "WARNING: Using external S3 endpoint. Make sure the test MinIO server is running and accessible at " << std::getenv("S3_ENDPOINT") << std::endl;
#endif

    // Start the main loop on another thread
    std::thread thr([&]() {
        // Queues the promise to be fulfilled after starting the loop
        drogon::app().addListener("127.0.0.1", 8080);
        drogon::orm::MysqlConfig config;
        config.host = "test-db";
        config.port = 3306;
        config.databaseName = "playbacq_test";
        config.username = "test_user";
        config.password = "test_pass";
        config.connectionNumber = 3;
        config.name = "default";
        drogon::app().addDbClient(config);
        app().loadConfigFile("config.test.json");
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
