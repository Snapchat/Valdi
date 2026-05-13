import fs from 'fs';
import path from 'path';
import type { InteractiveState, ParsedProp, ParsedViewModel } from './viewModelParser';
import { detectInteractiveState, sampleValueForType } from './viewModelParser';

export interface PreviewConfig {
  /** Absolute path to the component .tsx file */
  componentFilePath: string;
  /** Import path for the component, e.g. "widgets/src/components/button/Checkbox" */
  importPath: string;
  /** Bazel label for the component's module, e.g. "//valdi_modules/widgets:widgets" */
  moduleLabel: string;
  /** Absolute path to the workspace root */
  workspaceRoot: string;
  /** Whether this is the core Valdi repo (workspace name = "valdi") */
  isValdiRepo: boolean;
  /** Parsed ViewModel info */
  viewModel: ParsedViewModel;
}

const PREVIEW_DIR = 'apps/preview';

export function getPreviewDir(workspaceRoot: string): string {
  return path.join(workspaceRoot, PREVIEW_DIR);
}

/**
 * Write all preview harness files. Creates the directory if needed.
 */
export function generatePreviewFiles(config: PreviewConfig): void {
  const previewDir = getPreviewDir(config.workspaceRoot);
  fs.mkdirSync(previewDir, { recursive: true });

  fs.writeFileSync(path.join(previewDir, 'PreviewRoot.tsx'), generatePreviewTsx(config));
  fs.writeFileSync(path.join(previewDir, 'BUILD.bazel'), generateBuildBazel(config));
  fs.writeFileSync(path.join(previewDir, 'tsconfig.json'), generateTsconfig(config));
}

// ---------------------------------------------------------------------------
// PreviewRoot.tsx
// ---------------------------------------------------------------------------

function generatePreviewTsx(config: PreviewConfig): string {
  const { viewModel, importPath } = config;
  const { componentName, props } = viewModel;
  const interactiveState = detectInteractiveState(props);
  const hasState = interactiveState.length > 0;

  const lines: string[] = [];

  // Imports
  if (hasState) {
    lines.push("import { StatefulComponent } from 'valdi_core/src/Component';");
  } else {
    lines.push("import { Component } from 'valdi_core/src/Component';");
  }
  lines.push("import { Style } from 'valdi_core/src/Style';");
  lines.push("import { View } from 'valdi_tsx/src/NativeTemplateElements';");
  lines.push(`import { ${componentName} } from '${importPath}';`);
  lines.push('');

  // ViewModel + Context (required by valdi_application)
  lines.push('/**');
  lines.push(' * @ViewModel');
  lines.push(' * @ExportModel');
  lines.push(' */');
  lines.push('export interface ViewModel {}');
  lines.push('');
  lines.push('/**');
  lines.push(' * @Context');
  lines.push(' * @ExportModel');
  lines.push(' */');
  lines.push('export interface ComponentContext {}');
  lines.push('');

  // State interface (if interactive)
  if (hasState) {
    lines.push('interface PreviewState {');
    for (const s of interactiveState) {
      lines.push(`  ${s.stateName}: boolean;`);
    }
    lines.push('}');
    lines.push('');
  }

  // Component class
  lines.push('/**');
  lines.push(' * @Component');
  lines.push(' * @ExportModel');
  lines.push(' */');

  if (hasState) {
    lines.push('export class PreviewRoot extends StatefulComponent<ViewModel, PreviewState, ComponentContext> {');
    lines.push('  state: PreviewState = {');
    for (const s of interactiveState) {
      lines.push(`    ${s.stateName}: false,`);
    }
    lines.push('  };');
    lines.push('');
    // Generate class-property handlers for each interactive state callback
    // (inline arrows in onRender() break Valdi's native view equality checks)
    for (const s of interactiveState) {
      lines.push(`  private ${s.callbackPropName}Handler = (val: boolean) => this.setState({ ${s.stateName}: val });`);
    }
  } else {
    lines.push('export class PreviewRoot extends Component<ViewModel, ComponentContext> {');
  }

  lines.push('');
  lines.push('  onRender(): void {');
  lines.push('    <view style={styles.container}>');
  lines.push(`      <label value="${componentName}" font="system-bold 20" color="#000000" marginBottom={24} />`);
  lines.push(`      ${generateComponentJsx(componentName, props, interactiveState)}`);
  lines.push('    </view>;');
  lines.push('  }');
  lines.push('}');
  lines.push('');

  // Styles
  lines.push('const styles = {');
  lines.push('  container: new Style<View>({');
  lines.push("    backgroundColor: '#ffffff',");
  lines.push("    width: '100%',");
  lines.push("    height: '100%',");
  lines.push("    alignItems: 'center',");
  lines.push("    justifyContent: 'center',");
  lines.push('    padding: 24,');
  lines.push('  }),');
  lines.push('};');
  lines.push('');

  return lines.join('\n');
}

function generateComponentJsx(
  componentName: string,
  props: ParsedProp[],
  interactiveState: InteractiveState[],
): string {
  const stateMap = new Map<string, InteractiveState>();
  for (const s of interactiveState) {
    stateMap.set(s.boolPropName, s);
    stateMap.set(s.callbackPropName, s);
  }

  const requiredProps = props.filter(p => !p.optional);
  if (requiredProps.length === 0) {
    return `<${componentName} />`;
  }

  const propAssignments: string[] = [];
  for (const prop of requiredProps) {
    const state = stateMap.get(prop.name);
    if (state && prop.isBoolean) {
      propAssignments.push(`${prop.name}={this.state.${state.stateName}}`);
    } else if (state && prop.isCallback) {
      propAssignments.push(
        `${prop.name}={this.${state.callbackPropName}Handler}`,
      );
    } else {
      propAssignments.push(`${prop.name}={${sampleValueForType(prop.typeString)}}`);
    }
  }

  if (propAssignments.length <= 2) {
    return `<${componentName} ${propAssignments.join(' ')} />`;
  }

  const indentedProps = propAssignments.map(p => `        ${p}`).join('\n');
  return `<${componentName}\n${indentedProps}\n      />`;
}

