#include "S3Plugin.h"

using namespace drogon;

void S3Plugin::initAndStart(const Json::Value& config) {
    Aws::InitAPI(options);

    const char* envUser = std::getenv("MINIO_ROOT_USER");
    const char* envPassword = std::getenv("MINIO_ROOT_PASSWORD");
    Aws::Auth::AWSCredentials credentials(envUser, envPassword);
    Aws::Client::ClientConfiguration clientConfig;
    // ダミーリージョン
    clientConfig.region = "us-east-1";
    // MinIOのエンドポイント
    clientConfig.endpointOverride = "http://localhost:9000";
    s3Client = std::make_shared<Aws::S3::S3Client>(
        credentials,
        clientConfig,
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Default,
        false
    );
}

void S3Plugin::shutdown() {
    s3Client.reset();
    Aws::ShutdownAPI(options);
}

std::string S3Plugin::genPresignedUrl(const std::string& videoId, const std::string bucket) {
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
