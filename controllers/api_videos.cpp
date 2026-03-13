#include "api_videos.h"
#include <drogon/orm/Exception.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>
#include <drogon/nosql/RedisClient.h>
#include <json/json.h>
#include <string>
#include <optional>
#include <iostream>
#include <cstdlib>
#include <regex>
#include <format>
#include <ranges>
#include <string_view>
#include "../models/Videos.h"
#include "../models/Tags.h"
#include "../plugins/S3Plugin.h"
#include "Status.h"

using namespace api;

drogon::Task<drogon::HttpResponsePtr> videos::getVideos(HttpRequestPtr req) {
	std::optional<std::string> userId = req->getOptionalParameter<std::string>("userId");
	std::optional<std::string> search = req->getOptionalParameter<std::string>("search");
	std::optional<std::string> tag = req->getOptionalParameter<std::string>("tag");
	std::optional<std::string> sortby = req->getOptionalParameter<std::string>("sortby");
	std::optional<int> order = req->getOptionalParameter<int>("order");

	drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
	drogon::orm::Criteria criteria;

	criteria = criteria && drogon::orm::Criteria(drogon_model::playbacq::Videos::Cols::_status, drogon::orm::CompareOperator::EQ, (uint8_t)Status::completed);
	if (userId.has_value()) {
		criteria = criteria && drogon::orm::Criteria(drogon_model::playbacq::Videos::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId.value());
	}
	if (search.has_value()) {
		std::string searchStr = "%" + search.value() + "%";
		auto titleCriteria = drogon::orm::Criteria(drogon_model::playbacq::Videos::Cols::_title, drogon::orm::CompareOperator::Like, searchStr);
		auto descriptionCriteria = drogon::orm::Criteria(drogon_model::playbacq::Videos::Cols::_description, drogon::orm::CompareOperator::Like, searchStr);
		criteria = criteria && (titleCriteria || descriptionCriteria);
	}
	if (tag.has_value()) {
		//TODO: タグ検索の実装。タグは複数持てる可能性があるため、動画とタグの中間テーブルを作成して、そこから動画を検索する必要がある。
	}
	if (sortby.has_value()) {
		auto sort_order = (order.has_value() && order.value() == 0) ? drogon::orm::SortOrder::DESC : drogon::orm::SortOrder::ASC;
		// TODO : バリエーションを増やす
		if (sortby.value() == "created_at") {
			mapper.orderBy(drogon_model::playbacq::Videos::Cols::_created_at, sort_order);
		} else if (sortby.value() == "title") {
			mapper.orderBy(drogon_model::playbacq::Videos::Cols::_title, sort_order);
		} else {
			// TODO : デフォルト値をいい感じにする
			mapper.orderBy(drogon_model::playbacq::Videos::Cols::_created_at, drogon::orm::SortOrder::DESC);
		}
	}

	// DB検索開始
	try {
		auto videosList = co_await mapper.findBy(criteria);
		Json::Value jsonResponse(Json::arrayValue);
		for (const auto& video : videosList) {
			jsonResponse.append(video.toJson());
		}
		co_return drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
	}
	catch (const std::exception& e) {
		std::cerr << "DB Error: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to retrieve videos: " + std::string(e.what()));
		co_return resp;
	}
}

