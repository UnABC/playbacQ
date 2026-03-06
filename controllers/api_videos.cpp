#include "api_videos.h"
#include <drogon/orm/Exception.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>
#include <json/json.h>
#include <string>
#include <optional>
#include <iostream>
#include <regex>
#include "../models/Videos.h"
#include "../models/Tags.h"

using namespace api;

enum class Status {
	pending,
	processing,
	completed
};

drogon::Task<drogon::HttpResponsePtr> videos::getVideos(HttpRequestPtr req) {
	std::optional<std::string> userId = req->getOptionalParameter<std::string>("userId");
	std::optional<std::string> search = req->getOptionalParameter<std::string>("search");
	std::optional<std::string> tag = req->getOptionalParameter<std::string>("tag");
	std::optional<std::string> sortby = req->getOptionalParameter<std::string>("sortby");
	std::optional<int> order = req->getOptionalParameter<int>("order");

	drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
	drogon::orm::Criteria criteria;

	criteria = criteria && drogon::orm::Criteria(drogon_model::playbacq::Videos::Cols::_status, drogon::orm::CompareOperator::EQ, Status::completed);
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

drogon::Task<drogon::HttpResponsePtr> videos::postVideos([[maybe_unused]] HttpRequestPtr req) {
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
		newVideo.setVideoId(drogon::utils::genRandomString(11));
		newVideo.setCreatedAt(trantor::Date::now());
		newVideo.setViewCount(0);

		// JSONから動画情報を設定
		newVideo.setTitle((*jsonPtr)["title"].asString());
		newVideo.setDescription((*jsonPtr)["description"].asString());
		if (auto urlString = (*jsonPtr)["url"].asString(); urlString.empty() || !std::regex_search(urlString, std::regex(R"(^(https?|ftp)://[^\s/$.?#].[^\s]*)"))) {
			auto resp = drogon::HttpResponse::newHttpResponse();
			resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
			resp->setBody("Invalid video URL");
			co_return resp;
		} else {
			newVideo.setVideoUrl(urlString);
		}
		if (auto thumbnailUrlString = (*jsonPtr)["thumbnail_url"].asString(); thumbnailUrlString.empty() || !std::regex_search(thumbnailUrlString, std::regex(R"(^(https?|ftp)://[^\s/$.?#].[^\s]*)"))) {
			auto resp = drogon::HttpResponse::newHttpResponse();
			resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
			resp->setBody("Invalid thumbnail URL");
			co_return resp;
		} else {
			newVideo.setThumbnailUrl(thumbnailUrlString);
		}
		newVideo.setUserId(req->getAttributes()->get<std::string>("userId"));
		newVideo.setStatus(Status::pending);

		drogon::orm::CoroMapper<drogon_model::playbacq::Videos> mapper(drogon::app().getDbClient());
		co_await mapper.insert(newVideo);

		auto resp = drogon::HttpResponse::newHttpJsonResponse(newVideo.toJson());
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
		// NOt found 404
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