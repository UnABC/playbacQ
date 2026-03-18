#include "S3Plugin.h"
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/s3/model/Delete.h>
#include <aws/s3/model/ObjectIdentifier.h>
#include <sstream>
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
    // 外部向けpresigned URL生成用エンドポイント(localhostだとIP v6の問題上バグるので127.0.0.1を使用)
    clientConfig.endpointOverride = "http://127.0.0.1:9000";
    // HTTPを使用
    clientConfig.scheme = Aws::Http::Scheme::HTTP;
    s3Client = std::make_shared<Aws::S3::S3Client>(
        credentials,
        clientConfig,
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent,
        false
    );

    // 内部S3操作用クライアント（Docker内部ネットワーク経由）
    Aws::Client::ClientConfiguration internalConfig;
    internalConfig.region = "us-east-1";
    const char* envEndpoint = std::getenv("MINIO_ENDPOINT");
    internalConfig.endpointOverride = envEndpoint ? std::string("http://") + envEndpoint : "http://minio:9000";
    internalConfig.scheme = Aws::Http::Scheme::HTTP;
    s3InternalClient = std::make_shared<Aws::S3::S3Client>(
        credentials,
        internalConfig,
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent,
        false
    );
}

void S3Plugin::shutdown() {
    s3Client.reset();
    s3InternalClient.reset();
    Aws::ShutdownAPI(options);
}

std::string S3Plugin::genPresignedUrl(const std::string& videoId, const std::string bucket) {
    if (!s3Client) {
        throw std::runtime_error("S3 client is not initialized");
    }
    // 有効期限は15分
    const long long expirationSeconds = 900;

    Aws::Http::HeaderValueCollection customHeaders;
    customHeaders.emplace("content-type", "video/mp4");

    Aws::String presignedUrl = s3Client->GeneratePresignedUrl(
        bucket,
        videoId,
        Aws::Http::HttpMethod::HTTP_PUT,
        customHeaders,
        expirationSeconds
    );
    return std::string(presignedUrl.c_str());
}

std::string S3Plugin::genPresignedGetUrl(const std::string& videoPath, const long long expirationSeconds, const std::string bucket) {
    if (!s3Client) {
        throw std::runtime_error("S3 client is not initialized");
    }

    Aws::String presignedUrl = s3Client->GeneratePresignedUrl(
        bucket,
        videoPath,
        Aws::Http::HttpMethod::HTTP_GET,
        Aws::Http::HeaderValueCollection(),
        expirationSeconds
    );
    return std::string(presignedUrl.c_str());
}

// m3u8ファイル取得用
std::string S3Plugin::getObject(const std::string& key, const std::string bucket) {
    if (!s3InternalClient) {
        throw std::runtime_error("S3 internal client is not initialized");
    }

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(key);

    auto outcome = s3InternalClient->GetObject(request);
    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to get object from S3: " + std::string(outcome.GetError().GetMessage().c_str()));
    }

    std::ostringstream ss;
    ss << outcome.GetResult().GetBody().rdbuf();
    return ss.str();
}

void S3Plugin::deleteFolder(const std::string& prefix, const std::string bucket) {
    if (!s3InternalClient) {
        throw std::runtime_error("S3 internal client is not initialized");
    }

    Aws::S3::Model::ListObjectsV2Request listRequest;
    listRequest.SetBucket(bucket);
    listRequest.SetPrefix(prefix);

    bool isTruncated = false;
    do {
        auto listOutcome = s3InternalClient->ListObjectsV2(listRequest);
        if (!listOutcome.IsSuccess()) {
            throw std::runtime_error("Failed to list objects for deletion: " + std::string(listOutcome.GetError().GetMessage().c_str()));
        }
        const auto& objects = listOutcome.GetResult().GetContents();
        // 削除するオブジェクトがない場合は終了
        if (objects.empty()) break;
        // 一括削除リクエストの作成
        Aws::S3::Model::DeleteObjectsRequest deleteRequest;
        deleteRequest.SetBucket(bucket);
        Aws::S3::Model::Delete deleteObjects;
        for (const auto& object : objects) {
            Aws::S3::Model::ObjectIdentifier id;
            id.SetKey(object.GetKey());
            deleteObjects.AddObjects(id);
        }
        deleteObjects.SetQuiet(true);
        deleteRequest.SetDelete(deleteObjects);
        // 一括削除の実行
        auto deleteOutcome = s3InternalClient->DeleteObjects(deleteRequest);
        if (!deleteOutcome.IsSuccess()) {
            throw std::runtime_error("Failed to delete objects: " + std::string(deleteOutcome.GetError().GetMessage().c_str()));
        }
        // 一括削除の上限は1000件なので、続きがあるか確認してループ
        isTruncated = listOutcome.GetResult().GetIsTruncated();
        if (isTruncated) {
            listRequest.SetContinuationToken(listOutcome.GetResult().GetNextContinuationToken());
        }
    } while (isTruncated);
}