drogon::Task<drogon::HttpResponsePtr> videos::postVideos(HttpRequestPtr req) {
	auto jsonPtr = req->getJsonObject();
	if (!jsonPtr) {
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
		resp->setBody("Invalid JSON format");
		co_return resp;
	}
	try {
		drogon_model::playbacq::Videos newVideo;
		// 動画IDは乱数。垓一衝突したらエラーを投げるはずなのでユーザーがもう一度リクエストすればよい。
		std::string videoId = drogon::utils::genRandomString(11);
		newVideo.setVideoId(videoId);
		newVideo.setCreatedAt(trantor::Date::now());
		newVideo.setViewCount(0);

		// JSONから動画情報を設定
		newVideo.setTitle(jsonPtr->get("title", "NULL").asString());
		newVideo.setDescription(jsonPtr->get("description", "NULL").asString());
		newVideo.setUserId(req->getAttributes()->get<std::string>("userId"));
		newVideo.setStatus((uint8_t)Status::pending);

		// S3の事前署名URLを生成
		auto s3Plugin = drogon::app().getPlugin<S3Plugin>();
		std::string uploadUrl = s3Plugin->genPresignedUrl(videoId + ".mp4");
		std::string thumbUploadUrl = s3Plugin->genPresignedUrl(videoId + ".jpg", "thumbnails");

		// 動画URLを設定
		auto customConfig = drogon::app().getCustomConfig();
		std::string baseUrl = customConfig["baseUrl"].asString();
		newVideo.setVideoUrl(baseUrl + "/watch/" + videoId + ".mp4");
		newVideo.setThumbnailUrl(baseUrl + "/thumbnails/" + videoId + ".jpg");

		drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
		co_await mapper.insert(newVideo);

		Json::Value jsonResponse = newVideo.toJson();
		jsonResponse["uploadUrl"] = uploadUrl;
		jsonResponse["thumbUploadUrl"] = thumbUploadUrl;

		auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
		resp->setStatusCode(drogon::HttpStatusCode::k201Created);
		co_return resp;
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to create video: " + std::string(e.what()));
		co_return resp;
	}
}

drogon::Task<drogon::HttpResponsePtr> videos::getVideo([[maybe_unused]] HttpRequestPtr req, std::string id) {
	drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
	try {
		auto video = co_await mapper.findByPrimaryKey(id);
		auto resp = drogon::HttpResponse::newHttpJsonResponse(video.toJson());
		co_return resp;
	}
	catch (const drogon::orm::UnexpectedRows& e) {
		// Not found 404
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
		resp->setBody("Video not found");
		co_return resp;
	}
	catch (const std::exception& e) {
		std::cerr << "DB Error: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to retrieve video: " + std::string(e.what()));
		co_return resp;
	}
}

drogon::Task<drogon::HttpResponsePtr> videos::getVideoProgress([[maybe_unused]] HttpRequestPtr req, std::string id) {
	// Statusの確認
	std::cout << "Checking progress for video ID: " << id << std::endl;
	drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
	Json::Value jsonResponse;
	try {
		auto video = co_await mapper.findByPrimaryKey(id);
		if (*video.getStatus() != (uint8_t)Status::processing) {
			jsonResponse["progress"] = Json::nullValue;
			jsonResponse["status"] = *video.getStatus();
			auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
			co_return resp;
		}
	}
	catch (const drogon::orm::UnexpectedRows& e) {
		// Not found 404
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
		resp->setBody("Video not found");
		co_return resp;
	}
	catch (const std::exception& e) {
		std::cerr << "DB Error: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to retrieve video: " + std::string(e.what()));
		co_return resp;
	}
	// processing中の場合進捗も取得
	auto redis = drogon::app().getRedisClient();
	if (!redis) {
		std::cerr << "Failed to get Redis client" << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to get Redis client");
		co_return resp;
	}
	try {
		auto result = co_await redis->execCommandCoro("GET video:progress:%s", id.c_str());
		if (!result.isNil()) {
			std::string progressStr = result.asString();
			try {
				jsonResponse["progress"] = std::max(0, std::min(100, std::stoi(progressStr)));
			}
			catch (const std::exception& e) {
				std::cerr << "Invalid progress value in Redis: " << progressStr << std::endl;
				jsonResponse["progress"] = Json::nullValue;
			}
		} else {
			jsonResponse["progress"] = Json::nullValue;
		}
		jsonResponse["status"] = (uint8_t)Status::processing;
		auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse);
		co_return resp;
	}
	catch (const std::exception& e) {
		std::cerr << "Redis Error: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to retrieve progress: " + std::string(e.what()));
		co_return resp;
	}
}

