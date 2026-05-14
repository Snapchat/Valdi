import type { ComponentFingerprint, KnownPatternMatch, ScanConfig } from './fingerprint';
import { isSharedLibFile } from './scanner';

type PatternDetector = (fingerprints: ComponentFingerprint[], config: ScanConfig) => KnownPatternMatch | undefined;

const DETECTORS: Record<string, PatternDetector> = {
  async: detectAsyncStatePattern,
  button: detectButtonWrapperPattern,
  shimmer: detectShimmerPattern,
};

export function detectKnownPatterns(
  fingerprints: ComponentFingerprint[],
  config: ScanConfig,
  patternFilter?: string,
): KnownPatternMatch[] {
  const nonShared = fingerprints.filter(fp => !isSharedLibFile(fp.filePath, config));
  const results: KnownPatternMatch[] = [];

  const detectorsToRun = patternFilter ? { [patternFilter]: DETECTORS[patternFilter] } : DETECTORS;

  for (const [name, detector] of Object.entries(detectorsToRun)) {
    if (!detector) continue;
    const match = detector(nonShared, config);
    if (match) results.push(match);
  }

  return results.sort((a, b) => severityRank(a.severity) - severityRank(b.severity));
}

function severityRank(s: 'HIGH' | 'MEDIUM' | 'LOW'): number {
  return s === 'HIGH' ? 0 : s === 'MEDIUM' ? 1 : 2;
}

function detectAsyncStatePattern(
  fingerprints: ComponentFingerprint[],
  _config: ScanConfig,
): KnownPatternMatch | undefined {
  const loadingOnly = fingerprints.filter(fp => fp.flags.hasLoadingState);

  const existingImpls = fingerprints.filter(
    fp => fp.className.includes('AsyncContentContainer') || fp.className.includes('AsyncStateContainer'),
  );

  if (loadingOnly.length < 3) return undefined;

  return {
    patternName: 'Async State Container',
    severity: loadingOnly.length >= 50 ? 'HIGH' : loadingOnly.length >= 15 ? 'MEDIUM' : 'LOW',
    matchCount: loadingOnly.length,
    files: loadingOnly.map(fp => fp.filePath),
    existingImplementations: existingImpls.map(fp => fp.filePath),
    recommendation:
      `${loadingOnly.length} components implement loading/error/content state machines. ` +
      (existingImpls.length > 0
        ? `${existingImpls.length} shared implementation(s) already exist — consolidate into one and migrate consumers.`
        : `Extract a generic AsyncStateContainer to the shared component library.`),
  };
}

function detectButtonWrapperPattern(
  fingerprints: ComponentFingerprint[],
  _config: ScanConfig,
): KnownPatternMatch | undefined {
  const buttonComponentNames = ['CoreButton', 'PillButton'];

  const matches = fingerprints.filter(fp => {
    const importsButton =
      fp.sharedLibImports.some(imp => buttonComponentNames.some(name => imp.includes(name))) ||
      fp.importPaths.some(imp => buttonComponentNames.some(name => imp.includes(name)));

    const isWrapper = fp.renderElements.length <= 5 && fp.className.toLowerCase().includes('button');

    return importsButton && isWrapper;
  });

  if (matches.length < 5) return undefined;

  return {
    patternName: 'Button Wrapper',
    severity: matches.length >= 30 ? 'HIGH' : matches.length >= 10 ? 'MEDIUM' : 'LOW',
    matchCount: matches.length,
    files: matches.map(fp => fp.filePath),
    existingImplementations: [],
    recommendation: `${matches.length} thin button wrapper components found. Add preset enums to the shared button component instead of per-module wrappers.`,
  };
}

function detectShimmerPattern(
  fingerprints: ComponentFingerprint[],
  _config: ScanConfig,
): KnownPatternMatch | undefined {
  const matches = fingerprints.filter(
    fp =>
      fp.flags.usesAnimationShimmer ||
      fp.className.toLowerCase().includes('shimmer') ||
      fp.className.toLowerCase().includes('skeleton'),
  );

  if (matches.length < 3) return undefined;

  return {
    patternName: 'Shimmer / Skeleton Layout',
    severity: matches.length >= 20 ? 'HIGH' : matches.length >= 8 ? 'MEDIUM' : 'LOW',
    matchCount: matches.length,
    files: matches.map(fp => fp.filePath),
    existingImplementations: [],
    recommendation: `${matches.length} shimmer/skeleton components found. Extract a configurable ShimmerLayout to the shared component library.`,
  };
}
