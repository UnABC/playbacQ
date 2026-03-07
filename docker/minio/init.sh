#!/bin/sh

echo "Waiting for MinIO to start..."
until mc alias set myminio http://localhost:9000 ${MINIO_ROOT_USER} ${MINIO_ROOT_PASSWORD}; do
  echo "MinIO is not available yet. Retrying..."
  sleep 3
done

echo "Successfully connected to MinIO. Creating bucket..."

mc mb myminio/videos --ignore-existing
mc event add myminio/videos arn::minio::sqs::playbacq:webhook --event put

echo "Bucket created and event notification set up."