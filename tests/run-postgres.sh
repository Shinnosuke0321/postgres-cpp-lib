#!/usr/bin/env bash

set -e  # stop on error

echo "Deleting test data"
rm -rf /data/*

echo "Stop already-running postgres on docker"
cd docker
docker compose down --volumes

echo "Pulling the latest image and start postgres locally on docker"
docker compose pull
docker compose up -d

echo "Done."