/**
 *
 *  AuthFilter.h
 *
 */

#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/drogon.h>
using namespace drogon;


class AuthFilter : public HttpFilter<AuthFilter>
{
public:
  AuthFilter() {}
  void doFilter(const HttpRequestPtr& req,
    FilterCallback&& fcb,
    FilterChainCallback&& fccb) override;
};

