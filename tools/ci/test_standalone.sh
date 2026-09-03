#!/usr/bin/env bash

# Tests for the standalone runtime whose deps come from this repo's MODULE.bazel.
set -eux

(

	# Intended to be run from open_source/
	cd "$(dirname "$0")/../.."

	bzl test //valdi:test_standalone_http --test_output=errors

)
