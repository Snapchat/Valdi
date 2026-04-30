base_dir: .
project_name: valdi_snapchat

ignored_files:
  - proto_config.json
  - proto_config.yaml
  - tslint.json
  - unicode_script.ts
  - OWNERS
  - README.md
  - \.DS_Store
  - package.json
  - package-lock.json
  - settings\.json
  - yarn.lock
  - .*\.proto
  - .*\.bzl

ios:
  codegen_enabled: true
  default_module_name_prefix: SCC
  default_language: objc
  output:
    base: {IOS_OUTPUT_BASE}
    debug_path: debug
    release_path: release
    metadata_path: metadata

android:
  class_path: com.snap.modules
  output:
    base: {ANDROID_OUTPUT_BASE}
    debug_path: debug
    release_path: release
    metadata_path: '.'

cpp:
  codegen_enabled: true
  default_class_prefix: 'snap::valdi_modules::'
  import_path_prefix: 'valdi_modules/'
  output:
    base: cpp
    debug_path: debug
    release_path: release
    metadata_path: '.'


compiler_companion_binary: {COMPANION_BINARY}

compiler_toolbox_path:
  linux: {COMPILER_TOOLBOX_PATH}
  macos: {COMPILER_TOOLBOX_PATH}

pngquant_bin_path:
  linux: {PNGQUANT_PATH}
  macos: {PNGQUANT_PATH}

minify_config_path: {MINIFY_CONFIG_PATH}

node_modules_dir: {NODE_MODULES_DIR}
node_modules_target: {NODE_MODULES_TARGET}
node_modules_workspace: {NODE_MODULES_WORKSPACE}
external_modules_target: {EXTERNAL_MODULES_TARGET}
external_modules_workspace: {EXTERNAL_MODULES_WORKSPACE}
