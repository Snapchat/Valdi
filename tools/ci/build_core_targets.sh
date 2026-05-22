#!/usr/bin/env bash

set -e
set -u
set -x

(

# Intended to be run from open_source/
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

# Build all core targets in a single invocation to avoid repeated Bazel startup
# and allow maximum parallelism across the dependency graph.
bzl build \
  //valdi:valdi \
  //valdi_core:valdi_core \
  //libs/dummy:dummy \
  //libs/dummy:dummy_android

# Android hello world AAR (NDK downloaded hermetically by Bazel)
# Build just the AAR, not the full android_binary — aar_import filters for
# host arch (x86_64 on Linux CI) which won't match the arm64-only native libs.
bzl build //apps/helloworld:hello_world_android_aar --define=client_repo_arm64=true

if [[ $(uname) != Linux ]] ; then
    # Hello world apps (Apple platforms — macOS only)
    # Pre-fetch primary dependencies, limiting threads to reduce memory usage
    bzl fetch //apps/helloworld:hello_world_ios --loading_phase_threads=4
    bzl build //apps/helloworld:hello_world_ios
    bzl fetch //apps/helloworld:hello_world_macos --loading_phase_threads=4
    bzl build //apps/helloworld:hello_world_macos

    # Swift xcframework export (validates ios_swift = True path)
    bzl build //apps/helloworld:hello_world_swift_export_ios
fi

)