// ---------------------------------------------------------------------------
// BUILD.bazel
// ---------------------------------------------------------------------------

function generateBuildBazel(config: PreviewConfig): string {
  const prefix = config.isValdiRepo ? '' : '@valdi';

  return `load("${prefix}//bzl/valdi:valdi_application.bzl", "valdi_application")
load("${prefix}//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "preview",
    srcs = glob([
        "**/*.ts",
        "**/*.tsx",
    ]),
    visibility = ["//visibility:public"],
    deps = [
        "${prefix}//src/valdi_modules/src/valdi/valdi_core",
        "${prefix}//src/valdi_modules/src/valdi/valdi_tsx",
        "${config.moduleLabel}",
    ],
)

valdi_application(
    name = "preview_app",
    root_component_path = "PreviewRoot@preview/PreviewRoot",
    title = "Valdi Preview",
    deps = [":preview"],
)
`;
}

// ---------------------------------------------------------------------------
// tsconfig.json
// ---------------------------------------------------------------------------

function generateTsconfig(config: PreviewConfig): string {
  const previewDir = getPreviewDir(config.workspaceRoot);

  if (config.isValdiRepo) {
    // Valdi repo: base tsconfig lacks wildcard paths, need explicit mappings
    const valdiModulesDir = path.join(config.workspaceRoot, 'src/valdi_modules/src/valdi');
    const relToValdiModules = path.relative(previewDir, valdiModulesDir);

    // Also add the target module path
    const moduleDir = findModuleDir(config.componentFilePath);
    const moduleName = getModuleNameFromImport(config.importPath);
    const relToModule = path.relative(previewDir, moduleDir);

    const paths: Record<string, string[]> = {
      'preview/*': ['./*'],
      'valdi_tsx/*': [`${relToValdiModules}/valdi_tsx/*`],
      'valdi_core/*': [`${relToValdiModules}/valdi_core/*`],
      'valdi_core': [`${relToValdiModules}/valdi_core/src/tslib.d.ts`],
    };
    paths[`${moduleName}/*`] = [`${relToModule}/*`];

    return JSON.stringify(
      {
        extends: '../../modules/_configs/base.tsconfig.json',
        compilerOptions: {
          types: ['../../modules/types/Long', '../../modules/types/globals'],
          paths,
        },
      },
      null,
      2,
    ) + '\n';
  }

  // External repo: base tsconfig has wildcard paths that auto-resolve
  const baseConfigRel = findBaseConfigRelativePath(config.workspaceRoot, previewDir);

  return JSON.stringify(
    {
      extends: baseConfigRel,
      compilerOptions: {
        paths: {
          'preview/*': ['./*'],
        },
      },
    },
    null,
    2,
  ) + '\n';
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Walk up from a component file to find the directory containing its module BUILD.bazel */
export function findModuleDir(componentFilePath: string): string {
  let dir = path.dirname(componentFilePath);
  while (dir !== path.dirname(dir)) {
    const buildFile = path.join(dir, 'BUILD.bazel');
    if (fs.existsSync(buildFile)) {
      const content = fs.readFileSync(buildFile, 'utf8');
      if (/valdi_module\s*\(/.test(content)) {
        return dir;
      }
    }
    dir = path.dirname(dir);
  }
  throw new Error(`Could not find a BUILD.bazel with valdi_module() above ${componentFilePath}`);
}

/** Extract the module name (first segment) from an import path like "widgets/src/..." */
export function getModuleNameFromImport(importPath: string): string {
  return importPath.split('/')[0]!;
}

/** Read the valdi_module name from a BUILD.bazel file in the given directory */
export function readModuleName(moduleDir: string): string {
  const buildContent = fs.readFileSync(path.join(moduleDir, 'BUILD.bazel'), 'utf8');
  const nameMatch = buildContent.match(/valdi_module\s*\([^)]*name\s*=\s*"(\w+)"/s);
  if (!nameMatch) {
    throw new Error(`Could not find valdi_module name in ${moduleDir}/BUILD.bazel`);
  }
  return nameMatch[1]!;
}

/** Detect whether this workspace is the core Valdi repo */
export function isValdiWorkspace(workspaceRoot: string): boolean {
  const wsFile = path.join(workspaceRoot, 'WORKSPACE');
  if (!fs.existsSync(wsFile)) return false;
  const content = fs.readFileSync(wsFile, 'utf8');
  return /workspace\s*\(\s*name\s*=\s*"valdi"\s*\)/.test(content);
}

/** Compute the Bazel label for a module given its directory and workspace root */
export function computeModuleLabel(moduleDir: string, workspaceRoot: string): string {
  const relPath = path.relative(workspaceRoot, moduleDir);
  const moduleName = readModuleName(moduleDir);
  return `//${relPath}:${moduleName}`;
}

/** Find the base tsconfig relative path for external repos */
function findBaseConfigRelativePath(workspaceRoot: string, fromDir: string): string {
  // Try common locations
  const candidates = [
    'valdi_modules/_configs/base.tsconfig.json',
    'modules/_configs/base.tsconfig.json',
  ];
  for (const candidate of candidates) {
    const fullPath = path.join(workspaceRoot, candidate);
    if (fs.existsSync(fullPath)) {
      return path.relative(fromDir, fullPath);
    }
  }
  // Fallback
  return '../../valdi_modules/_configs/base.tsconfig.json';
}
