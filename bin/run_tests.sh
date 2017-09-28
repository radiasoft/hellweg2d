#!/bin/bash

set -e -x -u -o pipefail

REPO_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

docker pull radiasoft/beamsim:latest
docker run --rm -u vagrant -v "${REPO_DIR}:/rslinac" radiasoft/beamsim /rslinac/bin/test_cpp.sh
docker run --rm -v "${REPO_DIR}:/rslinac" radiasoft/beamsim /radia-run "$(id -u)" "$(id -g)" /rslinac/bin/test_python.sh
