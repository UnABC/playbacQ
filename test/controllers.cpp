#include <drogon/drogon_test.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpTypes.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <future>
#include <iostream>

drogon::HttpResponsePtr sendSyncRequest(
	drogon::HttpMethod method,
	const std::string& path,
	const Json::Value& body = Json::Value::null,
	const std::unordered_map<std::string, std::string>& queries = {}
) {
	auto client = drogon::HttpClient::newHttpClient("http://127.0.0.1:8080");
	auto req = drogon::HttpRequest::newHttpRequest();
	if (!body.isNull()) {
		req = drogon::HttpRequest::newHttpJsonRequest(body);
	} else {
		req = drogon::HttpRequest::newHttpRequest();
	}

	req->setMethod(method);
	req->setPath(path);

	for (const auto& [key, value] : queries) {
		req->setParameter(key, value);
	}

	req->addHeader("X-Forwarded-User", "testuser"); // 認証フィルタを通すためのヘッダ

	std::promise<drogon::HttpResponsePtr> prom;
	auto future = prom.get_future();

	client->sendRequest(req, [&prom](drogon::ReqResult res, const drogon::HttpResponsePtr& resp) {
		if (res == drogon::ReqResult::Ok && resp != nullptr) {
			prom.set_value(resp);
		} else {
			prom.set_value(nullptr); // 通信エラー
		}
		});
	return future.get();
}

void uploadDummyM3u8ToMinIO(const std::string& videoId, const std::string& content) {
	Aws::Client::ClientConfiguration clientConfig;
	clientConfig.region = "us-east-1";
	const char* envEndpoint = std::getenv("MINIO_ENDPOINT");
	clientConfig.endpointOverride = envEndpoint ? std::string("http://") + envEndpoint : "http://minio:9000";
	clientConfig.scheme = Aws::Http::Scheme::HTTP;

	const char* envUser = std::getenv("MINIO_ROOT_USER");
	const char* envPassword = std::getenv("MINIO_ROOT_PASSWORD");
	const std::string accessKey = envUser ? envUser : "";
	const std::string secretKey = envPassword ? envPassword : "";
	Aws::Auth::AWSCredentials credentials(accessKey.c_str(), secretKey.c_str());

	Aws::S3::S3Client s3Client(
		credentials,
		clientConfig,
		Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent,
		false
	);

	Aws::S3::Model::PutObjectRequest request;
	request.SetBucket("videos");
	request.SetKey("hls/" + videoId + "/output.m3u8");

	auto inputData = Aws::MakeShared<Aws::StringStream>("PutObjectInputStream");
	*inputData << content;
	request.SetBody(inputData);

	auto outcome = s3Client.PutObject(request);
	if (!outcome.IsSuccess()) {
		std::cerr << "Failed to upload dummy M3U8 to S3: "
			<< outcome.GetError().GetMessage() << std::endl;
	} else {
		std::cerr << "Successfully uploaded dummy M3U8 to S3" << std::endl;
	}
}

std::optional<std::string> postVideo(std::string title, std::string description = "This is a test video.") {
	Json::Value createBody;
	createBody["title"] = title;
	createBody["description"] = description;
	auto createResp = sendSyncRequest(drogon::Post, "/api/videos", createBody);
	if (createResp == nullptr) {
		std::cerr << "Failed to send POST request to create video" << std::endl;
		return std::nullopt;
	}
	if (createResp->getStatusCode() != drogon::k201Created) {
		std::cerr << "Failed to create video, status code: " << createResp->getStatusCode() << std::endl;
		return std::nullopt;
	}
	auto createdJson = createResp->getJsonObject();
	if (createdJson == nullptr || !createdJson->isMember("video_id") || !(*createdJson)["video_id"].isString()) {
		std::cerr << "Invalid response format when creating video" << std::endl;
		return std::nullopt;
	}
	std::string videoId = (*createdJson)["video_id"].asString();

	// Webhookを送信してステータスをCompletedにする
	Json::Value webhookBody;
	webhookBody["video_id"] = videoId;
	webhookBody["status"] = "completed";
	webhookBody["message"] = "success";
	webhookBody["duration"] = 120;
	sendSyncRequest(drogon::Post, "/webhooks/encode_result", webhookBody);

	return videoId;
}

