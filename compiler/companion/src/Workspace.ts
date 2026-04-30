import * as path from 'path';
import {
  AST,
  dumpEnum,
  dumpFunction,
  dumpInterface,
  dumpRootNodes,
  getImportStringLiteralsFromSourceFile,
  isNonModuleWithAmbiantDeclarations,
} from './AST';
import { ILogger } from './logger/ILogger';
import { JSXProcessor } from './JSXProcessor';
import { createConsoleLogTransformer } from './ConsoleLogTransformer';
import {
  Diagnostic,
  DumpedInterface,
  DumpedSymbolsWithComments,
  DumpEnumResponseBody,
  DumpFunctionResponseBody,
  Location,
} from './protocol';
import { findNode } from './TSUtils';
import { IProjectListener, Project } from './project/Project';
import * as ts from 'typescript';
import { Stopwatch } from './utils/Stopwatch';
import { IWorkspace, OpenFileImportPath, OpenFileResult } from './IWorkspace';
import * as _path from 'path';
import { ImportPathResolver } from './utils/ImportPathResolver';
import { debounce } from 'lodash';

export interface OpenedFile {
  sourceFile: ts.SourceFile;
  workspaceProject: Project;
}

export interface EmitResultEntry {
  fileName: string;
  content: string;
}

export interface EmitResult {
  entries: EmitResultEntry[];
  emitted: boolean;
}

export interface GetDiagnosticsResult {
  diagnostics: Diagnostic[];
  fileContent: string | undefined;
  hasError: boolean;
  timeTakenMs: number;
}

interface PendingFileToOpen {
  fileName: string;
  resolve: (result: OpenFileResult) => void;
  reject: (error: unknown) => void;
}

function mergeCustomTransformers(
  transformers: ts.CustomTransformers,
  newTransformers: ts.CustomTransformers | undefined,
): ts.CustomTransformers {
  if (!newTransformers) {
    return transformers;
  }

  const out = { ...transformers };

  if (newTransformers.after) {
    if (!out.after) {
      out.after = [];
    }
    out.after.push(...newTransformers.after);
  }

  if (newTransformers.before) {
    if (!out.before) {
      out.before = [];
    }

    out.before.push(...newTransformers.before);
  }

  return out;
}

class ProjectListener implements IProjectListener {
  private stopWatch = new Stopwatch();
  constructor(private readonly logger: ILogger) {}

  onProgramWillChange(): void {
    this.stopWatch.start();
    this.logger.debug?.('Creating TypeScript program...');
  }

  onProgramChanged(): void {
    const elapsed = this.stopWatch.elapsedString;
    this.logger.debug?.(`Created TypeScript program in ${elapsed}`);
  }

  onCompilerOptionsResolved(): void {}
  onFileContentLoaded(path: string): void {}
  onSourceFileCreated(path: string): void {}
  onSourceFileInvalidated(path: string): void {}
}

export class Workspace implements IWorkspace {
  workspaceRoot: string;
  initialized: boolean = false;

  private project: Project;
  private importPathResolver: ImportPathResolver;

  private pendingFilesToOpen: PendingFileToOpen[] = [];
  private scheduleFlushOpenQueuedFiles: () => void;

  constructor(
    workspaceRoot: string,
    shouldDebounceOpenFile: boolean,
    readonly logger: ILogger | undefined,
    readonly compilerOptions: ts.CompilerOptions | undefined,
  ) {
    this.workspaceRoot = workspaceRoot;
    const project = new Project(workspaceRoot, compilerOptions, logger ? new ProjectListener(logger) : undefined);
    this.project = project;
    this.importPathResolver = new ImportPathResolver(() => project.compilerOptions);

    const flushOpenQueuedFiles = () => {
      this.openQueuedFiles();
    };
    if (shouldDebounceOpenFile) {
      this.scheduleFlushOpenQueuedFiles = debounce(flushOpenQueuedFiles, 20, { trailing: true });
    } else {
      this.scheduleFlushOpenQueuedFiles = flushOpenQueuedFiles;
    }
  }

