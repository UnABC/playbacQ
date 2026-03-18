# Playbacq
動画共有サービス
・フロントエンド：https://github.com/UnABC/playbacQ-UI

## 技術スタック
### Frontend
* **Framework**: Angular 19+
* **UI Library**: Angular Material
* **Video Player**: Plyr / HLS.js
* **Reactive Programming**: RxJS

### Backend
* **Language**: C++23
* **Framework**: Drogon (C++ Web Framework)
* **Storage**: MinIO (S3 Compatible Object Storage)
* **Database**: MySQL / Redis (View Count Caching & Job Queue)
* **Others**: AWS SDK for C++ (S3 Plugin)

## 環境構築
docker compose up -d

## 実行
mkdir build
cd build
cmake ..
make
./playbacq

または

VSCodeでCMake Toolsを使用してビルド+実行。
