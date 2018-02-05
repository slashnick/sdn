#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"/..
source test/venv/bin/activate
py.test -vvvv test/
