#!/usr/bin/env bash

set -e  # stop on error

echo "Stop already-running postgres on docker"
docker compose down

echo "Deleting test data"
rm -rf ../data
mkdir -p ../data

echo "Pulling the latest image and start postgres locally"
docker compose pull

echo "Starting postgres"
docker compose up -d

echo "Waiting for postgres to start"
sleep 3

echo "Logging"
docker compose logs postgres