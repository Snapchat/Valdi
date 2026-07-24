import { AST } from './AST';
import { BatchMinifyResult } from './BatchMinifier';
import { CodeInstrumentationFileConfig, CodeInstrumentationResult } from './CodeInstrumentation';

export enum Command {
  batchMinifyJS = 'batchMinifyJS',
  register = 'registerFile',
  open = 'openFile',
  emit = 'emitFile',
  addCodeInstrumentation = 'addCodeInstrumentation',
  getDiagnostics = 'getDiagnostics',
  createWorkspace = 'createWorkspace',
  destroyWorkspace = 'destroyWorkspace',
  initializeWorkspace = 'initializeWorkspace',
  updatedAndroidTargets = 'updatedAndroidTargets',
  dumpSymbolsWithComments = 'dumpSymbolsWithComments',
  dumpInterface = 'dumpInterface',
  dumpFunction = 'dumpFunction',
  dumpEnum = 'dumpEnum',
  generateIdsFiles = 'generateIdsFiles',
  exportStringsFiles = 'exportStringsFiles',
  exportTranslationStrings = 'exportTranslationStrings',
  exportStringsFilesBatch = 'exportStringsFilesBatch',
  startDebuggingProxy = 'startDebuggingProxy',
  compileNative = 'compileNative',
  uploadArtifact = 'uploadArtifact',
  updatedDebuggerPorts = 'updatedDebuggerPorts',
  rewriteImports = 'rewriteImports',
}

export interface Request {
  id: string;
  command: Command;
  body: RequestBody;
}

export interface SuccessPayload {
  id: string;
  body: ResponseBody;
}

export interface ErrorPayload {
  id: string;
  error: string;
}

export interface EventBody {
  type: string;
  message: string;
}

export interface EventPayload {
  event: EventBody;
}

export type TransportPayload = SuccessPayload | ErrorPayload | EventPayload;

export interface BatchMinifyJSRequestBody {
  inputFiles: string[];
  minifier: 'terser';
  options: string;
}

export interface CreateWorkspaceRequestBody {
  nativeApiMinVersion?: number;
}

export interface DestroyWorkspaceRequestBody {
  workspaceId: number;
}

export interface WorkspaceScopedRequestBodyBase {
  workspaceId: number;
}

export interface InitializeWorkspaceRequestBody extends WorkspaceScopedRequestBodyBase {}

export interface RegisterFileRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
  // If set, the file will be registered as a disk file
  // where the content can be read from this path.
  absoluteDiskPath?: string;
  // If set, the file will be registered as an in-memory file
  // where the content will be read from this property.
  fileContent?: string;
}

export interface OpenFileRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
}

export interface EmitFileRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
}

export interface AddCodeInstrumentationRequestBody {
  files: [CodeInstrumentationFileConfig];
}

export interface GetDiagnosticsRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
}

export interface DumpSymbolsWithCommentsRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
}

export interface DumpInterfaceRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
  position: number;
}

export interface DumpFunctionRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
  position: number;
}

export interface DumpEnumRequestBody extends WorkspaceScopedRequestBodyBase {
  fileName: string;
  position: number;
}

export interface UpdatedAndroidTargetsRequestBody {
  targets: { deviceId: string; target: string }[];
}

export interface GenerateIdsFilesRequestBody {
  moduleName: string;
  iosHeaderImportPath: string;
  filePath: string;
}

export interface ExportStringsFilesRequestBody {
  moduleName: string;
  inputPath: string;
  iosOutputPath?: string;
  androidOutputPath?: string;
}

export interface ExportTranslationStringsRequestBody {
  /** Module name */
  moduleName: string;
  /** Path to the base locale, "en", file */
  baseLocaleStringsPath: string;
  /** Path to the locale file to be converted */
  inputLocaleStringsPath: string;
  /** Are we exporting strings for Android or iOS? */
  platform: 'android' | 'ios';
}

export interface ExportStringsFilesBatchRequestBody {
  batch: ExportStringsFilesRequestBody[];
}

export interface StartDebuggingProxyRequestBody {}

export interface CompileNativeRequestBody extends WorkspaceScopedRequestBodyBase {
  stripIncludePrefix: string;
  inputFiles: string[];
  // Whether the input files should be registered into the workspace as
  // references to files on disk
  registerInputFiles?: boolean;
  // If set, will store the output into the given path.
  outputFile?: string;
}

export interface UploadArtifactRequestBody {
  artifactName: string;
  artifactData: string;
  sha256: string;
}

export interface UpdatedDebuggerPorts {
  ports: number[];
}

export interface RewriteImportsRequestBody {
  projectDirectory: string;
  oldTypeName: string;
  newTypeName: string;
  oldImportPath: string;
  newImportPath: string;
}