  initialize(): void {
    const tsConfigCompilerOptions = this.project.compilerOptions;
    // Doing this to make sure we automatically "open" any files referred to in
    // tsconfig.compilerOptions.types _if they are present_.
    //
    // Note: we seem to be using tsconfig.compilerOptions.types in a _very_
    // unexpected way (see docs: https://www.typescriptlang.org/tsconfig#types)
    //
    // It seems likely that the fact that this lets us make Long.d.ts available globally
    // is accidental.
    const typesFiles = tsConfigCompilerOptions.types ?? [];
    const basePath = (tsConfigCompilerOptions.typeRoots ?? [''])[0];
    const presentTypesFiles = typesFiles
      .map((unresolvedTypesFilePath) => path.resolve(basePath, unresolvedTypesFilePath + '.d.ts'))
      .filter((f) => this.project.fileExists(f));

    presentTypesFiles.forEach((typesFilePath) => this.doOpenFile(typesFilePath));
    this.initialized = true;
  }

  destroy(): void {
    this.project.destroy();
  }

  addSourceFileAtPath(fileName: string) {
    this.project.openSourceFile(fileName);
  }

  async openFile(fileName: string): Promise<OpenFileResult> {
    return new Promise((resolve, reject) => {
      const pendingFile: PendingFileToOpen = {
        fileName: fileName,
        resolve,
        reject,
      };
      this.pendingFilesToOpen.push(pendingFile);
      this.scheduleFlushOpenQueuedFiles();
    });
  }

  private doOpenFile(fileName: string): OpenFileResult {
    this.logger?.debug?.(`Opening file ${fileName}`);
    const sourceFile = this.project.openSourceFile(fileName);
    const relativeImportPaths = getImportStringLiteralsFromSourceFile(sourceFile).map((lit) => lit.text);
    const isTypeScriptFile = fileName.endsWith('.ts') || fileName.endsWith('.tsx');
    const importPaths: OpenFileImportPath[] = relativeImportPaths.map((i) => {
      return { relative: i, absolute: this.resolveImportPath(sourceFile.fileName, i) };
    });

    return {
      importPaths,
      isNonModuleWithAmbiantDeclarations: isTypeScriptFile && isNonModuleWithAmbiantDeclarations(sourceFile),
    };
  }

  private openQueuedFiles(): void {
    while (this.pendingFilesToOpen.length > 0) {
      const fileToOpen = this.pendingFilesToOpen.shift()!;

      try {
        fileToOpen.resolve(this.doOpenFile(fileToOpen.fileName));
      } catch (err: unknown) {
        fileToOpen.reject(err);
      }
    }
  }

  getOpenedFile(fileName: string): OpenedFile {
    const sourceFile = this.project.getOpenedSourceFile(fileName);
    if (sourceFile) {
      return { workspaceProject: this.project, sourceFile };
    }

    throw new Error(`File '${fileName}' is not loaded`);
  }

  registerInMemoryFile(fileName: string, fileContent: string): void {
    const resolvedPath = this.project.resolvePath(fileName);
    this.importPathResolver.registerAlias(resolvedPath);
    this.project.setFileContentAtResolvedPath(resolvedPath, fileContent);
  }

  registerDiskFile(fileName: string, absoluteDiskPath: string): void {
    const resolvedPath = this.project.resolvePath(fileName);
    this.importPathResolver.registerAlias(resolvedPath);
    this.project.setFileFromDiskAtResolvedPath(resolvedPath, absoluteDiskPath);
  }

  static requiresTSXProcessor(fileName: string): boolean {
    return fileName.endsWith('.tsx') || fileName.endsWith('.jsx');
  }

