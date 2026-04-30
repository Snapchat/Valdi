import fs from 'fs';
import path from 'path';
import type { Argv } from 'yargs';
import { ANSI_COLORS, MACOS_BUILD_FLAGS } from '../core/constants';
import { CliError } from '../core/errors';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import { makeCommandHandler } from '../utils/errorUtils';
import { wrapInColor } from '../utils/logUtils';
import {
  computeModuleLabel,
  findModuleDir,
  generatePreviewFiles,
  getPreviewDir,
  isValdiWorkspace,
} from '../preview/previewGenerator';
import { parseComponentFile } from '../preview/viewModelParser';

interface CommandParameters {
  component: string;
}

/**
 * Resolve a component argument to an absolute .tsx file path.
 * Accepts:
 *   - A file path (absolute or relative): ./valdi_modules/widgets/src/.../Checkbox.tsx
 *   - An import path: widgets/src/components/button/Checkbox
 */
function resolveComponentFile(component: string, workspaceRoot: string): string {
  // If it already looks like a file path
  if (component.endsWith('.tsx') || component.endsWith('.ts')) {
    const abs = path.resolve(component);
    if (fs.existsSync(abs)) return abs;
    throw new CliError(`Component file not found: ${abs}`);
  }

  // Treat as an import path — search common module directories
  const candidates = [
    // External repos: valdi_modules/<module>/...
    path.join(workspaceRoot, 'valdi_modules', component + '.tsx'),
    // Valdi repo: src/valdi_modules/src/valdi/<module>/...
    path.join(workspaceRoot, 'src/valdi_modules/src/valdi', component + '.tsx'),
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) return candidate;
  }

  throw new CliError(
    `Could not find component file for "${component}". Searched:\n` +
      candidates.map(c => `  ${c}`).join('\n'),
  );
}

/**
 * Compute the import path from a resolved file path.
 * E.g. /path/to/valdi_modules/widgets/src/components/button/Checkbox.tsx
 *   → widgets/src/components/button/Checkbox
 */
function computeImportPath(filePath: string, moduleDir: string): string {
  const moduleName = path.basename(moduleDir);
  const relFromModule = path.relative(moduleDir, filePath);
  // Remove .tsx extension
  const withoutExt = relFromModule.replace(/\.tsx?$/, '');
  return `${moduleName}/${withoutExt}`;
}

async function valdiPreview(argv: ArgumentsResolver<CommandParameters>) {
  const bazel = new BazelClient();
  const workspaceRoot = await bazel.getWorkspaceRoot();
  const component = argv.getArgument('component') as string;

  // 1. Resolve the component file
  console.log(`Resolving component: ${wrapInColor(component, ANSI_COLORS.GREEN_COLOR)}`);
  const componentFile = resolveComponentFile(component, workspaceRoot);

  // 2. Find the module
  const moduleDir = findModuleDir(componentFile);
  const moduleLabel = computeModuleLabel(moduleDir, workspaceRoot);
  const importPath = computeImportPath(componentFile, moduleDir);
  console.log(`Module: ${wrapInColor(moduleLabel, ANSI_COLORS.BLUE_COLOR)}`);
  console.log(`Import: ${wrapInColor(importPath, ANSI_COLORS.BLUE_COLOR)}`);

  // 3. Parse the ViewModel
  console.log('Parsing ViewModel...');
  const viewModel = parseComponentFile(componentFile);
  const requiredProps = viewModel.props.filter(p => !p.optional);
  console.log(
    `Found ${viewModel.props.length} props (${requiredProps.length} required) on ${wrapInColor(viewModel.componentName, ANSI_COLORS.GREEN_COLOR)}`,
  );

  // 4. Generate preview files
  const isValdi = isValdiWorkspace(workspaceRoot);
  generatePreviewFiles({
    componentFilePath: componentFile,
    importPath,
    moduleLabel,
    workspaceRoot,
    isValdiRepo: isValdi,
    viewModel,
  });

  const previewDir = getPreviewDir(workspaceRoot);
  console.log(`Preview harness written to ${wrapInColor(previewDir, ANSI_COLORS.GRAY_COLOR)}`);

  // 5. Build and launch the macOS app
  const target = '//apps/preview:preview_app_macos';
  const buildFlags = MACOS_BUILD_FLAGS.join(' ');
  console.log(`\nBuilding and launching ${wrapInColor(target, ANSI_COLORS.GREEN_COLOR)}...`);
  await bazel.runTarget(target, buildFlags);
}

export const command = 'preview <component>';
export const describe = 'Preview a Valdi component in a macOS window';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs.positional('component', {
    describe:
      'Component import path or file path\n' +
      '  e.g. widgets/src/components/button/Checkbox\n' +
      '  e.g. ./valdi_modules/widgets/src/components/button/Checkbox.tsx',
    type: 'string',
    demandOption: true,
  });
};
export const handler = makeCommandHandler(valdiPreview);