void clearDatabase() {
	auto dbClient = drogon::app().getDbClient();
	try {
		dbClient->execSqlSync("DELETE FROM comments");
		dbClient->execSqlSync("DELETE FROM video_tags");
		dbClient->execSqlSync("DELETE FROM tags");
		dbClient->execSqlSync("DELETE FROM videos");

		std::cerr << "Cleared database" << std::endl;
	}
	catch (const drogon::orm::DrogonDbException& e) {
		std::cerr << "Failed to clear database: " << e.base().what() << std::endl;
	}
}

DROGON_TEST(ApiVideosTest)
{
	// POST,GET,DELETE api/videosのE2Eテスト
	clearDatabase();
	std::optional<std::string> videoIdOpt = postVideo("テスト動画");
	REQUIRE(videoIdOpt.has_value());
	std::string videoId = videoIdOpt.value();

	auto getResp = sendSyncRequest(drogon::Get, "/api/videos/" + videoId);
	REQUIRE(getResp != nullptr);
	CHECK(getResp->getStatusCode() == drogon::k200OK);
	auto getJson = getResp->getJsonObject();
	CHECK((*getJson)["title"].asString() == "テスト動画");
	CHECK((*getJson)["description"].asString() == "This is a test video.");

	Json::Value deleteBody;
	deleteBody["video_id"] = videoId;
	auto deleteResp = sendSyncRequest(drogon::Delete, "/api/videos", deleteBody);
	REQUIRE(deleteResp != nullptr);
	CHECK(deleteResp->getStatusCode() == drogon::k200OK);

	auto confirmResp = sendSyncRequest(drogon::Get, "/api/videos/" + videoId);
	REQUIRE(confirmResp != nullptr);
	CHECK(confirmResp->getStatusCode() == drogon::k404NotFound);
}

