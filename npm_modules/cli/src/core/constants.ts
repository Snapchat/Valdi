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
  LINUX = 'linux',
  CLI = 'cli',
  WEB = 'web',
}

export enum Architecture {
  ARM64,
  ARMV7,
  X86_64,
}

export const ALL_ARCHITECTURES: Architecture[] = [Architecture.ARMV7, Architecture.ARM64, Architecture.X86_64];

// Relative path starts at .metadata
export enum TEMPLATE_BASE_PATHS {
  MODULE_BAZEL = 'MODULE.bazel.template',
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
  AGENTS = 'AGENTS.md.template',
}

export const VALID_PLATFORMS: string[] = [
  PLATFORM.ANDROID,
  PLATFORM.IOS,
  PLATFORM.MACOS,
  PLATFORM.LINUX,
  PLATFORM.CLI,
  PLATFORM.WEB,
];

export interface UserConfig {
  logs_output_dir: string | undefined;
}

// TODO: Replace with Valdi defined bazel rule targets
export const IOS_BAZEL_APPLICATION_TAG = 'valdi_ios_application';
export const ANDROID_BAZEL_APPLICATION_TAG = 'valdi_android_application';
export const MACOS_BAZEL_APPLICATION_TAG = 'valdi_macos_application';
export const LINUX_BAZEL_APPLICATION_TAG = 'valdi_linux_application';
export const CLI_BAZEL_APPLICATION_TAG = 'valdi_cli_application';
export const WEB_BAZEL_APPLICATION_TAG = 'valdi_web_application';

export const IOS_EXPORTED_LIBRARY_TAG = 'valdi_ios_exported_library';
export const ANDROID_EXPORTED_LIBRARY_TAG = 'valdi_android_exported_library';
export const WEB_EXPORTED_LIBRARY_TAG = 'valdi_web_exported_library';

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

// The client_repo_* defines pick which .so goes into the AAR. They do not pick which
// architecture aar_import then requests back out of it: that comes from the target
// platform's @platforms//cpu: constraint, i.e. --android_platforms. --fat_apk_cpu and
// --android_cpu are deprecated no-ops under platform-based Android, so without an
// explicit --android_platforms the platform falls back to the host CPU. Building for an
// arm64 device on an x86_64 host then asks an arm64-only AAR for x86_64 libs and fails
// with "missing native libs for requested architecture". See Valdi#136.
export const ANDROID_ARM64_BUILD_FLAGS = ['--define', 'client_repo_arm64=true'];
export const ANDROID_ARMV7_BUILD_FLAGS = ['--define', 'client_repo_arm32=true'];
export const ANDROID_X86_64_BUILD_FLAGS = ['--define', 'client_repo_x86_64=true'];

const ANDROID_PLATFORMS: Readonly<Record<Architecture, string>> = {
  [Architecture.ARM64]: '@snap_platforms//os:android_arm64',
  [Architecture.ARMV7]: '@snap_platforms//os:android_arm32',
  [Architecture.X86_64]: '@snap_platforms//os:android_x86_64',
};

// --android_platforms takes one comma separated list. Repeating the flag overrides
// rather than accumulates, which would silently drop every architecture but the last.
export function androidPlatformsFlag(architectures: readonly Architecture[]): string {
  return `--android_platforms=${architectures.map(architecture => ANDROID_PLATFORMS[architecture]).join(',')}`;
}

// Converges the exec tool config across build and test flows so alternating
// them does not recompile shared tools. Defined as `build:stable_exec` in the
// generated .bazelrc; test inherits build configs. Applied centrally in
// BazelClient, but only when the project's .bazelrc actually defines the
// config, so projects bootstrapped by an older CLI (no such block) keep
// building unchanged instead of hard-failing on an undefined --config. Raw
// bazel keeps the default. See Valdi#137.
export const STABLE_EXEC_BUILD_FLAGS = ['--config=stable_exec'];

// The .bazelrc block that defines the config above. `bootstrap` writes it into
// new projects via the template; `projectsync` appends it to an existing
// project's .bazelrc so older projects gain the config (and then the CLI starts
// passing --config=stable_exec for them). Keep the last line in sync with the
// template's.
export const STABLE_EXEC_BAZELRC_BLOCK = `# Alternating a Valdi test target and a web/npm target trims test-only options
# differently, so their exec tool configs collide under one output path and
# recompile thousands of shared tool actions. Opt-in config that the \`valdi\`
# CLI passes to its build/test flows so they converge; raw \`bazel\` keeps the
# default. Changing test flags (e.g. --test_output) repopulates it once.
build:stable_exec --notrim_test_configuration
`;

export const ENABLE_RUNTIME_LOGS_BUILD_FLAGS = ['--@valdi//bzl/runtime_flags:enable_logging'];
export const ENABLE_RUNTIME_TRACES_BUILD_FLAGS = ['--@valdi//bzl/runtime_flags:enable_tracing'];
export const DEBUG_BUILD_FLAGS = ['--snap_flavor=platform_development'];
export const RELEASE_BUILD_FLAGS = ['--snap_flavor=production', '-c opt'];

export const IOS_DEVICE_BUILD_FLAGS = '--ios_multi_cpus=arm64';
export const IOS_BUILD_FLAGS = '--repo_env=VALDI_PLATFORM_DEPENDENCIES=ios';

const INLINE_ASSETS_BUILD_FLAGS = ['--@valdi//bzl/valdi:assets_mode=inline'];

export const MACOS_BUILD_FLAGS = [...INLINE_ASSETS_BUILD_FLAGS, '--repo_env=VALDI_PLATFORM_DEPENDENCIES=macos'];
export const LINUX_BUILD_FLAGS = [...INLINE_ASSETS_BUILD_FLAGS, '--repo_env=VALDI_PLATFORM_DEPENDENCIES=linux'];
export const CLI_BUILD_FLAGS = [...INLINE_ASSETS_BUILD_FLAGS, '--repo_env=VALDI_PLATFORM_DEPENDENCIES=cli'];
export const WEB_BUILD_FLAGS = ['--define', 'enable_web=true', ...INLINE_ASSETS_BUILD_FLAGS];
export const TEST_BUILD_FLAGS = INLINE_ASSETS_BUILD_FLAGS;
