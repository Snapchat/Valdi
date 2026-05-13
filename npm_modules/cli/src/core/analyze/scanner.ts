import * as fs from 'fs';
import * as path from 'path';
import type { ScanConfig } from './fingerprint';

const INTERNAL_SCAN_ROOT = 'src/composer_modules/src/composer';
const INTERNAL_SHARED_LIBS = ['coreui'];
const EXTERNAL_SHARED_LIBS = ['valdi_widgets'];

export function resolveScanConfig(
  cwd: string,
  scanRootOverrides?: string[],
  sharedLibOverrides?: string[],
): ScanConfig {
  if (scanRootOverrides && scanRootOverrides.length > 0) {
    const resolvedRoots = scanRootOverrides.map(r => path.resolve(cwd, r));
    const libNames = sharedLibOverrides ?? EXTERNAL_SHARED_LIBS;
    const libPaths = libNames.map(name => discoverSharedLibPath(cwd, resolvedRoots, name));
    return {
      scanRoots: resolvedRoots,
      sharedLibPaths: libPaths.filter((p): p is string => p !== undefined),
      sharedLibNames: libNames,
    };
  }

  const internalRoot = path.join(cwd, INTERNAL_SCAN_ROOT);
  if (fs.existsSync(internalRoot)) {
    const libNames = sharedLibOverrides ?? INTERNAL_SHARED_LIBS;
    const libPaths = libNames.map(name => discoverSharedLibPath(cwd, [internalRoot], name));
    return {
      scanRoots: [internalRoot],
      sharedLibPaths: libPaths.filter((p): p is string => p !== undefined),
      sharedLibNames: libNames,
    };
  }

  const srcDir = path.join(cwd, 'src');
  if (fs.existsSync(srcDir)) {
    const libNames = sharedLibOverrides ?? EXTERNAL_SHARED_LIBS;
    const libPaths = libNames.map(name => discoverSharedLibPath(cwd, [srcDir], name));
    return {
      scanRoots: [srcDir],
      sharedLibPaths: libPaths.filter((p): p is string => p !== undefined),
      sharedLibNames: libNames,
    };
  }

  const libNames = sharedLibOverrides ?? EXTERNAL_SHARED_LIBS;
  const libPaths = libNames.map(name => discoverSharedLibPath(cwd, [cwd], name));
  return {
    scanRoots: [cwd],
    sharedLibPaths: libPaths.filter((p): p is string => p !== undefined),
    sharedLibNames: libNames,
  };
}

function discoverSharedLibPath(workspaceRoot: string, scanRoots: string[], libName: string): string | undefined {
  // 1. Direct child of a scan root
  for (const root of scanRoots) {
    const candidate = path.join(root, libName);
    if (fs.existsSync(candidate)) return candidate;
  }

  // 2. Sibling directory of the workspace (separate git checkout)
  //    e.g. workspace is ~/projects/my_app, look for ~/projects/valdi_widgets
  const parentDir = path.dirname(workspaceRoot);
  const siblingCandidates = [
    libName,
    libName
      .split('_')
      .map(s => s.charAt(0).toUpperCase() + s.slice(1))
      .join('_'),
    libName.replace(/_/g, '-'),
    libName.replace(/_([a-z])/g, (_, c: string) => c.toUpperCase()),
  ];
  for (const name of siblingCandidates) {
    const candidate = path.join(parentDir, name);
    if (fs.existsSync(candidate)) return candidate;
  }

  // 3. Bazel external deps (bzlmod: bazel-<workspace>/external/*+<name>*)
  const bazelSymlinks = findBazelWorkspaceSymlinks(workspaceRoot);
  for (const bazelDir of bazelSymlinks) {
    const externalDir = path.join(bazelDir, 'external');
    if (!fs.existsSync(externalDir)) continue;
    try {
      const entries = fs.readdirSync(externalDir);
      for (const entry of entries) {
        if (entry.includes(libName)) {
          const candidate = path.join(externalDir, entry);
          if (fs.statSync(candidate).isDirectory()) return candidate;
        }
      }
    } catch {
      // permission or symlink errors
    }
  }

  // 4. node_modules
  const nodeModules = path.join(workspaceRoot, 'node_modules', libName);
  if (fs.existsSync(nodeModules)) return nodeModules;

  return undefined;
}

function findBazelWorkspaceSymlinks(workspaceRoot: string): string[] {
  const results: string[] = [];
  try {
    const entries = fs.readdirSync(workspaceRoot);
    for (const entry of entries) {
      if (
        entry.startsWith('bazel-') &&
        !entry.startsWith('bazel-out') &&
        !entry.startsWith('bazel-bin') &&
        !entry.startsWith('bazel-testlogs')
      ) {
        const fullPath = path.join(workspaceRoot, entry);
        try {
          if (fs.lstatSync(fullPath).isSymbolicLink() || fs.statSync(fullPath).isDirectory()) {
            results.push(fullPath);
          }
        } catch {
          // broken symlink
        }
      }
    }
  } catch {
    // can't read workspace root
  }
  return results;
}

export function discoverTsxFiles(config: ScanConfig, moduleFilter?: string): string[] {
  const files: string[] = [];

  for (const root of config.scanRoots) {
    if (!fs.existsSync(root)) continue;
    walkDir(root, config, moduleFilter, files);
  }

  return files;
}

function walkDir(dir: string, config: ScanConfig, moduleFilter: string | undefined, out: string[]): void {
  let entries: fs.Dirent[];
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch {
    return;
  }

  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);

    if (entry.isDirectory()) {
      if (entry.name === 'node_modules' || entry.name === '.git' || entry.name === 'bazel-out') continue;

      if (moduleFilter) {
        const moduleName = extractModuleName(fullPath, config);
        if (moduleName && moduleName !== moduleFilter) continue;
      }

      walkDir(fullPath, config, moduleFilter, out);
    } else if (entry.name.endsWith('.tsx') && !entry.name.endsWith('.spec.tsx') && !entry.name.endsWith('.test.tsx')) {
      if (!isInTestDir(fullPath)) {
        out.push(fullPath);
      }
    }
  }
}

function isInTestDir(filePath: string): boolean {
  const parts = filePath.split(path.sep);
  return parts.includes('test') || parts.includes('__tests__');
}

export function extractModuleName(filePath: string, config: ScanConfig): string | undefined {
  for (const root of config.scanRoots) {
    if (filePath === root || filePath.startsWith(root + path.sep)) {
      const relative = path.relative(root, filePath);
      const firstSegment = relative.split(path.sep)[0];
      return firstSegment;
    }
  }
  return undefined;
}

export function isSharedLibFile(filePath: string, config: ScanConfig): boolean {
  return config.sharedLibPaths.some(libPath => filePath === libPath || filePath.startsWith(libPath + path.sep));
}
