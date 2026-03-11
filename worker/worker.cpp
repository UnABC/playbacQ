#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <sw/redis++/redis++.h>
#include <boost/process.hpp>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <curl/curl.h>

bool upload2MinIO(const std::string& local_file_path, const std::string& bucket_name, const std::string& object_key) {
	const char* envUser = std::getenv("MINIO_ROOT_USER");
	const char* envPassword = std::getenv("MINIO_ROOT_PASSWORD");
	const std::string accessKey = envUser ? envUser : "";
	const std::string secretKey = envPassword ? envPassword : "";
	Aws::Auth::AWSCredentials credentials(accessKey.c_str(), secretKey.c_str());
	Aws::Client::ClientConfiguration clientConfig;
	clientConfig.endpointOverride = "http://minio:9000";
	clientConfig.region = "us-east-1";
	clientConfig.scheme = Aws::Http::Scheme::HTTP;
	Aws::S3::S3Client s3_client(credentials, clientConfig, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);

	Aws::S3::Model::PutObjectRequest request;
	request.SetBucket(bucket_name);
	request.SetKey(object_key);

	std::shared_ptr<Aws::IOStream> input_data =
		Aws::MakeShared<Aws::FStream>("PutObjectInputStream", local_file_path.c_str(), std::ios_base::in | std::ios_base::binary);

	if (!input_data->good()) {
		std::cerr << "Failed to open local file: " << local_file_path << std::endl;
		return false;
	}
	request.SetBody(input_data);

	auto outcome = s3_client.PutObject(request);
	if (outcome.IsSuccess()) {
		std::cout << "Successfully uploaded: " << object_key << std::endl;
		return true;
	} else {
		std::cerr << "Upload failed: " << outcome.GetError().GetMessage() << std::endl;
		return false;
	}
}

void postEncodeResult(const std::string& videoId, const std::string& status, const std::string& message) {
	try {
		std::string payload = "{\"video_id\": \"" + videoId + "\", \"status\": \"" + status + "\", \"message\": \"" + message + "\"}";
		CURL* curl = curl_easy_init();
		if (curl) {
			struct curl_slist* headers = curl_slist_append(NULL, "Content-Type: application/json");
			curl_easy_setopt(curl, CURLOPT_URL, "http://backend:8080/webhooks/encode_result");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
			// サーバーからのレスポンスを標準出力に出さないためのミュート設定
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
				return size * nmemb;
				});

			CURLcode res = curl_easy_perform(curl);

			if (res != CURLE_OK) {
				std::cerr << "Webhook failed: " << curl_easy_strerror(res) << std::endl;
			} else {
				std::cout << "Sent webhook to Drogon. Status: " << status << std::endl;
			}

			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
		} else {
			std::cerr << "Failed to initialize CURL" << std::endl;
			return;
		}
	}
	catch (const sw::redis::Error& e) {
		std::cerr << "Redis publish error: " << e.what() << std::endl;
	}
}

bool deleteFromMinIO(const std::string& bucket_name, const std::string& object_key) {
	const char* envUser = std::getenv("MINIO_ROOT_USER");
	const char* envPassword = std::getenv("MINIO_ROOT_PASSWORD");
	const std::string accessKey = envUser ? envUser : "";
	const std::string secretKey = envPassword ? envPassword : "";
	Aws::Auth::AWSCredentials credentials(accessKey.c_str(), secretKey.c_str());
	Aws::Client::ClientConfiguration clientConfig;
	clientConfig.endpointOverride = "http://minio:9000";
	clientConfig.region = "us-east-1";
	clientConfig.scheme = Aws::Http::Scheme::HTTP;
	Aws::S3::S3Client s3_client(credentials, clientConfig, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);

	Aws::S3::Model::DeleteObjectRequest request;
	request.SetBucket(bucket_name);
	request.SetKey(object_key);

	auto outcome = s3_client.DeleteObject(request);
	if (outcome.IsSuccess()) {
		std::cout << "Successfully deleted original file: " << object_key << std::endl;
		return true;
	} else {
		std::cerr << "Delete failed: " << outcome.GetError().GetMessage() << std::endl;
		return false;
	}
}

