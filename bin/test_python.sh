#!/bin/bash -l

set -x -e -u -o pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
python setup.py test
