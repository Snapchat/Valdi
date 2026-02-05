import path from 'path';

export const VALDI_CONFIG_PATHS: string[] = ['~/.valdi/config.yaml', '~/.valdi/config.yml'];
export const BAZEL_BIN_ENV = 'BAZEL_BIN';
export const BAZEL_EXECUTABLES: string[] = ['bazel', 'bzl', 'bazelisk'];

// Console color ANSI escape sequences
export enum ANSI_COLORS {
  RESET_COLOR = '\u001B[0m',
  RED_COLOR = '\u001B[31m',
  GREEN_COLOR = '\u001B[32m',
  YELLOW_COLOR = '\u001B[33m',
  BLUE_COLOR = '\u001B[34m',
  GRAY_COLOR = '\u001B[90m',
}

export enum PLATFORM {
  IOS = 'ios',
  ANDROID = 'android',
  MACOS = 'macos',
  CLI = 'cli',
}

export enum Architecture {
  ARM64,
  ARMV7,
  X86_64,
}

export const ALL_ARCHITECTURES: Architecture[] = [Architecture.ARMV7, Architecture.ARM64, Architecture.X86_64];

// Relative path starts at .metadata
export enum TEMPLATE_BASE_PATHS {
  WORKSPACE = 'WORKSPACE.template',
  BAZEL_RC = '.bazelrc.template',
  BAZEL_VERSION = '.bazelversion.template',
  USER_CONFIG = 'config.yaml.template',
  PRETTIER_CONFIG = '.prettierrc.json.template',
  ESLINT_CONFIG = '.eslintrc.js.template',
  ESLINT_PACKAGE_JSON_CONFIG = 'package.json.template',
  README = 'README.md.template',
  WATCHMAN_CONFIG = '.watchmanconfig.template',
  GIT_IGNORE = '.gitignore.template',
  EDITOR_CONFIG = '.editorconfig.template',
  CURSOR_RULES = '.cursorrules.template',
}

export const VALID_PLATFORMS: string[] = [PLATFORM.ANDROID, PLATFORM.IOS, PLATFORM.MACOS, PLATFORM.CLI];

export interface UserConfig {
  logs_output_dir: string | undefined;
}

// TODO: Replace with Valdi defined bazel rule targets
export const IOS_BAZEL_APPLICATION_TAG = 'valdi_ios_application';
export const ANDROID_BAZEL_APPLICATION_TAG = 'valdi_android_application';
export const MACOS_BAZEL_APPLICATION_TAG = 'valdi_macos_application';
export const CLI_BAZEL_APPLICATION_TAG = 'valdi_cli_application';

export const IOS_EXPORTED_LIBRARY_TAG = 'valdi_ios_exported_library';
export const ANDROID_EXPORTED_LIBRARY_TAG = 'valdi_android_exported_library';

// Paths
// eslint-disable-next-line unicorn/prefer-module
export const CLI_ROOT = path.join(__dirname, '../..');
export const CONFIG_DIR_PATH = path.join(CLI_ROOT, '.config');
export const META_DIR_PATH = path.join(CLI_ROOT, '.metadata');
export const BOOTSTRAP_DIR_PATH = path.join(CLI_ROOT, '.bootstrap');
export const SETUP_SCRIPT_DIR_PATH = path.join(CLI_ROOT, 'src/setup');
export const MACOS_SETUP_SCRIPT_DIR_PATH = path.join(SETUP_SCRIPT_DIR_PATH, 'macos');
export const LINUX_SETUP_SCRIPT_DIR_PATH = path.join(SETUP_SCRIPT_DIR_PATH, 'linux');

export const COPY_CONFIG_PATH = path.join(CONFIG_DIR_PATH, 'copyconfig.json');

export const ANDROID_BUILD_FLAGS = ['--copt=-DANDROID_WITH_JNI', '--repo_env=VALDI_PLATFORM_DEPENDENCIES=android'];

function androidCpuFlags(cpu: string): readonly string[] {
  return [`--fat_apk_cpu=${cpu}`, `--android_cpu=${cpu}`];
}

export const ANDROID_ARM64_BUILD_FLAGS = ['--define', 'client_repo_arm64=true', ...androidCpuFlags('arm64-v8a')];
export const ANDROID_ARMV7_BUILD_FLAGS = ['--define', 'client_repo_arm32=true', ...androidCpuFlags('armeabi-v7a')];
export const ANDROID_X86_64_BUILD_FLAGS = ['--define', 'client_repo_x86_64=true', ...androidCpuFlags('x86_64')];

export const ENABLE_RUNTIME_LOGS_BUILD_FLAGS = ['--@valdi//bzl/runtime_flags:enable_logging'];
export const ENABLE_RUNTIME_TRACES_BUILD_FLAGS = ['--@valdi//bzl/runtime_flags:enable_tracing'];
export const DEBUG_BUILD_FLAGS = ['--snap_flavor=platform_development'];
export const RELEASE_BUILD_FLAGS = ['--snap_flavor=production', '-c opt'];

export const IOS_DEVICE_BUILD_FLAGS = '--ios_multi_cpus=arm64';
export const IOS_BUILD_FLAGS = '--repo_env=VALDI_PLATFORM_DEPENDENCIES=ios';

const INLINE_ASSETS_BUILD_FLAGS = ['--@valdi//bzl/valdi:assets_mode=inline'];

export const MACOS_BUILD_FLAGS = [...INLINE_ASSETS_BUILD_FLAGS, '--repo_env=VALDI_PLATFORM_DEPENDENCIES=macos'];
export const CLI_BUILD_FLAGS = [...INLINE_ASSETS_BUILD_FLAGS, '--repo_env=VALDI_PLATFORM_DEPENDENCIES=cli'];
export const TEST_BUILD_FLAGS = INLINE_ASSETS_BUILD_FLAGS;
