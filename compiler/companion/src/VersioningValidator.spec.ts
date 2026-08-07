import 'ts-jest';
import * as ts from 'typescript';
import { Workspace } from './Workspace';

function createWorkspaceWithFile(
  contents: string,
  nativeApiMinVersion: number | undefined,
  fileName: string,
): Workspace {
  const workspace = new Workspace(
    '/',
    false,
    undefined,
    {
      target: ts.ScriptTarget.ESNext,
      module: ts.ModuleKind.CommonJS,
      lib: ['lib.es2015.d.ts'],
      strict: true,
      jsx: ts.JsxEmit.Preserve,
    },
    nativeApiMinVersion,
  );

  workspace.registerInMemoryFile(fileName, contents);
  workspace.addSourceFileAtPath(fileName);
  return workspace;
}

function getDiagnosticTexts(contents: string, nativeApiMinVersion?: number): string[] {
  const workspace = createWorkspaceWithFile(contents, nativeApiMinVersion, '/file.ts');
  const diagnostics = workspace.getDiagnosticsSync('/file.ts').diagnostics;
  workspace.destroy();
  return diagnostics.map((diagnostic) => diagnostic.text);
}

function getJsxDiagnosticTexts(contents: string): string[] {
  const workspace = createWorkspaceWithFile(contents, undefined, '/file.tsx');
  const diagnostics = workspace.getDiagnosticsSync('/file.tsx').diagnostics;
  workspace.destroy();
  return diagnostics.map((diagnostic) => diagnostic.text);
}

function getDiagnosticTextsWithImportedFile(
  contents: string,
  importedContents: string,
  nativeApiMinVersion: number | undefined,
): string[] {
  const workspace = createWorkspaceWithFile(importedContents, nativeApiMinVersion, '/models.ts');
  workspace.registerInMemoryFile('/file.ts', contents);
  workspace.addSourceFileAtPath('/file.ts');
  const diagnostics = workspace.getDiagnosticsSync('/file.ts').diagnostics;
  workspace.destroy();
  return diagnostics.map((diagnostic) => diagnostic.text);
}

