#pragma once

#include <cstdlib>
#include <string>
#include <drogon/plugins/Plugin.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>

class S3Plugin : public drogon::Plugin<S3Plugin> {
private:
  Aws::SDKOptions options;
  // 外部向けpresigned URL生成用（127.0.0.1:9000）
  std::shared_ptr<Aws::S3::S3Client> s3Client;
  // 内部S3操作用（minio:9000）
  std::shared_ptr<Aws::S3::S3Client> s3InternalClient;
public:
  S3Plugin() {}
  void initAndStart(const Json::Value& config) override;
  void shutdown() override;
  std::string genPresignedUrl(const std::string& videoId, const std::string bucket = "videofiles");
  std::string genPresignedGetUrl(const std::string& videoId, const long long expirationSeconds = 900, const std::string bucket = "videos");
  std::string getObject(const std::string& key, const std::string bucket = "videos");
};

