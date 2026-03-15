#pragma once
#include <drogon/WebSocketController.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <string>

class CommentController : public drogon::WebSocketController<CommentController>
{
private:
  // 部屋割りを管理するデータ構造
  static std::mutex mtx_;
  // video_id -> その動画を見ているユーザーの接続(コネクション)の集合
  static std::unordered_map<std::string, std::unordered_set<drogon::WebSocketConnectionPtr>> rooms;
public:
  static void broadcastToRoom(const std::string& video_id, const std::string& message);
  
  void handleNewMessage(const drogon::WebSocketConnectionPtr& wsConnPtr,
    std::string&& message,
    const drogon::WebSocketMessageType& type) override;

  void handleNewConnection(const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& wsConnPtr) override;

  void handleConnectionClosed(const drogon::WebSocketConnectionPtr& wsConnPtr) override;

  WS_PATH_LIST_BEGIN
    // エンドポイントの設定。今回はクエリパラメータ(?video_id=xxx)で動画IDを受け取る想定
    WS_PATH_ADD("/ws/comments");
  WS_PATH_LIST_END
};