export type RequestBody =
  | BatchMinifyJSRequestBody
  | RegisterFileRequestBody
  | OpenFileRequestBody
  | EmitFileRequestBody
  | GetDiagnosticsRequestBody
  | CreateWorkspaceRequestBody
  | DestroyWorkspaceRequestBody
  | InitializeWorkspaceRequestBody
  | UpdatedAndroidTargetsRequestBody
  | GenerateIdsFilesRequestBody
  | ExportStringsFilesRequestBody
  | ExportStringsFilesBatchRequestBody
  | StartDebuggingProxyRequestBody
  | CompileNativeRequestBody
  | AddCodeInstrumentationRequestBody
  | UploadArtifactRequestBody
  | UpdatedDebuggerPorts
  | RewriteImportsRequestBody;

export interface BatchMinifyJSResponseBody {
  results: BatchMinifyResult[];
}

export interface RegisterFileResponseBody {}

export interface OpenFileResponseBody {
  openResult: {
    importPaths: {
      relative: string;
      absolute: string;
    }[];
  };
}

export interface EmittedFile {
  fileName: string;
  content?: string;
}

export interface EmitFileResponseBody {
  emitted: boolean;
  files: EmittedFile[];
}

export interface Location {
  line: number;
  offset: number;
}

export interface Diagnostic {
  start?: Location | undefined;
  end?: Location | undefined;
  text: string;
  category: string;
  code?: number;
  source?: string;
}

export interface GetDiagnosticsResponseBody {
  diagnostics: Diagnostic[];
  fileContent: string | undefined;
  hasError: boolean;
  timeTakenMs: number;
}

export interface DumpedSymbolsWithComments {
  leadingComments?: AST.Comments;
  dumpedSymbols: {
    interface?: AST.Interface;
    leadingComments?: AST.Comments;
  }[];
  references: AST.TypeReference[];
}

export interface DumpSymbolsWithCommentsResponseBody {
  dumpedSymbols: DumpedSymbolsWithComments;
}

export interface DumpedInterface {
  interface: AST.Interface | undefined;
  references: AST.TypeReference[];
}

export interface DumpInterfaceResponseBody {
  interface: DumpedInterface;
}

export interface DumpFunctionResponseBody {
  function: AST.Function;
  references: AST.TypeReference[];
}

export interface DumpEnumResponseBody {
  enum: AST.Enum;
}

export interface CreateWorkspaceResponseBody {
  workspaceId: number;
}

export interface DestroyWorkspaceResponseBody {}

export interface InitializeWorkspaceResponseBody {}

export interface UpdatedAndroidTargetsResponseBody {}

export interface GenerateIdsFilesResponseBody {
  android: string;
  iosHeader: string;
  iosImplementation: string;
  typescriptDefinition: string;
  typescriptImplementation: string;
}

export interface ExportStringsFilesResponseBody {}

export interface ExportTranslationStringsResponseBody {
  /** Locale name used in the input locale file.
   *
   * iOS and Android use different locale names.
   * Valdi uses its own locale names to unify across platforms,
   * this is the valdi locale name.
   */
  inputLocale: string;
  /** Platform specific locale name. */
  platformLocale: string;
  /** Platform specific translation file representation. */
  content: string;
  /** Platform specific file name. */
  outputFileName: string;
}

export interface ExportStringsFilesBatchResponseBody {}

export interface StartDebuggingProxyResponseBody {
  actualPort: number;
}

export interface CompileNativeResponseBody {
  files: EmittedFile[];
}

export interface AddCodeInstrumentationResponseBody {
  results: CodeInstrumentationResult[];
}

export interface UploadArtifactResponseBody {
  url: string;
  sha256: string;
}

export interface RewriteImportsResponseBody {}

export type ResponseBody =
  | BatchMinifyJSResponseBody
  | RegisterFileResponseBody
  | OpenFileResponseBody
  | EmitFileResponseBody
  | GetDiagnosticsResponseBody
  | DumpSymbolsWithCommentsResponseBody
  | CreateWorkspaceResponseBody
  | DestroyWorkspaceResponseBody
  | InitializeWorkspaceResponseBody
  | UpdatedAndroidTargetsResponseBody
  | DumpInterfaceResponseBody
  | DumpFunctionResponseBody
  | DumpEnumResponseBody
  | GenerateIdsFilesResponseBody
  | ExportStringsFilesResponseBody
  | ExportStringsFilesBatchResponseBody
  | StartDebuggingProxyResponseBody
  | CompileNativeResponseBody
  | AddCodeInstrumentationResponseBody
  | RewriteImportsResponseBody;