DROGON_TEST(SearchTest)
{
	clearDatabase();
	// 動画を投稿
	std::optional<std::string> videoId1Opt = postVideo("1猫の動画", "にゃーん");
	std::optional<std::string> videoId2Opt = postVideo("2犬の動画", "きゃんきゃん");
	std::optional<std::string> videoId3Opt = postVideo("3ヌオーの動画", "ヌオー");
	std::optional<std::string> videoId4Opt = postVideo("4空白の動画", "empty");
	std::optional<std::string> videoId5Opt = postVideo("5帝国", "empire");

	if (!videoId1Opt.has_value() || !videoId2Opt.has_value() || !videoId3Opt.has_value() || !videoId4Opt.has_value() || !videoId5Opt.has_value()) {
		REQUIRE(false);
	}

	std::string videoId1 = videoId1Opt.value();
	std::string videoId2 = videoId2Opt.value();
	std::string videoId3 = videoId3Opt.value();
	std::string videoId4 = videoId4Opt.value();
	std::string videoId5 = videoId5Opt.value();
	// 投稿された動画の中から検索
	std::unordered_map<std::string, std::string> queries = {
		{"search", "動画"},
		{"sortby", "title"},
		{"order", "1"}
	};
	auto searchResp = sendSyncRequest(drogon::Get, "/api/videos", Json::Value::null, queries);
	REQUIRE(searchResp != nullptr);
	CHECK(searchResp->getStatusCode() == drogon::k200OK);
	auto searchJson = searchResp->getJsonObject();
	REQUIRE(searchJson != nullptr);
	CHECK(searchJson->isArray());
	CHECK(searchJson->size() == 4);
	CHECK((*searchJson)[0]["video_id"].asString() == videoId1);
	CHECK((*searchJson)[1]["video_id"].asString() == videoId2);
	CHECK((*searchJson)[2]["video_id"].asString() == videoId3);
	CHECK((*searchJson)[3]["video_id"].asString() == videoId4);
	// 検索その2
	queries = {
		{"search", "em"},
		{"sortby", "title"},
		{"order", "0"}
	};
	searchResp = sendSyncRequest(drogon::Get, "/api/videos", Json::Value::null, queries);
	REQUIRE(searchResp != nullptr);
	CHECK(searchResp->getStatusCode() == drogon::k200OK);
	searchJson = searchResp->getJsonObject();
	REQUIRE(searchJson != nullptr);
	CHECK(searchJson->isArray());
	CHECK(searchJson->size() == 2);
	CHECK((*searchJson)[0]["video_id"].asString() == videoId5);
	CHECK((*searchJson)[1]["video_id"].asString() == videoId4);
	// タグ付与
	Json::Value tagBody;
	tagBody["tag"] = "かわいい";
	auto tagResp = sendSyncRequest(drogon::Post, "/api/videos/" + videoId1 + "/tags", tagBody);
	REQUIRE(tagResp != nullptr);
	CHECK(tagResp->getStatusCode() == drogon::k200OK);
	tagResp = sendSyncRequest(drogon::Post, "/api/videos/" + videoId2 + "/tags", tagBody);
	REQUIRE(tagResp != nullptr);
	CHECK(tagResp->getStatusCode() == drogon::k200OK);
	tagResp = sendSyncRequest(drogon::Post, "/api/videos/" + videoId3 + "/tags", tagBody);
	REQUIRE(tagResp != nullptr);
	CHECK(tagResp->getStatusCode() == drogon::k200OK);
	// タグ検索
	queries = {
		{"search", "ー"},
		{"tag", "かわいい"},
		{"sortby", "title"},
		{"order", "1"}
	};
	searchResp = sendSyncRequest(drogon::Get, "/api/videos", Json::Value::null, queries);
	REQUIRE(searchResp != nullptr);
	CHECK(searchResp->getStatusCode() == drogon::k200OK);
	searchJson = searchResp->getJsonObject();
	REQUIRE(searchJson != nullptr);
	CHECK(searchJson->isArray());
	CHECK(searchJson->size() == 2);
	CHECK((*searchJson)[0]["video_id"].asString() == videoId1);
	CHECK((*searchJson)[1]["video_id"].asString() == videoId3);
	// タグ複数付与テスト
	tagBody["tag"] = "人類には早すぎる動画";
	tagResp = sendSyncRequest(drogon::Post, "/api/videos/" + videoId3 + "/tags", tagBody);
	REQUIRE(tagResp != nullptr);
	CHECK(tagResp->getStatusCode() == drogon::k200OK);
	tagResp = sendSyncRequest(drogon::Post, "/api/videos/" + videoId4 + "/tags", tagBody);
	REQUIRE(tagResp != nullptr);
	CHECK(tagResp->getStatusCode() == drogon::k200OK);
	// タグ検索2
	queries = {
		{"search", ""},
		{"tag", "人類には早すぎる動画"},
		{"sortby", "title"},
		{"order", "1"}
	};
	searchResp = sendSyncRequest(drogon::Get, "/api/videos", Json::Value::null, queries);
	REQUIRE(searchResp != nullptr);
	CHECK(searchResp->getStatusCode() == drogon::k200OK);
	searchJson = searchResp->getJsonObject();
	REQUIRE(searchJson != nullptr);
	CHECK(searchJson->isArray());
	CHECK(searchJson->size() == 2);
	CHECK((*searchJson)[0]["video_id"].asString() == videoId3);
	CHECK((*searchJson)[1]["video_id"].asString() == videoId4);
	// タグ情報取得
	auto tagInfo = sendSyncRequest(drogon::Get, "/api/videos/" + videoId4 + "/tags");
	REQUIRE(tagInfo != nullptr);
	CHECK(tagInfo->getStatusCode() == drogon::k200OK);
	auto tagInfoJson = tagInfo->getJsonObject();
	REQUIRE(tagInfoJson != nullptr);
	CHECK(tagInfoJson->isArray());
	CHECK(tagInfoJson->size() == 1);
	CHECK((*tagInfoJson)[0]["name"].asString() == "人類には早すぎる動画");
	// タグ削除
	Json::Value removeTagBody;
	removeTagBody["tag_id"] = (*tagInfoJson)[0]["tag_id"].asInt();
	auto removeTagResp = sendSyncRequest(drogon::Delete, "/api/videos/" + videoId4 + "/tags", removeTagBody);
	REQUIRE(removeTagResp != nullptr);
	CHECK(removeTagResp->getStatusCode() == drogon::k200OK);
	// タグ削除確認
	auto confirmTagInfo = sendSyncRequest(drogon::Get, "/api/videos/" + videoId4 + "/tags");
	REQUIRE(confirmTagInfo != nullptr);
	CHECK(confirmTagInfo->getStatusCode() == drogon::k200OK);
	auto confirmTagInfoJson = confirmTagInfo->getJsonObject();
	REQUIRE(confirmTagInfoJson != nullptr);
	CHECK(confirmTagInfoJson->isArray());
	CHECK(confirmTagInfoJson->size() == 0);
}

