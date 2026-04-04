#pragma once

#include <format>
#include <chrono>
#include <string>
#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace Token {
  std::string generateEmbedToken(const std::string& videoId);
  bool validateToken(const std::string& videoId, const std::string& token);
}
