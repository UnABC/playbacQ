/**
 *
 *  AuthFilter.cc
 *
 */

#include "AuthFilter.h"

using namespace drogon;

void AuthFilter::doFilter(const HttpRequestPtr &req,
                         FilterCallback &&fcb,
                         FilterChainCallback &&fccb)
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
    fcb(res);
}