DROGON_TEST(WebhookTest)
{
	// WebhookのE2Eテスト
	clearDatabase();
	std::optional<std::string> videoIdOpt = postVideo("Webhookと再生のテスト");
	REQUIRE(videoIdOpt.has_value());
	std::string videoId = videoIdOpt.value();

	Json::Value webhookBody;
	webhookBody["video_id"] = videoId;
	webhookBody["status"] = "completed";
	webhookBody["message"] = "success";
	webhookBody["duration"] = 120;
	auto webhookResp = sendSyncRequest(drogon::Post, "/webhooks/encode_result", webhookBody);
	REQUIRE(webhookResp != nullptr);
	CHECK(webhookResp->getStatusCode() == drogon::k200OK);

	std::string dummyM3u8 =
		"#EXTM3U\n"
		"#EXT-X-VERSION:3\n"
		"#EXT-X-MAP:URI=\"init.mp4\"\n"
		"segment0.ts\n";
	uploadDummyM3u8ToMinIO(videoId, dummyM3u8);
	auto playResp = sendSyncRequest(drogon::Get, "/api/videos/" + videoId + "/play");
	REQUIRE(playResp != nullptr);
	CHECK(playResp->getStatusCode() == drogon::k200OK);

	std::string responseM3u8 = std::string(playResp->getBody());
	CHECK(responseM3u8.find("X-Amz-Signature=") != std::string::npos);
	CHECK(responseM3u8.find("X-Amz-Credential=") != std::string::npos);
	CHECK(responseM3u8.find("X-Amz-Expires=") != std::string::npos);
	CHECK(responseM3u8.find("segment0.ts") != std::string::npos);
}

DROGON_TEST(ProgressTest)
{
	clearDatabase();
	std::string videoId = drogon::utils::genRandomString(11);
	auto dbClient = drogon::app().getDbClient();
	try {
		dbClient->execSqlSync(
			"INSERT INTO videos (video_id, user_id,video_url, title, status) "
			"VALUES (?, 'test_user', 'https://example.com/video.mp4', '進捗テスト', 1)",
			videoId
		);
	}
	catch (const drogon::orm::DrogonDbException& e) {
		std::cerr << "DB Error: " << e.base().what() << std::endl;
	}

	auto redisClient = drogon::app().getRedisClient();
	REQUIRE(redisClient != nullptr);
	std::promise<void> redisProm;
	auto redisFut = redisProm.get_future();
	std::string redisKey = "video:progress:" + videoId;
	redisClient->execCommandAsync(
		[&redisProm](const drogon::nosql::RedisResult& r) {
			std::cerr << "SET '75%' completed" << std::endl;
			redisProm.set_value();
		},
		[&redisProm](const std::exception& e) {
			std::cerr << "Error setting Redis value: " << e.what() << std::endl;
			redisProm.set_value();
		},
		"SET %s %d", redisKey.c_str(), 75
	);
	redisFut.get();

	auto getResp = sendSyncRequest(drogon::Get, "/api/videos/" + videoId + "/progress");
	REQUIRE(getResp != nullptr);
	CHECK(getResp->getStatusCode() == drogon::k200OK);
	auto getJson = getResp->getJsonObject();
	REQUIRE(getJson != nullptr);
	CHECK((*getJson)["progress"].asInt() == 75);
	CHECK((*getJson)["status"].asInt() == 1);
}