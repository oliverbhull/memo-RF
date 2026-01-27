#!/bin/bash
# Quick start script for GPT-OSS 20B model

cd "$(dirname "$0")/.."
./scripts/start_server.sh gpt-oss "$@"
