import { batchMinify, CommonMinifyOptions } from './BatchMinifier';
import {
  AddCodeInstrumentationRequestBody,
  BatchMinifyJSRequestBody,
  Command,
  CompileNativeRequestBody,
  DumpEnumRequestBody,
  DumpFunctionRequestBody,
  DumpInterfaceRequestBody,
  DumpSymbolsWithCommentsRequestBody,
  EmitFileRequestBody,
  EmitFileResponseBody,
  EmittedFile,
  ExportStringsFilesBatchRequestBody,
  ExportStringsFilesBatchResponseBody,
  ExportStringsFilesRequestBody,
  ExportTranslationStringsRequestBody,
  ExportStringsFilesResponseBody,
  GenerateIdsFilesRequestBody,
  GenerateIdsFilesResponseBody,
  GetDiagnosticsRequestBody,
  GetDiagnosticsResponseBody,
  InitializeWorkspaceRequestBody,
  OpenFileRequestBody,
  RegisterFileRequestBody,
  RewriteImportsRequestBody,
  RewriteImportsResponseBody,
  StartDebuggingProxyResponseBody,
  UpdatedAndroidTargetsRequestBody,
  UpdatedDebuggerPorts,
  CreateWorkspaceRequestBody,
  CreateWorkspaceResponseBody,
  DestroyWorkspaceRequestBody,
  DestroyWorkspaceResponseBody,
  BatchMinifyJSResponseBody,
  InitializeWorkspaceResponseBody,
  RegisterFileResponseBody,
  OpenFileResponseBody,
  AddCodeInstrumentationResponseBody,
  UpdatedAndroidTargetsResponseBody,
  DumpSymbolsWithCommentsResponseBody,
  DumpInterfaceResponseBody,
  DumpEnumResponseBody,
  DumpFunctionResponseBody,
  ExportTranslationStringsResponseBody,
  StartDebuggingProxyRequestBody,
  CompileNativeResponseBody,
} from './protocol';
import { DebuggingProxy } from './DebuggingProxy';
import { generateIdsFromPath } from './GenerateIds';
import { exportStringsFiles, exportTranslationStrings } from './strings/exportStringsFiles';
import { getArgumentValue } from './utils/getArgumentValue';
import { compileNative } from './native/CompileNativeCommand';
import { CodeInstrumentation } from './CodeInstrumentation';
import { rewriteImports } from './cli/rewriteImports';
import { WorkspaceStore } from './WorkspaceStore';
import { CompanionServiceBase, ExitKind } from './CompanionServiceBase';

export class CompilerCompanion extends CompanionServiceBase {
  private workspaceStore: WorkspaceStore;
  private debuggingProxy: DebuggingProxy;

  constructor(options: Set<string>) {
    super(options);
    let shouldDebounceOpenFile = true;
    if (options.has('--command')) {
      shouldDebounceOpenFile = false;
    }

    this.debuggingProxy = new DebuggingProxy(this.logger);

    let cacheDir: string | undefined;
    if (options.has('--cache-dir')) {
      cacheDir = getArgumentValue('--cache-dir');
    }

    this.workspaceStore = new WorkspaceStore(this.logger, cacheDir, shouldDebounceOpenFile);

    this.addBaseEndpoints();
  }

