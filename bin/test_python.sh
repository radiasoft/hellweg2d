#!/bin/bash -l

set -x -e -u -o pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
ls -aln .
ls -aln ~
id
mkdir /home/vagrant/.cache
py.test
