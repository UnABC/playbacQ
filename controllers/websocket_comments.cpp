#include "websocket_comments.h"
#include <iostream>

std::mutex CommentController::mtx_;
std::unordered_map<std::string, std::unordered_set<drogon::WebSocketConnectionPtr>> CommentController::rooms;

void CommentController::broadcastToRoom(const std::string& video_id, const std::string& message)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = rooms.find(video_id);
    if (it != rooms.end()) {
        // その部屋にいる全員(コネクション)に対して send() を叩く
        for (auto& conn : it->second) {
            conn->send(message);
        }
    }
}

void CommentController::handleNewConnection(const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& wsConnPtr)
{
    // URLのクエリからvideo_idを取得 (例：?video_id=Pa4Rme3yeZX)
    std::string video_id = req->getParameter("video_id");
    if (video_id.empty()) {
        wsConnPtr->forceClose(); // IDがなければ強制切断
        return;
    }

    // コネクションに「自分はどの部屋にいるか」を記憶させる
    wsConnPtr->setContext(std::make_shared<std::string>(video_id));

    {
        std::lock_guard<std::mutex> lock(mtx_);
        rooms[video_id].insert(wsConnPtr);
    }

    std::cout << "New user joined room: " << video_id << std::endl;
}

void CommentController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& wsConnPtr)
{
    // このユーザーがどの部屋にいたか、コンテキストを取り出す
    if (wsConnPtr->hasContext()) {
        auto video_id_ptr = wsConnPtr->getContext<std::string>();
        std::string video_id = *video_id_ptr;

        // 部屋からユーザーを削除する
        {
            std::lock_guard<std::mutex> lock(mtx_);
            rooms[video_id].erase(wsConnPtr);
            if (rooms[video_id].empty()) {
                rooms.erase(video_id);
            }
        }
        std::cout << "User left room: " << video_id << std::endl;
    }
}

void CommentController::handleNewMessage(const drogon::WebSocketConnectionPtr& wsConnPtr,
    [[maybe_unused]] std::string&& message,
    [[maybe_unused]] const drogon::WebSocketMessageType& type)
{
    wsConnPtr->forceClose();
}