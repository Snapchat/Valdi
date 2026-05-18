#!/usr/bin/env bash

set -e
set -u
set -x

(

# Intended to be run from open_source/
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

# High level core targets
bzl build //valdi:valdi 
bzl build //valdi_core:valdi_core

# Dummy libs
bzl build //libs/dummy:dummy
bzl build //libs/dummy:dummy_android

# Android hello world (NDK downloaded hermetically by Bazel)
bzl build //apps/helloworld:hello_world_android --define=client_repo_arm64=true

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