int main() {
	Aws::SDKOptions options;
	Aws::InitAPI(options);
	curl_global_init(CURL_GLOBAL_ALL);
	{
		std::cout << "Starting worker..." << std::endl;
		try {
			// ※ホスト名はdocker-composeのサービス名(redis)を指定
			sw::redis::ConnectionOptions connection_options;
			connection_options.host = "redis";
			connection_options.port = 6379;

			auto redis = sw::redis::Redis(connection_options);
			std::cout << "Connected to Redis successfully." << std::endl;
			std::cout << "Waiting for jobs on 'encode_queue'..." << std::endl;

			while (true) {
				// 戻り値 = {queue名(encode_queue), videoId}
				auto item = redis.blpop("encode_queue", 0);

				if (item) {
					std::string video_id = item->second;
					std::cout << "\n[JOB RECEIVED] Video ID: " << video_id << std::endl;
					std::string video_url = "http://minio:9000/videofiles/" + video_id + ".mp4";

					// 動画の総時間をffprobeで取得
					double total_duration_sec = 0.0;
					try {
						boost::process::ipstream probe_is;
						boost::process::child probe_c(boost::process::search_path("ffprobe"),
							boost::process::args({ "-v", "error", "-show_entries", "format=duration", "-of", "default=noprint_wrappers=1:nokey=1", video_url }),
							boost::process::std_out > probe_is
						);

						std::string duration_str;
						if (std::getline(probe_is, duration_str)) {
							total_duration_sec = std::stod(duration_str);
						}
						probe_c.wait();

						if (total_duration_sec <= 0.0) {
							std::cerr << "Invalid video duration: " << total_duration_sec << " seconds for video ID: " << video_id << std::endl;
							postEncodeResult(video_id, "failed", "Invalid video duration");
							continue;
						}
					}
					catch (const std::exception& e) {
						std::cerr << "ffprobe error: " << e.what() << std::endl;
						postEncodeResult(video_id, "failed", "ffprobe error: " + std::string(e.what()));
						continue;
					}

					int last_notified_percent = -1;
					// エンコード処理
					try {
						auto ffmpeg_path = boost::process::search_path("ffmpeg");
						if (ffmpeg_path.empty()) {
							throw std::runtime_error("ffmpeg not found in PATH");
						}
						std::string base_dir = "/tmp/playbacq_encode/" + video_id + "/";
						std::filesystem::create_directories(base_dir);

						boost::process::ipstream output_stream;
						std::vector<std::string> args = {
							"-i", "http://minio:9000/videofiles/" + video_id + ".mp4",
							"-progress", "pipe:1",
							"-c:v", "libx264",
							"-f", "hls",
							"-hls_time", "2",	// セグメントの長さを2秒に設定
							"-hls_segment_type", "fmp4",
							"-hls_flags", "single_file",
							base_dir + "/output.m3u8"
						};
						std::cout << "Starting ffmpeg process for video ID: " << video_id << std::endl;
						boost::process::child ffmpeg_process(ffmpeg_path, boost::process::args(args), boost::process::std_out > output_stream, boost::process::std_err > boost::process::null);
						std::string line;
						while (ffmpeg_process.running() && std::getline(output_stream, line)) {
							std::cout << "[FFmpeg] " << line << std::endl;
							if (line.starts_with("out_time_us=")) {
								try {
									// "out_time_us="以降の数値を取得
									long long micro_seconds = std::stoll(line.substr(12));
									double current_sec = micro_seconds / 1000000.0;
									int current_percent = std::min(static_cast<int>((current_sec / total_duration_sec) * 100), 100);

									if (current_percent > last_notified_percent) {
										// SET video:progress:{id} {percent} (有効期限24時間)
										redis.set("video:progress:" + video_id, std::to_string(current_percent), std::chrono::hours(24));
										last_notified_percent = current_percent;
										std::cout << "Progress updated: " << current_percent << "% for video ID: " << video_id << std::endl;
									}
								}
								catch (const std::exception& e) {
									// パース失敗時 (out_time_us=N/A などが来た場合) は無視して続行
								}
							}
						}
						ffmpeg_process.wait();
						int exit_code = ffmpeg_process.exit_code();
						if (exit_code == 0) {
							std::cout << "Encoding completed successfully for video ID: " << video_id << std::endl;
						} else {
							std::cerr << "ffmpeg exited with code " << exit_code << " for video ID: " << video_id << std::endl;
							// エラーが発生した場合はDrogonに失敗を知らせる (Pub/Sub)
							postEncodeResult(video_id, "failed", "ffmpeg exited with code " + std::to_string(exit_code));
							continue;
						}
					}
					catch (const std::exception& e) {
						std::cerr << "Encoding Error: " << e.what() << std::endl;
						postEncodeResult(video_id, "failed", "Encoding error: " + std::string(e.what()));
						continue;
					}

					std::string base_dir = "/tmp/playbacq_encode/" + video_id + "/";
					upload2MinIO(base_dir + "output.mp4", "videos", "hls/" + video_id + "/output.mp4");
					upload2MinIO(base_dir + "output.m3u8", "videos", "hls/" + video_id + "/output.m3u8");
					std::filesystem::remove_all("/tmp/playbacq_encode/" + video_id);

					deleteFromMinIO("videofiles", video_id + ".mp4");

					std::cout << "[JOB COMPLETED] Video ID: " << video_id << std::endl;

					postEncodeResult(video_id, "completed", "Encoding completed successfully");
				}
			}

		}
		catch (const sw::redis::Error& e) {
			std::cerr << "Redis Error: " << e.what() << std::endl;
			return 1;
		}
	}
	curl_global_cleanup();
	Aws::ShutdownAPI(options);
	return 0;
}