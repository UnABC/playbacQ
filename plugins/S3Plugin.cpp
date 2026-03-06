#include "S3Plugin.h"
#include <aws/core/auth/AWSCredentials.h>
#include <stdexcept>

using namespace drogon;

void S3Plugin::initAndStart([[maybe_unused]] const Json::Value& config) {
    Aws::InitAPI(options);

    const char* envUser = std::getenv("MINIO_ROOT_USER");
    const char* envPassword = std::getenv("MINIO_ROOT_PASSWORD");
    const std::string accessKey = envUser ? envUser : "";
    const std::string secretKey = envPassword ? envPassword : "";
    Aws::Auth::AWSCredentials credentials(accessKey.c_str(), secretKey.c_str());
    Aws::Client::ClientConfiguration clientConfig;
    // ダミーリージョン
    clientConfig.region = "us-east-1";
    // MinIOのエンドポイント
    clientConfig.endpointOverride = "http://localhost:9000";
    s3Client = std::make_shared<Aws::S3::S3Client>(
        credentials,
        clientConfig,
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent,
        false
    );
}

void S3Plugin::shutdown() {
    s3Client.reset();
    Aws::ShutdownAPI(options);
}

std::string S3Plugin::genPresignedUrl(const std::string& videoId, const std::string bucket) {
    if (!s3Client) {
        throw std::runtime_error("S3 client is not initialized");
    }
    // 有効期限は15分
    long long expirationSeconds = 900;
    Aws::String presignedUrl = s3Client->GeneratePresignedUrl(
        bucket,
        videoId,
        Aws::Http::HttpMethod::HTTP_PUT,
        expirationSeconds
    );
    return std::string(presignedUrl.c_str());
}
