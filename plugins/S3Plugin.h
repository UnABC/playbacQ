#pragma once

#include <cstdlib>
#include <string>
#include <drogon/plugins/Plugin.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/DeleteObjectsRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>

class S3Plugin : public drogon::Plugin<S3Plugin> {
private:
  Aws::SDKOptions options;
  // 外部向けpresigned URL生成用（127.0.0.1:9000）
  std::shared_ptr<Aws::S3::S3Client> s3Client;
#ifdef USE_INTERNAL_S3
  // 内部S3操作用（minio:9000）
  std::shared_ptr<Aws::S3::S3Client> s3InternalClient;
#endif
public:
  S3Plugin() {}
  void initAndStart(const Json::Value& config) override;
  void shutdown() override;
  std::string genPresignedUrl(const std::string& objectKey, const std::string& contentType, const std::string bucket);
  std::string genPresignedGetUrl(const std::string& videoPath, const long long expirationSeconds = 900, const std::string bucket = "videos");
  std::string getObject(const std::string& key, const std::string bucket = "videos");
  void deleteFolder(const std::string& prefix, const std::string bucket = "videos");
};

