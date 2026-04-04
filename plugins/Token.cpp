#include "Token.h"

namespace Token {

	std::string generateEmbedToken(const std::string& videoId)
	{
		auto expiry = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(24));
		std::string message = std::format("{0}:{1}", videoId, expiry);

		unsigned char hash[EVP_MAX_MD_SIZE];
		unsigned int hashLen = 0;
		const char* EMBED_TOKEN_SECRET_KEY = std::getenv("EMBED_TOKEN_SECRET_KEY");
		std::string SECRET_KEY = EMBED_TOKEN_SECRET_KEY ? EMBED_TOKEN_SECRET_KEY : "default_secret_key";
		HMAC(EVP_sha256(), SECRET_KEY.data(), SECRET_KEY.size(),
			reinterpret_cast<const unsigned char*>(message.data()), message.size(),
			hash, &hashLen);

		std::string signature;
		for (unsigned int i = 0;i < hashLen;++i) {
			signature += std::format("{:02x}", hash[i]);
		}
		return std::format("{0}.{1}", expiry, signature);
	}

	bool validateToken(const std::string& videoId, const std::string& token)
	{
		auto parts = token.find('.');
		if (parts == std::string::npos) {
			return false;
		}
		std::string expiryStr = token.substr(0, parts);
		std::string signature = token.substr(parts + 1);

		time_t expiry = std::stoll(expiryStr);
		if (std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) > expiry) {
			return false;
		}

		std::string message = std::format("{0}:{1}", videoId, expiry);

		unsigned char hash[EVP_MAX_MD_SIZE];
		unsigned int hashLen = 0;
		const char* EMBED_TOKEN_SECRET_KEY = std::getenv("EMBED_TOKEN_SECRET_KEY");
		std::string SECRET_KEY = EMBED_TOKEN_SECRET_KEY ? EMBED_TOKEN_SECRET_KEY : "default_secret_key";
		HMAC(EVP_sha256(), SECRET_KEY.data(), SECRET_KEY.size(),
			reinterpret_cast<const unsigned char*>(message.data()), message.size(),
			hash, &hashLen);

		std::string expectedSignature;
		for (unsigned int i = 0;i < hashLen;++i) {
			expectedSignature += std::format("{:02x}", hash[i]);
		}
		return expectedSignature == signature;
	}
}
