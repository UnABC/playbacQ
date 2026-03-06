#pragma once

#include <cstdlib>
#include <string>
#include <drogon/plugins/Plugin.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>

class S3Plugin : public drogon::Plugin<S3Plugin> {
private:
  Aws::SDKOptions options;
  std::shared_ptr<Aws::S3::S3Client> s3Client;
public:
  S3Plugin() {}
  void initAndStart(const Json::Value& config) override;
  void shutdown() override;
  std::string genPresignedUrl(const std::string& videoId, const std::string bucket = "videofiles");
};

