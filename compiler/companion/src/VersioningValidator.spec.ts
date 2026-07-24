import 'ts-jest';
import * as ts from 'typescript';
import { Workspace } from './Workspace';

function createWorkspaceWithFile(contents: string, nativeApiMinVersion: number | undefined): Workspace {
  const workspace = new Workspace(
    '/',
    false,
    undefined,
    {
      target: ts.ScriptTarget.ESNext,
      module: ts.ModuleKind.CommonJS,
      lib: ['lib.es2015.d.ts'],
      strict: true,
    },
    nativeApiMinVersion,
  );

  workspace.registerInMemoryFile('/file.ts', contents);
  workspace.addSourceFileAtPath('/file.ts');
  return workspace;
}

function getDiagnosticTexts(contents: string, nativeApiMinVersion?: number): string[] {
  const workspace = createWorkspaceWithFile(contents, nativeApiMinVersion);
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

  it('requires interfaces extending versioned types to be versioned', () => {
    const diagnostics = getDiagnosticTexts(`
      // @Version(42)
      interface NewModel {
        title: string;
      }

      interface ExtendedModel extends NewModel {
        subtitle: string;
      }
    `);

    expect(diagnostics).toEqual(["Type 'NewModel' requires @Version(42) on the containing declaration"]);
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
