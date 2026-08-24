#!/usr/bin/env bash
#
# valdi_web Puppeteer integration test for the helloworld_playground experiment. Builds the
# exported web bundle (requires enable_web), then boots it in chrome-headless-shell, which
# @puppeteer/browsers downloads at runtime — hence the target's `requires-network` tag (exempts
# it from Bazel's test network isolation).
#
# One CI unit = one script (see AGENTS.md).

set -euxo pipefail

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

bzl test //experiments/helloworld_playground:integration_test \
    --define enable_web=true \
    --test_output=errors