  private addBaseEndpoints(): void {
    this.addEndpoint<BatchMinifyJSRequestBody, BatchMinifyJSResponseBody>(Command.batchMinifyJS, async (body) => {
      const inputFiles: string[] = body.inputFiles;
      if (!inputFiles) {
        throw Error('Missing inputFiles in request');
      }

      let options: CommonMinifyOptions;
      try {
        options = JSON.parse(body.options);
      } catch (err) {
        throw Error(`Error while parsing minifying options: ${err}`);
      }

      try {
        const batchMinifyResults = await batchMinify(body.minifier, inputFiles, options);
        return { results: batchMinifyResults };
      } catch (err) {
        throw Error(`Error while minifying: ${err}`);
      }
    });

    this.addEndpoint<CreateWorkspaceRequestBody, CreateWorkspaceResponseBody>(Command.createWorkspace, async (body) => {
      const workspaceId = this.workspaceStore.createWorkspace(body.nativeApiMinVersion).workspaceId;
      return { workspaceId };
    });

    this.addEndpoint<DestroyWorkspaceRequestBody, DestroyWorkspaceResponseBody>(
      Command.destroyWorkspace,
      async (body) => {
        this.workspaceStore.destroyWorkspace(body.workspaceId);
        const response: DestroyWorkspaceResponseBody = {};
        return response;
      },
    );

    this.addEndpoint<InitializeWorkspaceRequestBody, InitializeWorkspaceResponseBody>(
      Command.initializeWorkspace,
      async (body) => {
        const workspace = this.workspaceStore.getWorkspace(body.workspaceId);
        if (!workspace.initialized) {
          workspace.initialize();
        }
        return {};
      },
    );

    this.addEndpoint<RegisterFileRequestBody, RegisterFileResponseBody>(Command.register, async (body) => {
      const workspace = this.workspaceStore.getWorkspace(body.workspaceId);
      if (body.fileContent) {
        workspace.registerInMemoryFile(body.fileName, body.fileContent);
      } else if (body.absoluteDiskPath) {
        workspace.registerDiskFile(body.fileName, body.absoluteDiskPath);
      } else {
        throw new Error('Should have either fileContent or absoluteDiskPath set');
      }
      return {};
    });

    this.addEndpoint<OpenFileRequestBody, OpenFileResponseBody>(Command.open, async (body) => {
      const workspace = this.workspaceStore.getInitializedWorkspace(body.workspaceId);
      const openResult = await workspace.openFile(body.fileName);
      return { openResult: { importPaths: openResult.importPaths } };
    });

    this.addEndpoint<EmitFileRequestBody, EmitFileResponseBody>(Command.emit, async (body) => {
      const workspace = this.workspaceStore.getInitializedWorkspace(body.workspaceId);

      const emitResult = await workspace.emitFile(body.fileName);
      const output: EmitFileResponseBody = {
        emitted: emitResult.emitted,
        files: emitResult.entries.map((e): EmittedFile => {
          return { fileName: e.fileName, content: e.content };
        }),
      };

      return output;
    });

    this.addEndpoint<AddCodeInstrumentationRequestBody, AddCodeInstrumentationResponseBody>(
      Command.addCodeInstrumentation,
      async (body) => {
        const codeInstrumentation = CodeInstrumentation.getInstance();

        return codeInstrumentation.instrumentFiles(body.files);
      },
    );

    this.addEndpoint<GetDiagnosticsRequestBody, GetDiagnosticsResponseBody>(Command.getDiagnostics, async (body) => {
      const workspace = this.workspaceStore.getInitializedWorkspace(body.workspaceId);
      const diagnostics = await workspace.getDiagnostics(body.fileName);
      const output: GetDiagnosticsResponseBody = diagnostics;
      return output;
    });

    this.addEndpoint<UpdatedAndroidTargetsRequestBody, UpdatedAndroidTargetsResponseBody>(
      Command.updatedAndroidTargets,
      async (body) => {
        const mapped = body.targets.map(({ deviceId, target }) => {
          return { deviceId, endpoint: target };
        });
        this.debuggingProxy.updateAvailableAndroidDevices(mapped);
        return {};
      },
    );

    this.addEndpoint<DumpSymbolsWithCommentsRequestBody, DumpSymbolsWithCommentsResponseBody>(
      Command.dumpSymbolsWithComments,
      async (body) => {
        const workspace = this.workspaceStore.getInitializedWorkspace(body.workspaceId);
        const result = await workspace.dumpSymbolsWithComments(body.fileName);
        return { dumpedSymbols: result };
      },
    );

    this.addEndpoint<DumpInterfaceRequestBody, DumpInterfaceResponseBody>(Command.dumpInterface, async (body) => {
      const workspace = this.workspaceStore.getInitializedWorkspace(body.workspaceId);
      const result = await workspace.getInterfaceAST(body.fileName, body.position);
      return { interface: result };
    });

    this.addEndpoint<DumpEnumRequestBody, DumpEnumResponseBody>(Command.dumpEnum, async (body) => {
      const workspace = this.workspaceStore.getInitializedWorkspace(body.workspaceId);
      return await workspace.getEnumAST(body.fileName, body.position);
    });

    this.addEndpoint<DumpFunctionRequestBody, DumpFunctionResponseBody>(Command.dumpFunction, async (body) => {
      const workspace = this.workspaceStore.getInitializedWorkspace(body.workspaceId);
      return await workspace.getFunctionAST(body.fileName, body.position);
    });

    this.addEndpoint<GenerateIdsFilesRequestBody, GenerateIdsFilesResponseBody>(
      Command.generateIdsFiles,
      async (body) => {
        const result = await generateIdsFromPath(body.moduleName, body.iosHeaderImportPath, body.filePath);
        const typedResponse: GenerateIdsFilesResponseBody = {
          android: result.android,
          iosHeader: result.ios.header,
          iosImplementation: result.ios.impl,
          typescriptDefinition: result.typescript.definition,
          typescriptImplementation: result.typescript.implementation,
        };
        return typedResponse;
      },
    );

    this.addEndpoint<ExportStringsFilesRequestBody, ExportStringsFilesResponseBody>(
      Command.exportStringsFiles,
      async (body) => {
        await exportStringsFiles(body.moduleName, body.inputPath, body.iosOutputPath, body.androidOutputPath);
        const typedResponse: ExportStringsFilesResponseBody = {};
        return typedResponse;
      },
    );

    this.addEndpoint<ExportTranslationStringsRequestBody, ExportTranslationStringsResponseBody>(
      Command.exportTranslationStrings,
      async (body) => {
        const result = await exportTranslationStrings(body, this.logger);
        return result;
      },
    );

    this.addEndpoint<ExportStringsFilesBatchRequestBody, ExportStringsFilesBatchResponseBody>(
      Command.exportStringsFilesBatch,
      async (body) => {
        const promises = body.batch.map((body) => {
          exportStringsFiles(body.moduleName, body.inputPath, body.iosOutputPath, body.androidOutputPath);
        });

        await Promise.all(promises);

        const typedResponse: ExportStringsFilesBatchResponseBody = {};
        return typedResponse;
      },
    );

    this.addEndpoint<StartDebuggingProxyRequestBody, StartDebuggingProxyResponseBody>(
      Command.startDebuggingProxy,
      async (body) => {
        const actualPort = await this.debuggingProxy.start();
        const typedResponse: StartDebuggingProxyResponseBody = {
          actualPort,
        };
        return typedResponse;
      },
    );

    this.addEndpoint<CompileNativeRequestBody, CompileNativeResponseBody>(Command.compileNative, async (body) => {
      let workspaceId: number;
      if (!body.workspaceId && body.registerInputFiles) {
        workspaceId = this.workspaceStore.createWorkspace(undefined).workspaceId;
      } else {
        workspaceId = body.workspaceId;
      }
      const uncachedWorkspace = this.workspaceStore.getUncachedWorkspace(workspaceId);
      return await compileNative(uncachedWorkspace, this.logger, body);
    });

    this.addEndpoint<UpdatedDebuggerPorts, {}>(Command.updatedDebuggerPorts, async (body) => {
      const mapped = body.ports.map((port) => {
        return { port: port };
      });
      this.debuggingProxy.updateAvailableHermesDevices(mapped);
      return {};
    });

    this.addEndpoint<RewriteImportsRequestBody, RewriteImportsResponseBody>(Command.rewriteImports, async (body) => {
      await rewriteImports(
        this.logger,
        body.projectDirectory,
        body.oldTypeName,
        body.newTypeName,
        body.oldImportPath,
        body.newImportPath,
      );
      const response: RewriteImportsResponseBody = {};
      return response;
    });
  }

  protected override handleExit(exitKind: ExitKind): void {
    this.debuggingProxy?.stop();
    this.workspaceStore?.destroyAllWorkspaces();

    super.handleExit(exitKind);
  }
}
