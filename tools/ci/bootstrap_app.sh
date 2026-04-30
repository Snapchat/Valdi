#!/usr/bin/env bash

set -e
set -x

# Should be run after cli is installed

# We're not installing the ios_webkit_debug_proxy and we're not runnig any simulators at this point
# However we want the valdi doctor command to believe there is an ios_webkit_debug_proxy
mkdir -p ~/bin
export PATH=$HOME/bin:$PATH
touch ~/bin/ios_webkit_debug_proxy
chmod +x ~/bin/ios_webkit_debug_proxy
which ios_webkit_debug_proxy


OPEN_SOURCE_DIR="$(pwd)"
APP_DIR="/tmp/valdi_app"

# Make sure the targets build
if [[ $(uname) != Linux ]] ; then
    pushd "$OPEN_SOURCE_DIR/npm_modules/cli"
    OPEN_SOURCE_DIR=$OPEN_SOURCE_DIR PROJECT_ROOT=$APP_DIR node -r ts-node/register --test "test/**/*test.ts"
    popd
fi