drogon::Task<drogon::HttpResponsePtr> videos::getVideoPlayM3u8([[maybe_unused]] HttpRequestPtr req, std::string id) {
	// 動画URLの確認
	std::cout << "Checking play URL for video ID: " << id << std::endl;
	drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
	Json::Value jsonResponse;
	try {
		auto video = co_await mapper.findByPrimaryKey(id);
		if (*video.getStatus() != (uint8_t)Status::completed) {
			auto resp = drogon::HttpResponse::newHttpJsonResponse(Json::Value());
			co_return resp;
		}
		auto s3Plugin = drogon::app().getPlugin<S3Plugin>();
		std::string base_path = "hls/" + id + "/";
		// S3 SDKで直接m3u8ファイルを取得
		std::string original_m3u8 = s3Plugin->getObject(base_path + "output.m3u8");
		std::cout << "Successfully fetched m3u8 from MinIO for video ID: " << id << std::endl;

		std::string rewritten_m3u8;
		for (const auto& line_range : original_m3u8 | std::views::split('\n')) {
			std::string_view line(line_range.begin(), line_range.end());
			// Windowsの改行コードに対応するため、行末の'\r'を削除
			if (!line.empty() && line.back() == '\r') {
				line.remove_suffix(1);
			}
			if (line.empty()) continue;
			// 署名付きURLに書き換える
			if (line.starts_with("#EXT-X-MAP:URI=\"")) {
				auto uri_start = line.find("\"") + 1;
				auto uri_end = line.find("\"", uri_start);
				if (uri_start != std::string_view::npos && uri_end != std::string_view::npos) {
					std::string_view filename{ line.substr(uri_start, uri_end - uri_start) };
					rewritten_m3u8 += std::format("#EXT-X-MAP:URI=\"{}\"{}\n",
						s3Plugin->genPresignedGetUrl(base_path + std::string(filename)),
						line.substr(uri_end + 1));
				} else {
					std::cerr << "Invalid EXT-X-MAP line in m3u8: " << line << std::endl;
					rewritten_m3u8 += std::string(line) + "\n";
				}

			} else if (line.starts_with("#")) {
				rewritten_m3u8 += std::string(line) + "\n";
			} else {
				rewritten_m3u8 += s3Plugin->genPresignedGetUrl(base_path + std::string(line)) + "\n";
			}
		}

		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setBody(rewritten_m3u8);
		resp->setContentTypeCode(drogon::CT_CUSTOM);
		resp->setContentTypeString("application/vnd.apple.mpegurl");

		co_return resp;
	}
	catch (const drogon::orm::UnexpectedRows& e) {
		// Not found 404
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
		resp->setBody("Video not found");
		co_return resp;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to build play m3u8 response: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to build play m3u8 response: " + std::string(e.what()));
		co_return resp;
	}
}

drogon::Task<drogon::HttpResponsePtr> videos::getVideoThumbnails([[maybe_unused]] HttpRequestPtr req, std::string id, std::string filename) {
	// サムネイル画像へリダイレクト
	drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
	try {
		auto s3Plugin = drogon::app().getPlugin<S3Plugin>();
		std::string presigned_url = s3Plugin->genPresignedGetUrl("hls/" + id + "/" + filename, 60);

		auto resp = drogon::HttpResponse::newRedirectionResponse(presigned_url);
		co_return resp;
	}
	catch (const drogon::orm::UnexpectedRows& e) {
		// Not found 404
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
		resp->setBody("Video not found");
		co_return resp;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to fetch thumbnail from MinIO: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to fetch thumbnail from MinIO: " + std::string(e.what()));
		co_return resp;
	}
}

drogon::Task<drogon::HttpResponsePtr> videos::getVideoThumbnailVtt([[maybe_unused]] HttpRequestPtr req, std::string id) {
	// WebVTTファイルを取得
	drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
	try {
		auto s3Plugin = drogon::app().getPlugin<S3Plugin>();
		std::string webVTT = s3Plugin->getObject("hls/" + id + "/thumbnails.vtt");
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setBody(webVTT);
		resp->setContentTypeCode(drogon::CT_CUSTOM);
		resp->setContentTypeString("text/vtt");
		co_return resp;
	}
	catch (const drogon::orm::UnexpectedRows& e) {
		// Not found 404
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
		resp->setBody("Video not found");
		co_return resp;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to fetch thumbnail VTT from MinIO: " << e.what() << std::endl;
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		resp->setBody("Failed to fetch thumbnail VTT from MinIO: " + std::string(e.what()));
		co_return resp;
	}
}