describe('VersioningValidator', () => {
  it('allows versioned properties inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        title: string;
        // @Version(43)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (isVersionAtLeast(43)) {
          model.subtitle;
        }
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects versioned properties inside an insufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        title: string;
        // @Version(43)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (isVersionAtLeast(42)) {
          model.subtitle;
        }
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects versioned properties outside a version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        title: string;
        // @Version(43)
        subtitle?: string;
      }

      function render(model: MyModel) {
        model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects destructuring versioned properties outside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      const { subtitle } = model;
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects renamed destructured properties with default values', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      const { subtitle: label = 'Default' } = model;
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects nested destructured properties requiring a newer version', () => {
    const diagnostics = getDiagnosticTexts(`
      interface NestedModel {
        // @Version(43)
        subtitle?: string;
      }

      interface MyModel {
        nested: NestedModel;
      }

      declare const model: MyModel;
      const { nested: { subtitle } } = model;
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects destructured properties referenced through computed literal names', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      const { ['subtitle']: label } = model;
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('allows destructuring versioned properties inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      if (isVersionAtLeast(43)) {
        const { subtitle: label = 'Default' } = model;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects destructuring versioned properties inside an insufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      if (isVersionAtLeast(42)) {
        const { subtitle } = model;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects destructuring versioned properties in unversioned function parameters', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      function render({ subtitle }: MyModel): void {}
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('allows destructured parameters to inherit their containing function version', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      // @Version(43)
      function render({ subtitle }: MyModel): void {}
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows destructured method parameters to inherit their containing class version', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      // @Version(43)
      class Renderer {
        render({ subtitle }: MyModel): void {}
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows rest destructuring because it does not require newer properties to exist', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      const { ...remaining } = model;
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects versioned properties in destructuring assignments', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      let subtitle: string | undefined;
      ({ subtitle } = model);
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects renamed versioned properties with defaults in destructuring assignments', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      let label: string;
      ({ subtitle: label = 'Default' } = model);
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('rejects nested versioned properties in destructuring assignments', () => {
    const diagnostics = getDiagnosticTexts(`
      interface NestedModel {
        // @Version(43)
        subtitle?: string;
      }

      interface MyModel {
        nested: NestedModel;
      }

      declare const model: MyModel;
      let label: string | undefined;
      ({ nested: { subtitle: label } } = model);
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('allows destructuring assignments inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      declare const model: MyModel;
      let label: string | undefined;
      if (isVersionAtLeast(43)) {
        ({ subtitle: label } = model);
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('does not treat ordinary object literals as destructuring assignments', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      const localModel: MyModel = { subtitle: 'Hello' };
    `);

    expect(diagnostics).toEqual([]);
  });

  it('requires interface members to inherit their containing version at use sites', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(43)
      interface Renderer {
        title: string;
        draw(): void;
      }

      declare const renderer: Renderer;
      renderer.title;
      renderer.draw();
    `);

    expect(diagnostics).toEqual([
      "Property 'title' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
      'Function call requires @Version(43) or an enclosing isVersionAtLeast(43) block',
    ]);
  });

  it('requires class members to inherit their containing version at use sites', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(43)
      class Renderer {
        title = 'title';
        draw() {}
      }

      declare const renderer: Renderer;
      renderer.title;
      renderer.draw();
    `);

    expect(diagnostics).toEqual([
      "Property 'title' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
      'Function call requires @Version(43) or an enclosing isVersionAtLeast(43) block',
    ]);
  });

  it('allows inherited container members inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      // @Version(43)
      interface Renderer {
        title: string;
        draw(): void;
      }

      declare const renderer: Renderer;
      if (isVersionAtLeast(43)) {
        renderer.title;
        renderer.draw();
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('requires the highest version across merged member declarations', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface Renderer {
        // @Version(42)
        title: string;
      }

      interface Renderer {
        // @Version(43)
        title: string;
      }

      declare const renderer: Renderer;
      if (isVersionAtLeast(42)) {
        renderer.title;
      }
    `);

    expect(diagnostics).toEqual(["Property 'title' requires @Version(43) or an enclosing isVersionAtLeast(43) block"]);
  });

  it('rejects placeholder-versioned properties outside a version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        title: string;
        // @Version(__PLACEHOLDER__)
        subtitle?: string;
      }

      function render(model: MyModel) {
        model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(__PLACEHOLDER__) or an enclosing isVersionAtLeast(__PLACEHOLDER__) block",
    ]);
  });

  it('allows placeholder-versioned properties inside a placeholder version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare const __PLACEHOLDER__: number;
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(__PLACEHOLDER__)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (isVersionAtLeast(__PLACEHOLDER__)) {
          model.subtitle;
        }
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows placeholder-versioned properties inside a max safe integer version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(__PLACEHOLDER__)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (isVersionAtLeast(9007199254740991)) {
          model.subtitle;
        }
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('applies the highest nested version guard to child blocks only', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
        // @Version(43)
        detail?: string;
      }

      function render(model: MyModel) {
        if (isVersionAtLeast(42)) {
          model.subtitle;
          if (isVersionAtLeast(43)) {
            model.detail;
          }
          model.detail;
        }
      }
    `);

    expect(diagnostics).toEqual(["Property 'detail' requires @Version(43) or an enclosing isVersionAtLeast(43) block"]);
  });

  it('does not apply the then branch version guard to the else branch', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (isVersionAtLeast(42)) {
          model.subtitle;
        } else {
          model.subtitle;
        }
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(42) or an enclosing isVersionAtLeast(42) block",
    ]);
  });

  it('allows versioned properties in the right side and body of a version-guarded && condition', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel): string | undefined {
        if (isVersionAtLeast(42) && model.subtitle) {
          return model.subtitle;
        }
        return undefined;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows standalone short-circuit expressions after a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        const subtitle = isVersionAtLeast(42) && model.subtitle;
        return isVersionAtLeast(42) && model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('keeps standalone short-circuit version guards order-sensitive', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        return model.subtitle && isVersionAtLeast(42);
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(42) or an enclosing isVersionAtLeast(42) block",
    ]);
  });

  it('rejects standalone short-circuit expressions with an insufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      function render(model: MyModel) {
        return isVersionAtLeast(42) && model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('applies short-circuit version guards inside JSX expressions', () => {
    const diagnostics = getJsxDiagnosticTexts(`
      declare namespace JSX {
        interface Element {}
        interface IntrinsicElements {
          view: {};
        }
      }

      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      declare const model: MyModel;
      <view>{isVersionAtLeast(42) && model.subtitle}</view>;
    `);

    expect(diagnostics).toEqual([]);
  });

  it('applies version guards to the true branch of ternary expressions', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        return isVersionAtLeast(42) ? model.subtitle : undefined;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('does not apply ternary version guards to the unguarded branch', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        return isVersionAtLeast(42) ? undefined : model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(42) or an enclosing isVersionAtLeast(42) block",
    ]);
  });

  it('applies negated version guards to the false branch of ternary expressions', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        return !isVersionAtLeast(42) ? undefined : model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('applies version guards after negated early returns', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (!isVersionAtLeast(42)) {
          return;
        }

        model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('applies version guards after early throws', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (!isVersionAtLeast(42)) {
          throw new Error('Unsupported native API');
        }

        model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('does not apply early-return guards when the guarded branch can continue', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;
      declare function shouldReturn(): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        if (!isVersionAtLeast(42)) {
          if (shouldReturn()) {
            return;
          }
        }

        model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(42) or an enclosing isVersionAtLeast(42) block",
    ]);
  });

  it('applies version guards to the right side of negated short-circuit alternatives', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel) {
        return !isVersionAtLeast(42) || model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('keeps && condition guards order-sensitive for short-circuit evaluation', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel): string | undefined {
        if (model.subtitle && isVersionAtLeast(42)) {
          return model.subtitle;
        }
        return undefined;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(42) or an enclosing isVersionAtLeast(42) block",
    ]);
  });

  it('rejects versioned properties in && conditions guarded by an insufficient version', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      function render(model: MyModel): string | undefined {
        if (isVersionAtLeast(42) && model.subtitle) {
          return model.subtitle;
        }
        return undefined;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('combines parenthesized and nested && version guards', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
        // @Version(43)
        detail?: string;
      }

      declare function isReady(): boolean;

      function render(model: MyModel): string | undefined {
        if ((isReady() && isVersionAtLeast(42)) && (isVersionAtLeast(43) && model.detail)) {
          model.subtitle;
          return model.detail;
        }
        return undefined;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows versioned properties inside a sufficiently versioned function body', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      // @Version(42)
      function render(model: MyModel) {
        model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects versioned properties inside an insufficiently versioned function body', () => {
    const diagnostics = getDiagnosticTexts(`
      interface MyModel {
        // @Version(43)
        subtitle?: string;
      }

      // @Version(42)
      function render(model: MyModel) {
        model.subtitle;
      }
    `);

    expect(diagnostics).toEqual([
      "Property 'subtitle' requires @Version(43) or an enclosing isVersionAtLeast(43) block",
    ]);
  });

  it('allows nested lambdas created inside a far outer version guard to use versioned properties', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;
      declare function run(callback: () => string | undefined): string | undefined;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      function render(model: MyModel): string | undefined {
        if (isVersionAtLeast(42)) {
          const renderLater = () => {
            const renderNested = () => {
              return model.subtitle;
            };
            return renderNested();
          };

          return run(() => renderLater());
        }

        return undefined;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows nested lambdas created inside a far outer && version guard to use versioned properties', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;
      declare function run(callback: () => string | undefined): string | undefined;

      interface MyModel {
        // @Version(42)
        subtitle?: string;
      }

      declare function isReady(): boolean;

      function render(model: MyModel): string | undefined {
        if (isReady() && isVersionAtLeast(42)) {
          return run(() => {
            const renderNested = () => model.subtitle;
            return renderNested();
          });
        }

        return undefined;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows calls to versioned functions inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      // @Version(42)
      function renderLabelNew() {}

      function render() {
        if (isVersionAtLeast(42)) {
          renderLabelNew();
        }
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects calls to versioned functions outside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      function renderLabelNew() {}

      function render() {
        renderLabelNew();
      }
    `);

    expect(diagnostics).toEqual(['Function call requires @Version(42) or an enclosing isVersionAtLeast(42) block']);
  });

  it('rejects calls to versioned functions inside an insufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      // @Version(43)
      function renderLabelNew() {}

      function render() {
        if (isVersionAtLeast(42)) {
          renderLabelNew();
        }
      }
    `);

    expect(diagnostics).toEqual(['Function call requires @Version(43) or an enclosing isVersionAtLeast(43) block']);
  });

  it('allows calls to versioned methods inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      class Renderer {
        // @Version(42)
        renderLabelNew() {}
      }

      function render(renderer: Renderer) {
        if (isVersionAtLeast(42)) {
          renderer.renderLabelNew();
        }
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects calls to versioned methods outside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      class Renderer {
        // @Version(42)
        renderLabelNew() {}
      }

      function render(renderer: Renderer) {
        renderer.renderLabelNew();
      }
    `);

    expect(diagnostics).toEqual(['Function call requires @Version(42) or an enclosing isVersionAtLeast(42) block']);
  });

  it('rejects construction of versioned classes outside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      class NewRenderer {}

      function render(): void {
        new NewRenderer();
      }
    `,
      1,
    );

    expect(diagnostics).toEqual(['Constructor call requires @Version(42) or an enclosing isVersionAtLeast(42) block']);
  });

  it('allows construction of versioned classes inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(
      `
      declare function isVersionAtLeast(version: number): boolean;

      // @Version(42)
      class NewRenderer {}

      function render(): void {
        if (isVersionAtLeast(42)) {
          new NewRenderer();
        }
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('rejects JSX usage of versioned components outside a sufficient version guard', () => {
    const diagnostics = getJsxDiagnosticTexts(`
      declare namespace JSX {
        interface Element {}
        interface ElementClass {
          render(): Element;
        }
      }

      // @Version(42)
      class NewComponent {
        render(): JSX.Element {
          return {};
        }
      }

      <NewComponent />;
    `);

    expect(diagnostics).toEqual([
      "Component 'NewComponent' requires @Version(42) or an enclosing isVersionAtLeast(42) block",
    ]);
  });

  it('allows JSX usage of versioned components inside a sufficient version guard', () => {
    const diagnostics = getJsxDiagnosticTexts(`
      declare namespace JSX {
        interface Element {}
        interface ElementClass {
          render(): Element;
        }
      }

      declare function isVersionAtLeast(version: number): boolean;

      // @Version(42)
      class NewComponent {
        render(): JSX.Element {
          return {};
        }
      }

      isVersionAtLeast(42) && <NewComponent />;
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects calls to placeholder-versioned functions outside a version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(__PLACEHOLDER__)
      function renderLabelNew() {}

      function render() {
        renderLabelNew();
      }
    `);

    expect(diagnostics).toEqual([
      'Function call requires @Version(__PLACEHOLDER__) or an enclosing isVersionAtLeast(__PLACEHOLDER__) block',
    ]);
  });

  it('allows calls to placeholder-versioned functions inside a placeholder version guard', () => {
    const diagnostics = getDiagnosticTexts(`
      declare const __PLACEHOLDER__: number;
      declare function isVersionAtLeast(version: number): boolean;

      // @Version(__PLACEHOLDER__)
      function renderLabelNew() {}

      function render() {
        if (isVersionAtLeast(__PLACEHOLDER__)) {
          renderLabelNew();
        }
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('requires declarations exposing versioned types to be versioned', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      function render(model: NewModel) {}
    `);

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(42) on the containing declaration"]);
  });

  it('requires declarations exposing imported versioned types to be versioned', () => {
    const diagnostics = getDiagnosticTextsWithImportedFile(
      `
      import type { NewModel as ImportedModel } from './models';

      function render(model: ImportedModel): void {}
    `,
      `
      // @Version(42)
      export interface NewModel {
        value: string;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual(["Type 'ImportedModel' requires @Version(42) on the containing declaration"]);
  });

  it('requires constructor parameters exposing versioned types to be versioned', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      interface NewModel {
        value: string;
      }

      class ExistingRenderer {
        constructor(model: NewModel) {}
      }
    `,
      1,
    );

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(42) on the containing declaration"]);
  });

  it('allows constructor parameters to inherit their containing class version', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      interface NewModel {
        value: string;
      }

      // @Version(42)
      class NewRenderer {
        constructor(model: NewModel) {}
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('allows sufficiently versioned declarations exposing imported versioned types', () => {
    const diagnostics = getDiagnosticTextsWithImportedFile(
      `
      import type { NewModel as ImportedModel } from './models';

      // @Version(42)
      function render(model: ImportedModel): void {}
    `,
      `
      // @Version(42)
      export interface NewModel {
        value: string;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('requires declarations exposing placeholder-versioned types to be placeholder-versioned', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(__PLACEHOLDER__)
      interface NewModel {
        title: string;
      }

      function render(model: NewModel) {}
    `);

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(__PLACEHOLDER__) on the containing declaration"]);
  });

  it('allows placeholder-versioned declarations exposing placeholder-versioned types', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(__PLACEHOLDER__)
      interface NewModel {
        title: string;
      }

      // @Version(__PLACEHOLDER__)
      function render(model: NewModel): NewModel {
        return model;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('rejects versioned declarations that expose newer types', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(43)
      interface NewModel {
        title: string;
      }

      // @Version(42)
      function render(model: NewModel) {}
    `);

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(43) on the containing declaration"]);
  });

  it('allows declarations exposing older versioned types', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      // @Version(43)
      function render(model: NewModel): NewModel {
        return model;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('requires return types exposing versioned types to be versioned', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      function makeModel(): NewModel {
        return { title: 'title' };
      }
    `);

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(42) on the containing declaration"]);
  });

  it('allows interfaces to extend versioned types because interface heritage is erased', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      interface ExtendedModel extends NewModel {
        subtitle: string;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows interfaces extending older versioned types', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      // @Version(43)
      interface ExtendedModel extends NewModel {
        subtitle: string;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows classes to implement versioned interfaces because implements clauses are erased', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface Renderer {
        render(): void;
      }

      class LocalRenderer implements Renderer {
        render(): void {}
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('requires classes extending versioned classes to be versioned', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      class BaseRenderer {}

      class LocalRenderer extends BaseRenderer {}
    `);

    expect(diagnostics).toEqual(["Type 'BaseRenderer' requires @Version(42) on the containing declaration"]);
  });

  it('requires interface methods exposing versioned types to be versioned', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      interface Renderer {
        render(model: NewModel): void;
      }
    `);

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(42) on the containing declaration"]);
  });

  it('allows versioned interface methods exposing compatible versioned types', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      interface Renderer {
        // @Version(42)
        render(model: NewModel): void;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows class methods to inherit their containing version for signatures and bodies', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        value: string;
      }

      // @Version(42)
      class Renderer {
        render(model: NewModel): NewModel {
          return model;
        }
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows class field initializers to inherit their containing version', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      interface NewModel {
        value: string;
      }

      declare const model: NewModel;

      // @Version(42)
      class Renderer {
        value = model.value;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('allows class arrow-function properties and nested callbacks to inherit their containing version', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      interface NewModel {
        value: string;
      }

      // @Version(42)
      class Renderer {
        render = (model: NewModel): string => {
          const readValue = () => model.value;
          return readValue();
        };
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('allows nested callback signatures to inherit their containing class version', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      interface NewModel {
        value: string;
      }

      // @Version(42)
      class Renderer {
        render(): void {
          const callback = (model: NewModel): void => {};
        }
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('allows nested callback signatures inside a sufficient version guard', () => {
    const diagnostics = getDiagnosticTexts(
      `
      declare function isVersionAtLeast(version: number): boolean;

      // @Version(42)
      interface NewModel {
        value: string;
      }

      if (isVersionAtLeast(42)) {
        const callback = (model: NewModel): void => {};
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('still rejects class field initializers requiring a newer version than their containing class', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(43)
      interface NewModel {
        value: string;
      }

      declare const model: NewModel;

      // @Version(42)
      class Renderer {
        value = model.value;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual(["Property 'value' requires @Version(43) or an enclosing isVersionAtLeast(43) block"]);
  });

  it('does not apply a class version to eagerly evaluated static field initializers', () => {
    const diagnostics = getDiagnosticTexts(
      `
      interface Model {
        // @Version(42)
        value: string;
      }

      declare const model: Model;

      // @Version(42)
      class Renderer {
        static value = model.value;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual(["Property 'value' requires @Version(42) or an enclosing isVersionAtLeast(42) block"]);
  });

  it('allows eagerly evaluated static fields to use sufficiently guarded APIs', () => {
    const diagnostics = getDiagnosticTexts(
      `
      declare function isVersionAtLeast(version: number): boolean;

      interface Model {
        // @Version(42)
        value: string;
      }

      declare const model: Model;

      // @Version(42)
      class Renderer {
        static value = isVersionAtLeast(42) ? model.value : undefined;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('does not apply a class version to eagerly evaluated static blocks', () => {
    const diagnostics = getDiagnosticTexts(
      `
      interface Model {
        // @Version(42)
        value: string;
      }

      declare const model: Model;

      // @Version(42)
      class Renderer {
        static {
          model.value;
        }
      }
    `,
      1,
    );

    expect(diagnostics).toEqual(["Property 'value' requires @Version(42) or an enclosing isVersionAtLeast(42) block"]);
  });

  it('allows class accessors to inherit their containing version for signatures and bodies', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        value: string;
      }

      // @Version(42)
      class Renderer {
        get model(): NewModel {
          return { value: 'value' };
        }

        set model(value: NewModel) {}
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('requires a newer explicit class method version despite its containing version', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(43)
      interface NewModel {
        value: string;
      }

      // @Version(42)
      class Renderer {
        render(model: NewModel): void {}
      }
    `);

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(43) on the containing declaration"]);
  });

  it('allows versioned properties exposing compatible versioned types', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      interface Renderer {
        // @Version(42)
        model: NewModel;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows existing interface properties to refer to newer versioned types', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      interface NewModel {
        value: string;
      }

      interface ExistingOptions {
        model?: NewModel;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('allows unversioned class fields to refer to newer versioned types', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(42)
      interface NewModel {
        value: string;
      }

      class ExistingController {
        private model?: NewModel;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual([]);
  });

  it('rejects explicitly versioned properties exposing newer types', () => {
    const diagnostics = getDiagnosticTexts(
      `
      // @Version(43)
      interface NewModel {
        value: string;
      }

      interface ExistingOptions {
        // @Version(42)
        model?: NewModel;
      }
    `,
      1,
    );

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(43) on the containing declaration"]);
  });

  it('validates isVersionAtLeast rejects non-literal arguments', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(version: number): boolean;

      const version = 42;
      if (isVersionAtLeast(version)) {
      }
    `);

    expect(diagnostics).toEqual(['isVersionAtLeast expects exactly one numeric literal or __PLACEHOLDER__ argument']);
  });

  it('validates isVersionAtLeast rejects missing arguments', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(...versions: number[]): boolean;

      if (isVersionAtLeast()) {
      }
    `);

    expect(diagnostics).toEqual(['isVersionAtLeast expects exactly one numeric literal or __PLACEHOLDER__ argument']);
  });

  it('validates isVersionAtLeast rejects extra arguments', () => {
    const diagnostics = getDiagnosticTexts(`
      declare function isVersionAtLeast(...versions: number[]): boolean;

      if (isVersionAtLeast(42, 43)) {
      }
    `);

    expect(diagnostics).toEqual(['isVersionAtLeast expects exactly one numeric literal or __PLACEHOLDER__ argument']);
  });

  it('preserves unchecked native contracts when the workspace minimum is disabled', () => {
    const diagnostics = getDiagnosticTexts(`
      // @ExportModel
      export interface NativeModel {
        futureValue?: string;
      }

      function render(model: NativeModel) {
        model.futureValue;
      }
    `);

    expect(diagnostics).toEqual([]);
  });

  it('allows implicitly versioned native members at the workspace minimum', () => {
    const diagnostics = getDiagnosticTexts(
      `
        // @ExportModel
        export interface NativeModel {
          value: string;
        }

        function render(model: NativeModel) {
          model.value;
        }
      `,
      2,
    );

    expect(diagnostics).toEqual([]);
  });

  it('lets an explicit member version override native-contract inheritance', () => {
    const diagnostics = getDiagnosticTexts(
      `
        // @ExportModel
        export interface NativeModel {
          // @Version(1)
          existingValue: string;
          // @Version(3)
          futureValue?: string;
        }

        function render(model: NativeModel) {
          model.existingValue;
          model.futureValue;
        }
      `,
      2,
    );

    expect(diagnostics).toEqual([
      "Property 'futureValue' requires @Version(3) or an enclosing isVersionAtLeast(3) block",
    ]);
  });

  it('does not let the workspace minimum mask a higher native-container version', () => {
    const diagnostics = getDiagnosticTexts(
      `
        // @ExportModel
        // @Version(5)
        export interface NativeModel {
          value: string;
        }

        declare const model: NativeModel;
        model.value;
      `,
      2,
    );

    expect(diagnostics).toEqual(["Property 'value' requires @Version(5) or an enclosing isVersionAtLeast(5) block"]);
  });

  it('lets a placeholder member version override native-contract inheritance', () => {
    const diagnostics = getDiagnosticTexts(
      `
        // @ExportModel
        export interface NativeModel {
          // @Version(__PLACEHOLDER__)
          futureValue?: string;
        }

        function render(model: NativeModel) {
          model.futureValue;
        }
      `,
      2,
    );

    expect(diagnostics).toEqual([
      "Property 'futureValue' requires @Version(__PLACEHOLDER__) or an enclosing isVersionAtLeast(__PLACEHOLDER__) block",
    ]);
  });

  it('allows native members above the workspace minimum inside a sufficient guard', () => {
    const diagnostics = getDiagnosticTexts(
      `
        declare function isVersionAtLeast(version: number): boolean;

        // @NativeInterface
        export interface NativeModel {
          // @Version(3)
          futureValue?: string;
        }

        function render(model: NativeModel) {
          if (isVersionAtLeast(3)) {
            model.futureValue;
          }
        }
      `,
      2,
    );

    expect(diagnostics).toEqual([]);
  });

  it('applies explicit versions to members inherited from native contracts', () => {
    const diagnostics = getDiagnosticTexts(
      `
        // @ExportProxy
        export interface NativeBase {
          // @Version(3)
          futureValue?: string;
        }

        export interface NativeDerived extends NativeBase {}

        function render(model: NativeDerived) {
          model.futureValue;
        }
      `,
      2,
    );

    expect(diagnostics).toEqual([
      "Property 'futureValue' requires @Version(3) or an enclosing isVersionAtLeast(3) block",
    ]);
  });

  it('applies the workspace minimum to exports in an ExportModule file', () => {
    const diagnostics = getDiagnosticTexts(
      `
        /** @ExportModule */
        export function existingFunction(): void {}

        // @Version(3)
        export function futureFunction(): void {}

        function render() {
          existingFunction();
          futureFunction();
        }
      `,
      2,
    );

    expect(diagnostics).toEqual(['Function call requires @Version(3) or an enclosing isVersionAtLeast(3) block']);
  });

  it('does not implicitly version ordinary TypeScript declarations', () => {
    const diagnostics = getDiagnosticTexts(
      `
        interface Model {
          value: string;
        }

        function makeModel(): Model {
          return { value: 'value' };
        }

        function render() {
          makeModel().value;
        }
      `,
      2,
    );

    expect(diagnostics).toEqual([]);
  });
});
