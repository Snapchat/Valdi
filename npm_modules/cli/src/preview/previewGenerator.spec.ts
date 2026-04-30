import 'jasmine';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import {
  generatePreviewFiles,
  getPreviewDir,
  findModuleDir,
  getModuleNameFromImport,
  readModuleName,
  isValdiWorkspace,
  computeModuleLabel,
} from './previewGenerator';
import type { PreviewConfig } from './previewGenerator';

describe('previewGenerator', () => {
  let tmpDir: string;

  beforeEach(() => {
    tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'pg-test-'));
  });

  afterEach(() => {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  });

  describe('getPreviewDir', () => {
    it('returns apps/preview under workspace root', () => {
      expect(getPreviewDir('/workspace')).toBe('/workspace/apps/preview');
    });
  });

  describe('getModuleNameFromImport', () => {
    it('extracts the first path segment', () => {
      expect(getModuleNameFromImport('widgets/src/components/Button')).toBe('widgets');
      expect(getModuleNameFromImport('coreui/src/Cell')).toBe('coreui');
      expect(getModuleNameFromImport('mymod')).toBe('mymod');
    });
  });

  describe('findModuleDir', () => {
    it('walks up to find BUILD.bazel with valdi_module()', () => {
      // Create directory structure: tmpDir/mod/src/components/
      const modDir = path.join(tmpDir, 'mod');
      const srcDir = path.join(modDir, 'src', 'components');
      fs.mkdirSync(srcDir, { recursive: true });
      fs.writeFileSync(path.join(modDir, 'BUILD.bazel'), `
valdi_module(
    name = "mymod",
    srcs = glob(["**/*.tsx"]),
)
`, 'utf8');
      const componentFile = path.join(srcDir, 'Button.tsx');
      fs.writeFileSync(componentFile, 'export class Button {}', 'utf8');

      expect(findModuleDir(componentFile)).toBe(modDir);
    });

    it('skips BUILD.bazel files without valdi_module()', () => {
      const parentDir = path.join(tmpDir, 'parent');
      const childDir = path.join(parentDir, 'child');
      fs.mkdirSync(childDir, { recursive: true });
      // child has BUILD.bazel but no valdi_module
      fs.writeFileSync(path.join(childDir, 'BUILD.bazel'), 'cc_library(name = "lib")', 'utf8');
      // parent has valdi_module
      fs.writeFileSync(path.join(parentDir, 'BUILD.bazel'), 'valdi_module(name = "mod")', 'utf8');

      const componentFile = path.join(childDir, 'Comp.tsx');
      fs.writeFileSync(componentFile, '', 'utf8');

      expect(findModuleDir(componentFile)).toBe(parentDir);
    });

    it('throws when no valdi_module BUILD is found', () => {
      const isolatedDir = path.join(tmpDir, 'isolated');
      fs.mkdirSync(isolatedDir, { recursive: true });
      const componentFile = path.join(isolatedDir, 'Comp.tsx');
      fs.writeFileSync(componentFile, '', 'utf8');

      expect(() => findModuleDir(componentFile)).toThrowError(/Could not find a BUILD.bazel/);
    });
  });

  describe('readModuleName', () => {
    it('extracts module name from BUILD.bazel', () => {
      const modDir = path.join(tmpDir, 'mymod');
      fs.mkdirSync(modDir, { recursive: true });
      fs.writeFileSync(path.join(modDir, 'BUILD.bazel'), `
load("@valdi//bzl:valdi.bzl", "valdi_module")
valdi_module(
    name = "widgets",
    srcs = glob(["**/*.tsx"]),
)
`, 'utf8');
      expect(readModuleName(modDir)).toBe('widgets');
    });

    it('throws when no valdi_module name found', () => {
      const modDir = path.join(tmpDir, 'bad');
      fs.mkdirSync(modDir, { recursive: true });
      fs.writeFileSync(path.join(modDir, 'BUILD.bazel'), 'cc_library(name = "lib")', 'utf8');
      expect(() => readModuleName(modDir)).toThrowError(/Could not find valdi_module name/);
    });
  });

  describe('isValdiWorkspace', () => {
    it('returns true for workspace named "valdi"', () => {
      fs.writeFileSync(path.join(tmpDir, 'WORKSPACE'), 'workspace(name = "valdi")\n', 'utf8');
      expect(isValdiWorkspace(tmpDir)).toBe(true);
    });

    it('returns false for other workspace names', () => {
      fs.writeFileSync(path.join(tmpDir, 'WORKSPACE'), 'workspace(name = "my_app")\n', 'utf8');
      expect(isValdiWorkspace(tmpDir)).toBe(false);
    });

    it('returns false when no WORKSPACE file exists', () => {
      expect(isValdiWorkspace(tmpDir)).toBe(false);
    });
  });

  describe('computeModuleLabel', () => {
    it('computes a Bazel label from module dir', () => {
      const modDir = path.join(tmpDir, 'src', 'widgets');
      fs.mkdirSync(modDir, { recursive: true });
      fs.writeFileSync(path.join(modDir, 'BUILD.bazel'), 'valdi_module(name = "widgets")', 'utf8');
      expect(computeModuleLabel(modDir, tmpDir)).toBe('//src/widgets:widgets');
    });
  });

  describe('generatePreviewFiles', () => {
    it('generates PreviewRoot.tsx, BUILD.bazel, and tsconfig.json', () => {
      // Set up a module directory
      const modDir = path.join(tmpDir, 'src', 'widgets');
      fs.mkdirSync(modDir, { recursive: true });
      fs.writeFileSync(path.join(modDir, 'BUILD.bazel'), 'valdi_module(name = "widgets")', 'utf8');

      // Set up external repo (non-valdi workspace)
      fs.writeFileSync(path.join(tmpDir, 'WORKSPACE'), 'workspace(name = "my_app")', 'utf8');
      const configDir = path.join(tmpDir, 'valdi_modules', '_configs');
      fs.mkdirSync(configDir, { recursive: true });
      fs.writeFileSync(path.join(configDir, 'base.tsconfig.json'), '{}', 'utf8');

      const config: PreviewConfig = {
        componentFilePath: path.join(modDir, 'src', 'Button.tsx'),
        importPath: 'widgets/src/Button',
        moduleLabel: '//src/widgets:widgets',
        workspaceRoot: tmpDir,
        isValdiRepo: false,
        viewModel: {
          componentName: 'Button',
          viewModelName: 'ButtonViewModel',
          props: [
            { name: 'label', typeString: 'string', optional: false, isBoolean: false, isCallback: false, callbackParamType: undefined },
            { name: 'onTap', typeString: '() => void', optional: false, isBoolean: false, isCallback: true, callbackParamType: undefined },
          ],
          extraImports: [],
        },
      };

      generatePreviewFiles(config);

      const previewDir = getPreviewDir(tmpDir);
      expect(fs.existsSync(path.join(previewDir, 'PreviewRoot.tsx'))).toBe(true);
      expect(fs.existsSync(path.join(previewDir, 'BUILD.bazel'))).toBe(true);
      expect(fs.existsSync(path.join(previewDir, 'tsconfig.json'))).toBe(true);

      // Check PreviewRoot content
      const previewTsx = fs.readFileSync(path.join(previewDir, 'PreviewRoot.tsx'), 'utf8');
      expect(previewTsx).toContain("import { Button } from 'widgets/src/Button'");
      expect(previewTsx).toContain('<Button');
      expect(previewTsx).toContain("label={'Sample'}");
      expect(previewTsx).toContain('onTap={() => {}}');

      // Check BUILD.bazel content
      const buildBazel = fs.readFileSync(path.join(previewDir, 'BUILD.bazel'), 'utf8');
      expect(buildBazel).toContain('@valdi//bzl/valdi:valdi_application.bzl');
      expect(buildBazel).toContain('//src/widgets:widgets');
      expect(buildBazel).toContain('root_component_path = "PreviewRoot@preview/PreviewRoot"');

      // Check tsconfig.json content
      const tsconfig = JSON.parse(fs.readFileSync(path.join(previewDir, 'tsconfig.json'), 'utf8'));
      expect(tsconfig.compilerOptions.paths['preview/*']).toEqual(['./*']);
    });

    it('generates StatefulComponent when interactive state is detected', () => {
      const modDir = path.join(tmpDir, 'src', 'widgets');
      fs.mkdirSync(modDir, { recursive: true });
      fs.writeFileSync(path.join(modDir, 'BUILD.bazel'), 'valdi_module(name = "widgets")', 'utf8');
      fs.writeFileSync(path.join(tmpDir, 'WORKSPACE'), 'workspace(name = "other")', 'utf8');
      const configDir = path.join(tmpDir, 'valdi_modules', '_configs');
      fs.mkdirSync(configDir, { recursive: true });
      fs.writeFileSync(path.join(configDir, 'base.tsconfig.json'), '{}', 'utf8');

      const config: PreviewConfig = {
        componentFilePath: path.join(modDir, 'src', 'Toggle.tsx'),
        importPath: 'widgets/src/Toggle',
        moduleLabel: '//src/widgets:widgets',
        workspaceRoot: tmpDir,
        isValdiRepo: false,
        viewModel: {
          componentName: 'Toggle',
          viewModelName: 'ToggleVM',
          props: [
            { name: 'checked', typeString: 'boolean', optional: false, isBoolean: true, isCallback: false, callbackParamType: undefined },
            { name: 'onToggle', typeString: '(val: boolean) => void', optional: false, isBoolean: false, isCallback: true, callbackParamType: 'boolean' },
          ],
          extraImports: [],
        },
      };

      generatePreviewFiles(config);

      const previewTsx = fs.readFileSync(path.join(getPreviewDir(tmpDir), 'PreviewRoot.tsx'), 'utf8');
      expect(previewTsx).toContain('StatefulComponent');
      expect(previewTsx).toContain('state: PreviewState');
      expect(previewTsx).toContain('this.state.checked');
      expect(previewTsx).toContain('this.setState');
    });

    it('uses // prefix for Valdi repo BUILD.bazel', () => {
      const modDir = path.join(tmpDir, 'src', 'widgets');
      fs.mkdirSync(modDir, { recursive: true });
      fs.writeFileSync(path.join(modDir, 'BUILD.bazel'), 'valdi_module(name = "widgets")', 'utf8');
      fs.writeFileSync(path.join(tmpDir, 'WORKSPACE'), 'workspace(name = "valdi")', 'utf8');
      const configDir = path.join(tmpDir, 'modules', '_configs');
      fs.mkdirSync(configDir, { recursive: true });
      fs.writeFileSync(path.join(configDir, 'base.tsconfig.json'), '{}', 'utf8');

      const config: PreviewConfig = {
        componentFilePath: path.join(modDir, 'src', 'Button.tsx'),
        importPath: 'widgets/src/Button',
        moduleLabel: '//src/widgets:widgets',
        workspaceRoot: tmpDir,
        isValdiRepo: true,
        viewModel: {
          componentName: 'Button',
          viewModelName: 'ButtonVM',
          props: [],
          extraImports: [],
        },
      };

      generatePreviewFiles(config);

      const buildBazel = fs.readFileSync(path.join(getPreviewDir(tmpDir), 'BUILD.bazel'), 'utf8');
      // Valdi repo uses // prefix (no @valdi)
      expect(buildBazel).toContain('load("//bzl/valdi:valdi_application.bzl"');
      expect(buildBazel).not.toContain('@valdi');
    });
  });
});
