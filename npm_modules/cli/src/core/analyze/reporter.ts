import { ANSI_COLORS } from '../constants';
import { wrapInColor } from '../../utils/logUtils';
import type { AnalysisReport, GenericCluster, KnownPatternMatch } from './fingerprint';

export function printHumanReport(report: AnalysisReport, verbose: boolean): void {
  console.log(wrapInColor('Valdi Component Duplication Report', ANSI_COLORS.BLUE_COLOR));
  console.log('='.repeat(50));
  console.log();

  const { scanStats } = report;
  console.log(
    `Scanned ${wrapInColor(String(scanStats.filesScanned), ANSI_COLORS.GREEN_COLOR)} files across ${wrapInColor(String(scanStats.modulesScanned), ANSI_COLORS.GREEN_COLOR)} modules in ${scanStats.durationMs}ms`,
  );
  console.log(`Scan roots: ${report.config.scanRoots.join(', ')}`);
  console.log(`Shared libraries: ${report.config.sharedLibNames.join(', ') || '(none configured)'}`);
  console.log();

  if (report.knownPatterns.length > 0) {
    console.log(wrapInColor('Known Patterns', ANSI_COLORS.BLUE_COLOR));
    console.log('-'.repeat(30));
    for (const pattern of report.knownPatterns) {
      printKnownPattern(pattern, verbose);
    }
    console.log();
  }

  if (report.genericClusters.length > 0) {
    console.log(wrapInColor('Similar Component Clusters', ANSI_COLORS.BLUE_COLOR));
    console.log('-'.repeat(30));
    const clustersToShow = verbose ? report.genericClusters : report.genericClusters.slice(0, 10);
    for (const cluster of clustersToShow) {
      printCluster(cluster, verbose);
    }
    if (!verbose && report.genericClusters.length > 10) {
      console.log(
        wrapInColor(
          `  ... and ${report.genericClusters.length - 10} more clusters (use --verbose to see all)`,
          ANSI_COLORS.GRAY_COLOR,
        ),
      );
    }
    console.log();
  }

  console.log('='.repeat(50));
  console.log(wrapInColor('Summary', ANSI_COLORS.BLUE_COLOR));
  console.log(`  Known pattern matches: ${report.summary.totalKnownPatternMatches}`);
  console.log(`  Similar clusters: ${report.summary.totalClusters}`);
  console.log(`  Total duplicated files: ${report.summary.totalDuplicatedFiles}`);
}

function printKnownPattern(pattern: KnownPatternMatch, verbose: boolean): void {
  const severityColor =
    pattern.severity === 'HIGH'
      ? ANSI_COLORS.RED_COLOR
      : pattern.severity === 'MEDIUM'
        ? ANSI_COLORS.YELLOW_COLOR
        : ANSI_COLORS.GRAY_COLOR;

  console.log(
    `  ${wrapInColor(`[${pattern.severity}]`, severityColor)} ${pattern.patternName} — ${pattern.matchCount} matches`,
  );
  console.log(`    ${pattern.recommendation}`);

  if (pattern.existingImplementations.length > 0) {
    console.log(`    Existing implementations:`);
    for (const impl of pattern.existingImplementations) {
      console.log(`      ${wrapInColor('→', ANSI_COLORS.GREEN_COLOR)} ${impl}`);
    }
  }

  if (verbose) {
    const filesToShow = pattern.files.slice(0, 20);
    for (const file of filesToShow) {
      console.log(`    ${wrapInColor('·', ANSI_COLORS.GRAY_COLOR)} ${file}`);
    }
    if (pattern.files.length > 20) {
      console.log(wrapInColor(`    ... and ${pattern.files.length - 20} more`, ANSI_COLORS.GRAY_COLOR));
    }
  }
  console.log();
}

function printCluster(cluster: GenericCluster, verbose: boolean): void {
  console.log(
    `  Cluster #${cluster.id} (${cluster.files.length} files, avg similarity: ${(cluster.avgSimilarity * 100).toFixed(0)}%)`,
  );
  console.log(`    ${cluster.suggestedExtraction}`);

  if (cluster.sharedViewModelProps.length > 0) {
    console.log(`    Shared ViewModel props: ${cluster.sharedViewModelProps.join(', ')}`);
  }

  for (const file of cluster.files) {
    console.log(`    ${wrapInColor('·', ANSI_COLORS.GRAY_COLOR)} ${file.module}/${file.className} — ${file.path}`);
  }

  if (verbose && cluster.sharedImports.length > 0) {
    console.log(`    Shared imports: ${cluster.sharedImports.slice(0, 5).join(', ')}`);
  }
  console.log();
}

export function formatJsonReport(report: AnalysisReport): string {
  return JSON.stringify(report, null, 2);
}