  doEmitFile(
    fileName: string,
    cancellationToken: ts.CancellationToken | undefined,
    customTransformers: ts.CustomTransformers | undefined,
  ): EmitResult {
    const openedFile = this.getOpenedFile(fileName);
    const program = openedFile.workspaceProject.program;

    let resolvedCustomTransformers: ts.CustomTransformers = {
      before: [],
    };

    if (Workspace.requiresTSXProcessor(fileName)) {
      const jsxProcessor = new JSXProcessor(this.logger);
      resolvedCustomTransformers.before!.push(
        ...jsxProcessor.makeTransformers(openedFile.workspaceProject.typeChecker),
      );
    }

    // Wrap console.log/warn/error/info/debug calls in runtime.isLoggingEnabled guards.
    // This is safe in dev builds because runtime.isLoggingEnabled is always true.
    resolvedCustomTransformers.before!.push(createConsoleLogTransformer());

    resolvedCustomTransformers = mergeCustomTransformers(resolvedCustomTransformers, customTransformers);

    const output: EmitResult = { entries: [], emitted: false };
    const programResult = program.emit(
      openedFile.sourceFile,
      (fileName: string, text: string, writeByteOrderMark: boolean) => {
        output.entries.push({
          fileName,
          content: text,
        });
      },
      cancellationToken,
      undefined,
      resolvedCustomTransformers,
    );

    output.emitted = !programResult.emitSkipped;

    return output;
  }

  async emitFile(fileName: string): Promise<EmitResult> {
    return this.doEmitFile(fileName, undefined, undefined);
  }

  transformDiagnostic(project: Project, original: ts.DiagnosticWithLocation | ts.Diagnostic): Diagnostic {
    const text = project.formatDiagnostic(original);

    let category: string;
    switch (original.category) {
      case ts.DiagnosticCategory.Warning:
        category = 'warning';
        break;
      case ts.DiagnosticCategory.Error:
        category = 'error';
        break;
      case ts.DiagnosticCategory.Suggestion:
        category = 'suggestion';
        break;
      case ts.DiagnosticCategory.Message:
        category = 'message';
        break;
    }

    let file = original.file ?? (original.source && project.getOpenedSourceFile(original.source));

    let start: Location;
    let end: Location;
    if (file && original.start) {
      start = this.getLocationOfPositionInFile(file, original.start);
      end = this.getLocationOfPositionInFile(file, original.start + (original.length ?? 0));
    } else {
      start = { line: 0, offset: 0 };
      end = { line: 0, offset: 0 };
    }

    const transformed = {
      start,
      end,
      text,
      category,
    };
    return transformed;
  }

  getLocationOfPositionInFile(file: ts.SourceFile, position: number): Location {
    const lac = file.getLineAndCharacterOfPosition(position);
    // Location is 1-indexed
    return { line: lac.line + 1, offset: lac.character + 1 };
  }

  private doGetDiagnostics(openedFile: OpenedFile, syntactic: boolean, output: Diagnostic[]): boolean {
    const program = openedFile.workspaceProject.program;
    const diagnostics = syntactic
      ? program.getSyntacticDiagnostics(openedFile.sourceFile)
      : program.getSemanticDiagnostics(openedFile.sourceFile);
    let success = true;
    for (const diagnostic of diagnostics) {
      const transformedDiagnostic = this.transformDiagnostic(openedFile.workspaceProject, diagnostic);
      output.push(transformedDiagnostic);

      if (transformedDiagnostic.category === 'error') {
        success = false;
      }
    }

    return success;
  }

  private makeDiagnostic(sourceFile: ts.SourceFile, node: ts.Node, text: string): Diagnostic {
    const fullStart = node.getFullStart();
    const start = this.getLocationOfPositionInFile(sourceFile, fullStart);
    const end = this.getLocationOfPositionInFile(sourceFile, fullStart + node.getFullWidth());

    return { start, end, text, category: 'error' };
  }

  private validateImports(openedFile: OpenedFile, output: Diagnostic[]): boolean {
    const importPaths = getImportStringLiteralsFromSourceFile(openedFile.sourceFile);
    let success = true;

    for (const importPath of importPaths) {
      const resolvedImportPath = this.resolveImportPath(openedFile.sourceFile.fileName, importPath.text);
      const resolvedPath = this.project.resolvePath(resolvedImportPath);
      const pathToCheck = this.importPathResolver.getAlias(resolvedPath) ?? resolvedPath;

      if (!this.project.fileExists(pathToCheck.absolutePath)) {
        const errorMessage = `File '${importPath.text}' does not exist`;
        output.push(this.makeDiagnostic(openedFile.sourceFile, importPath, errorMessage));
        success = false;
      }
    }

    return success;
  }

