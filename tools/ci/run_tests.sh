#!/usr/bin/env bash

set -e
set -x

# Intended to be run from open_source/

if [[ $(uname) != Linux ]] ; then
    bzl test //valdi:test
    bzl test //valdi:valdi_ios_objc_test
    bzl test //valdi:valdi_ios_swift_test
    bzl test //valdi:valdi_macos_objc_test
fi
