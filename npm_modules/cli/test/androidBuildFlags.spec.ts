import { Architecture, PLATFORM, androidPlatformsFlag } from '../src/core/constants';
import { resolveBazelBuildArgs } from '../src/utils/applicationUtils';

function androidArgs(architectures: Architecture[]): string {
  return resolveBazelBuildArgs(PLATFORM.ANDROID, 'debug', architectures, true, false, false, '');
}

describe('androidPlatformsFlag', () => {
  it('maps each architecture to its snap_platforms label', () => {
    expect(androidPlatformsFlag([Architecture.ARM64])).toBe('--android_platforms=@snap_platforms//os:android_arm64');
    expect(androidPlatformsFlag([Architecture.ARMV7])).toBe('--android_platforms=@snap_platforms//os:android_arm32');
    expect(androidPlatformsFlag([Architecture.X86_64])).toBe('--android_platforms=@snap_platforms//os:android_x86_64');
  });

  it('joins multiple architectures into one flag', () => {
    // --android_platforms takes a comma separated list. Repeating the flag would
    // override rather than accumulate, dropping every architecture but the last.
    expect(androidPlatformsFlag([Architecture.ARM64, Architecture.X86_64])).toBe(
      '--android_platforms=@snap_platforms//os:android_arm64,@snap_platforms//os:android_x86_64',
    );
  });
});

describe('resolveBazelBuildArgs for Android', () => {
  it('sets the target platform so aar_import requests the architecture that was built', () => {
    // Without this, the platform falls back to the host CPU and an arm64 device build
    // on an x86_64 host asks an arm64-only AAR for x86_64 libs. See Valdi#136.
    const args = androidArgs([Architecture.ARM64]);

    expect(args).toContain('--define client_repo_arm64=true');
    expect(args).toContain('--android_platforms=@snap_platforms//os:android_arm64');
  });

  it('emits a single android_platforms flag for a multi architecture build', () => {
    const args = androidArgs([Architecture.ARM64, Architecture.X86_64]);

    expect(args).toContain('--define client_repo_arm64=true');
    expect(args).toContain('--define client_repo_x86_64=true');
    expect(args.match(/--android_platforms=/g)?.length).toBe(1);
  });

  it('drops the deprecated legacy cpu flags', () => {
    // Bazel treats both as no-ops under platform-based Android and warns on them.
    const args = androidArgs([Architecture.ARM64]);

    expect(args).not.toContain('--fat_apk_cpu');
    expect(args).not.toContain('--android_cpu');
  });

  it('omits android_platforms when no architecture is requested', () => {
    expect(androidArgs([])).not.toContain('--android_platforms');
  });
});
