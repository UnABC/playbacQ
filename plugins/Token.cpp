#include "Token.h"

namespace Token {

	std::string generateEmbedToken(const std::string& videoId)
	{
		unsigned char hash[EVP_MAX_MD_SIZE];
		unsigned int hashLen = 0;
		const char* EMBED_TOKEN_SECRET_KEY = std::getenv("EMBED_TOKEN_SECRET_KEY");
		std::string SECRET_KEY = EMBED_TOKEN_SECRET_KEY ? EMBED_TOKEN_SECRET_KEY : "default_secret_key";
		HMAC(EVP_sha256(), SECRET_KEY.data(), SECRET_KEY.size(),
			reinterpret_cast<const unsigned char*>(videoId.data()), videoId.size(),
			hash, &hashLen);

		std::string signature;
		for (unsigned int i = 0;i < hashLen;++i) {
			signature += std::format("{:02x}", hash[i]);
		}
		return signature;
	}

	bool validateToken(const std::string& videoId, const std::string& token)
	{
		return generateEmbedToken(videoId) == token;
	}
}
