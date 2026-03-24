#include "api_auth.h"

using namespace api;

drogon::Task<drogon::HttpResponsePtr> auth::login(HttpRequestPtr req) {
    auto redirectUrl = req->getOptionalParameter<std::string>("redirect");
    std::string target = redirectUrl.value_or("https://playbacq.trap.show/");
    auto resp = drogon::HttpResponse::newRedirectionResponse(target);
    co_return resp;
}