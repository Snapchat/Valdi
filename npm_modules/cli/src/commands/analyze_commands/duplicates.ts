import * as fs from 'fs';
import type { Argv } from 'yargs';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { AnalysisReport } from '../../core/analyze/fingerprint';
import { resolveScanConfig, discoverTsxFiles, extractModuleName } from '../../core/analyze/scanner';
import { parseFile } from '../../core/analyze/parser';
import { clusterFingerprints } from '../../core/analyze/clusterer';
import { detectKnownPatterns } from '../../core/analyze/knownPatterns';
import { printHumanReport, formatJsonReport } from '../../core/analyze/reporter';

interface CommandParameters {
  module: string | undefined;
  json: boolean;
  threshold: number;
  pattern: string | undefined;
  output: string | undefined;
  verbose: boolean;
  'scan-root': string[] | undefined;
  'shared-lib': string[] | undefined;
}

async function analyzeDuplicates(argv: ArgumentsResolver<CommandParameters>): Promise<void> {
  const moduleFilter = argv.getArgument('module') as string | undefined;
  const json = argv.getArgument('json') as boolean;
  const threshold = argv.getArgument('threshold') as number;
  const patternFilter = argv.getArgument('pattern') as string | undefined;
  const outputPath = argv.getArgument('output') as string | undefined;
  const verbose = argv.getArgument('verbose') as boolean;
  const scanRootArg = argv.getArgument('scan-root') as string[] | undefined;
  const sharedLibArg = argv.getArgument('shared-lib') as string[] | undefined;

  const startTime = Date.now();
  const cwd = process.cwd();

  const config = resolveScanConfig(cwd, scanRootArg, sharedLibArg);

  if (!json) {
    process.stdout.write('Scanning files...');
  }

  const tsxFiles = discoverTsxFiles(config, moduleFilter);

  if (!json) {
    process.stdout.write(` found ${tsxFiles.length} .tsx files\n`);
    process.stdout.write('Fingerprinting components...');
  }

  const fingerprints = [];
  for (const file of tsxFiles) {
    const fp = parseFile(file, config);
    if (fp) fingerprints.push(fp);
  }

  if (!json) {
    process.stdout.write(` extracted ${fingerprints.length} component fingerprints\n`);
  }

  const modulesScanned = new Set(fingerprints.map(fp => fp.moduleName)).size;

  if (!json) {
    process.stdout.write('Detecting known patterns...');
  }

  const knownPatterns = detectKnownPatterns(fingerprints, config, patternFilter);

  if (!json) {
    process.stdout.write(` found ${knownPatterns.length} patterns\n`);
    process.stdout.write('Clustering similar components...');
  }

  const genericClusters = patternFilter ? [] : clusterFingerprints(fingerprints, config, threshold);

  if (!json) {
    process.stdout.write(` found ${genericClusters.length} clusters\n\n`);
  }

  const durationMs = Date.now() - startTime;

  const totalKnownPatternMatches = knownPatterns.reduce((sum, p) => sum + p.matchCount, 0);
  const totalDuplicatedFiles = new Set(genericClusters.flatMap(c => c.files.map(f => f.path))).size;

  const report: AnalysisReport = {
    timestamp: new Date().toISOString(),
    config,
    scanStats: {
      filesScanned: tsxFiles.length,
      modulesScanned,
      durationMs,
    },
    knownPatterns,
    genericClusters,
    summary: {
      totalClusters: genericClusters.length,
      totalDuplicatedFiles,
      totalKnownPatternMatches,
    },
  };

  if (outputPath) {
    fs.writeFileSync(outputPath, formatJsonReport(report), 'utf8');
    if (!json) {
      console.log(`Report written to ${outputPath}`);
    }
  }

  if (json) {
    console.log(formatJsonReport(report));
  } else {
    printHumanReport(report, verbose);
  }
}

export const command = 'duplicates';
export const describe = 'Find duplicated component patterns across Valdi modules';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('module', {
      describe: 'Scope scan to a single module',
      type: 'string',
    })
    .option('json', {
      describe: 'Machine-readable JSON output',
      type: 'boolean',
      default: false,
      alias: 'j',
    })
    .option('threshold', {
      describe: 'Similarity threshold for clustering (0-1)',
      type: 'number',
      default: 0.6,
      alias: 't',
    })
    .option('pattern', {
      describe: 'Only detect a specific known pattern: async, button, shimmer',
      type: 'string',
      choices: ['async', 'button', 'shimmer'],
    })
    .option('output', {
      describe: 'Write JSON report to file',
      type: 'string',
      alias: 'o',
    })
    .option('verbose', {
      describe: 'Show per-file detail in output',
      type: 'boolean',
      default: false,
      alias: 'v',
    })
    .option('scan-root', {
      describe: 'Root directory to scan (repeatable). Auto-detected if omitted.',
      type: 'string',
      array: true,
    })
    .option('shared-lib', {
      describe: 'Shared component library name (repeatable). Defaults: coreui (internal) or valdi_widgets (external).',
      type: 'string',
      array: true,
    });
};
export const handler = makeCommandHandler(analyzeDuplicates);
