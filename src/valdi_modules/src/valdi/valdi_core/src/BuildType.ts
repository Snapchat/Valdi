import { getValdiRuntime } from './ValdiRuntimeProvider';

export enum BuildType {
  Dev = 'dev',
  Gold = 'gold',
  Alpha = 'alpha',
  Prod = 'prod',
}

const runtime = getValdiRuntime();

/**
 * @returns The BuildType that the Valdi runtime was compiled in.
 */
export function getBuildType(): BuildType {
  const buildType = runtime.buildType as BuildType;
  if (buildType === undefined) {
    return BuildType.Dev;
  }
  return buildType;
}

/**
 * Returns whether the Valdi runtime was compiled on a production like build.
 */
export function isProdBuild(): boolean {
  const buildType = getBuildType();
  return buildType == BuildType.Prod || buildType == BuildType.Alpha;
}

/**
 * Returns whether the Valdi runtime was compiled on a development build.
 */
export function isDevBuild(): boolean {
  return !isProdBuild();
}

export function isInternalBuild(): boolean {
  const buildType = getBuildType();
  return buildType == BuildType.Dev || buildType == BuildType.Gold || buildType == BuildType.Alpha;
}