  async getDiagnostics(fileName: string): Promise<GetDiagnosticsResult> {
    return this.getDiagnosticsSync(fileName);
  }

  getDiagnosticsSync(fileName: string): GetDiagnosticsResult {
    const sw = new Stopwatch();
    const openedFile = this.getOpenedFile(fileName);
    const diagnostics: Diagnostic[] = [];

    if (!this.doGetDiagnostics(openedFile, true, diagnostics)) {
      return {
        diagnostics,
        fileContent: openedFile.sourceFile.text,
        hasError: true,
        timeTakenMs: sw.elapsedMilliseconds,
      };
    }

    if (!this.validateImports(openedFile, diagnostics)) {
      return {
        diagnostics,
        fileContent: openedFile.sourceFile.text,
        hasError: true,
        timeTakenMs: sw.elapsedMilliseconds,
      };
    }

    if (!fileName.endsWith('.js')) {
      if (!this.doGetDiagnostics(openedFile, false, diagnostics)) {
        return {
          diagnostics,
          fileContent: openedFile.sourceFile.text,
          hasError: true,
          timeTakenMs: sw.elapsedMilliseconds,
        };
      }
    }

    return {
      diagnostics,
      fileContent: undefined,
      hasError: false,
      timeTakenMs: sw.elapsedMilliseconds,
    };
  }

  async dumpSymbolsWithComments(fileName: string): Promise<DumpedSymbolsWithComments> {
    const openedFile = this.getOpenedFile(fileName);

    const references: AST.TypeReference[] = [];

    const astReferences: AST.TypeReferences = {
      typeChecker: openedFile.workspaceProject.typeChecker,
      references: references,
    };

    const dumpedSymbols = dumpRootNodes(openedFile.sourceFile, astReferences);

    return {
      dumpedSymbols,
      references,
    };
  }

  async getInterfaceAST(fileName: string, position: number): Promise<DumpedInterface> {
    const openedFile = this.getOpenedFile(fileName);
    const node = findNode(openedFile.sourceFile, position, (node) => ts.isInterfaceDeclaration(node));
    if (!node) {
      throw new Error(`Could not resolve interface declaration at position ${position}`);
    }

    const references: AST.TypeReference[] = [];

    const dumpedInterface = dumpInterface(node as ts.InterfaceDeclaration, {
      typeChecker: openedFile.workspaceProject.typeChecker,
      references,
    });

    return {
      interface: dumpedInterface,
      references,
    };
  }

  async getFunctionAST(fileName: string, position: number): Promise<DumpFunctionResponseBody> {
    const openedFile = this.getOpenedFile(fileName);
    const node = findNode(openedFile.sourceFile, position, (node) => ts.isFunctionDeclaration(node));
    if (!node) {
      throw new Error(`Could not resolve function declaration at position ${position}`);
    }

    const references: AST.TypeReference[] = [];

    const dumpedFunction = dumpFunction(node as ts.FunctionDeclaration, {
      typeChecker: openedFile.workspaceProject.typeChecker,
      references,
    });

    return {
      function: dumpedFunction,
      references,
    };
  }

  async getEnumAST(fileName: string, position: number): Promise<DumpEnumResponseBody> {
    const openedFile = this.getOpenedFile(fileName);
    const node = findNode(openedFile.sourceFile, position, (node) => ts.isEnumDeclaration(node));
    if (!node) {
      throw new Error(`Could not resolve enum declaration at position ${position}`);
    }

    const dumpedEnum = dumpEnum(node as ts.EnumDeclaration);

    return {
      enum: dumpedEnum,
    };
  }

  resolveImportPath(fromPath: string, toPath: string): string {
    return this.importPathResolver.resolveImportPath(fromPath, toPath);
  }

  getFileNameForImportPath(importPath: string): string {
    const resolvedPath = this.project.resolvePath(importPath);
    return this.importPathResolver.getAlias(resolvedPath)?.absolutePath ?? resolvedPath.absolutePath;
  }
}
