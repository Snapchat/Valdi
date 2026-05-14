export interface ComponentFingerprint {
  filePath: string;
  moduleName: string;
  className: string;
  viewModelProps: string[];
  stateProps: string[];
  importPaths: string[];
  sharedLibImports: string[];
  renderElements: string[];
  flags: FingerprintFlags;
}

export interface FingerprintFlags {
  hasLoadingState: boolean;
  hasErrorState: boolean;
  usesPromise: boolean;
  usesObservable: boolean;
  usesAnimationShimmer: boolean;
  isStateful: boolean;
  extendsComponent: boolean;
}

export interface ScanConfig {
  scanRoots: string[];
  sharedLibPaths: string[];
  sharedLibNames: string[];
}

export interface AnalysisReport {
  timestamp: string;
  config: ScanConfig;
  scanStats: {
    filesScanned: number;
    modulesScanned: number;
    durationMs: number;
  };
  knownPatterns: KnownPatternMatch[];
  genericClusters: GenericCluster[];
  summary: {
    totalClusters: number;
    totalDuplicatedFiles: number;
    totalKnownPatternMatches: number;
  };
}

export interface KnownPatternMatch {
  patternName: string;
  severity: 'HIGH' | 'MEDIUM' | 'LOW';
  matchCount: number;
  files: string[];
  existingImplementations: string[];
  recommendation: string;
}

export interface GenericCluster {
  id: number;
  avgSimilarity: number;
  files: Array<{ path: string; module: string; className: string }>;
  sharedViewModelProps: string[];
  sharedImports: string[];
  suggestedExtraction: string;
}
