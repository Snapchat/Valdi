//
//  File.swift
//
//
//  Created by Simon Corsin on 10/11/19.
//

import Foundation

enum CommandType: String {
    case processJSX
    case batchMinifyJS

    case createWorkspace
    case destroyWorkspace
    case initializeWorkspace
    case registerFile
    case openFile
    case emitFile
    case uploadArtifact
    case addCodeInstrumentation
    case getDiagnostics
    case dumpInterface
    case dumpSymbolsWithComments
    case dumpEnum
    case dumpFunction
    case generateIdsFiles
    case exportTranslationStrings
    case getLcaToken
    case compileNative

    case startDebuggingProxy
    case updatedAndroidTargets
    case updatedDebuggerPorts
}

private struct BatchMinifyJS: Command {
    static let type: CommandType = .batchMinifyJS

    struct Request: RequestBody {
        let inputFiles: [String]
        let minifier: String
        let options: String?
    }

    struct Response: ResponseBody {
        let results: [BatchMinifyResult]
    }
}

private struct CreateWorkspace: Command {
    static let type: CommandType = .createWorkspace

    struct Request: RequestBody {
        let nativeApiMinVersion: Int?
    }

    struct Response: ResponseBody {
        let workspaceId: Int
    }
}

private struct DestroyWorkspace: Command {
    static let type: CommandType = .destroyWorkspace

    struct Request: RequestBody {
        let workspaceId: Int
    }

    struct Response: ResponseBody {
    }
}

private struct InitializeWorkspace: Command {
    static let type: CommandType = .initializeWorkspace

    struct Request: RequestBody {
        let workspaceId: Int
    }

    struct Response: ResponseBody {

    }
}

private struct OpenFile: Command {
    static let type: CommandType = .openFile

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
    }

    struct Response: ResponseBody {
        let openResult: TS.OpenResponseBody
    }
}

private struct RegisterFile: Command {
    static let type: CommandType = .registerFile

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
        let absoluteDiskPath: String?
        let fileContent: String?
    }

    struct Response: ResponseBody {
    }
}

private struct EmitFile: Command {
    static let type: CommandType = .emitFile

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
    }

    struct Response: ResponseBody {
        let emitted: Bool
        let files: [TS.EmittedFile]
    }
}

private struct CompileNative: Command {
    static let type: CommandType = .compileNative

    struct Request: RequestBody {
        let workspaceId: Int
        let stripIncludePrefix: String
        let inputFiles: [String]
        let outputFile: String?
    }

    struct Response: ResponseBody {
        let files: [TS.EmittedFile]
    }
}

private struct UploadArtifact: Command {
    static let type: CommandType = .uploadArtifact

    struct Request: RequestBody {
        let artifactName: String
        let artifactData: Data
        let sha256: String
    }

    struct Response: ResponseBody {
        let sha256: String
        let url: URL
    }
}

private struct AddCodeInstrumentation: Command {
    static let type: CommandType = .addCodeInstrumentation

    struct Request: RequestBody {
        let files: [InstrumentationFileConfig]
    }

    struct Response: ResponseBody {
        let results: [CodeInstrumentationResult]
    }
}

private struct GetSyntacticDiagnostics: Command {
    static let type: CommandType = .getDiagnostics

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
    }

    struct Response: ResponseBody {
        // Will be provided if an error was found
        let fileContent: String?
        let diagnostics: [TS.Diagnostic]
        let hasError: Bool
        let timeTakenMs: Double
    }
}

struct GetErrorsResult {
    let error: CompilerError?
    let timeTakenMs: Double
}

struct StartDebuggingProxy: Command {
    static let type: CommandType = .startDebuggingProxy

    struct Request: RequestBody {
    }

    struct Response: ResponseBody {
        let actualPort: Int
    }
}

private struct UpdatedAndroidTargets: Command {
    static let type: CommandType = .updatedAndroidTargets

    struct Request: RequestBody {
        let targets: [AndroidDebuggingTarget]
    }

    struct Response: ResponseBody {

    }
}

private struct UpdatedDebuggerPorts: Command {
    static let type: CommandType = .updatedDebuggerPorts

    struct Request: RequestBody {
        let ports: [Int]
    }

    struct Response: ResponseBody {
    }
}

private struct DumpSymbolsWithComments: Command {
    static let type: CommandType = .dumpSymbolsWithComments

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
    }

    struct Response: ResponseBody {
        let dumpedSymbols: TS.DumpSymbolsWithCommentsResponseBody
    }
}

private struct DumpInterface: Command {
    static let type: CommandType = .dumpInterface

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
        let position: Int
    }

    struct Response: ResponseBody {
        let interface: TS.DumpInterfaceResponseBody
    }
}

private struct DumpEnum: Command {
    static let type: CommandType = .dumpEnum

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
        let position: Int
    }

    struct Response: ResponseBody {
        let `enum`: TS.DumpEnumResponseBody
    }
}

private struct DumpFunction: Command {
    static let type: CommandType = .dumpFunction

    struct Request: RequestBody {
        let workspaceId: Int
        let fileName: String
        let position: Int
    }

    struct Response: ResponseBody {
        let function: TS.DumpFunctionResponseBody
    }
}

private struct GenerateIds: Command {
    static let type: CommandType = .generateIdsFiles

    struct Request: RequestBody {
        let moduleName: String
        let filePath: String
        let iosHeaderImportPath: String
    }

    struct Response: ResponseBody {
        let android: String
        let iosHeader: String
        let iosImplementation: String
        let typescriptDefinition: String
        let typescriptImplementation: String
    }
}

/// Command to generate platform specific translation strings representation
struct ExportTranslationStrings: Command {
    static let type: CommandType = .exportTranslationStrings

    struct Request: RequestBody {
        /// Module name to generate the translation file for
        let moduleName: String
        /// Path to the strings-en.json aka base locale file
        let baseLocaleStringsPath: String
        /// Path to the strings-.json to generate the translation file for
        let inputLocaleStringsPath: String
        /// One of: 'android', 'ios'
        let platform: String
    }

    struct Response: ResponseBody {
        /// Input locale extracted from the passed `inputLocaleStringsPaths`
        let inputLocale: String
        /// Platform specific input locale name. Android and iOS call the same locale using different names
        let platformLocale: String
        /// The exported translation content
        let content: String
        /// The platform specific translation string name, including the folder, if necessary.
        let outputFileName: String
    }
}

private protocol Command {
    static var type: CommandType { get }
    associatedtype Request: RequestBody
    associatedtype Response: ResponseBody
}

private protocol RequestBody: Encodable {}
private protocol ResponseBody: Decodable {}

private struct RequestEnvelope<C: Command>: Encodable {
    let id: Int
    let command: String
    let body: C.Request

    init(id: Int, body: C.Request) {
        self.id = id
        self.command = C.type.rawValue
        self.body = body
    }
}

private struct EventPayload: Decodable {
    let type: String
    let message: String
}

private struct ResponseHeader: Decodable {
    let id: Int?
    let error: String?

    // Events don't have an id and can occur outside of the normal request/response back-and-forth
    let event: EventPayload?
}

private struct ResponseEnvelope<C: Command>: Decodable {
    let id: Int
    let body: C.Response
}

class CompanionExecutable {
    private let logger: ILogger

    let compilerCompanionBinaryPath: String
    let nodeOptions: [String]
    let logsOutputPath: String?
    let compilerCacheURL: URL?
    let isBazelBuild: Bool

    private(set) var processExited = false

    private let queue = DispatchQueue(label: "CompanionExecutable")
    private var processHandle: AsyncProcessHandle?
    private var requestSequence = 0
    private var pendingResponses: [Int: Promise<Data>] = [:]
    private var exitError: CompilerError? = nil
    private static let newlineSeparator: UInt8 = 10

    init(logger: ILogger,
         compilerCompanionBinaryPath: String,
         nodeOptions: [String],
         logsOutputPath: String?,
         compilerCacheURL: URL?,
         isBazelBuild: Bool) {
        self.logger = logger
        self.compilerCompanionBinaryPath = compilerCompanionBinaryPath
        self.nodeOptions = nodeOptions
        self.logsOutputPath = logsOutputPath
        self.compilerCacheURL = compilerCacheURL
        self.isBazelBuild = isBazelBuild

        // Warm up process immediately
        self.prepareProcess()
    }

    private func startProcessIfNeeded() throws {
        guard self.processHandle == nil else {
            return
        }

        if let exitError {
            throw exitError
        }

        logger.info("Will start companion executable")

        var command: [String] = [compilerCompanionBinaryPath]
        let cwd: String? = nil
        let nodeOptions = self.nodeOptions

        if let compilerCacheURL = self.compilerCacheURL {
            command.append("--cache-dir")
            command.append(compilerCacheURL.absoluteURL.path)
        }

        if let logsOutputPath {
            command.append("--log-output")
            command.append(logsOutputPath)
        }

        logger.info("Starting companion executable with command: \(command)")
        var envOverrides: [String: String] = [
            "NODE_OPTIONS": nodeOptions.joined(separator: " "),
            // Set this flag to track Bazel build adoption during the migration
            "IS_BAZEL_BUILD": String(self.isBazelBuild)
        ]
        // Configure the service account
        if let serviceAccount = ProcessInfo.processInfo.environment["VALDI_COMPILER_GCP_SERVICE_ACCOUNT"] {
            envOverrides["VALDI_COMPILER_GCP_SERVICE_ACCOUNT"] = serviceAccount
        }

        let processHandle = AsyncProcessHandle.usingEnv(logger: logger,
                                                        command: command,
                                                        dispatchQueue: queue,
                                                        cwd: cwd,
                                                        envOverrides: envOverrides)

        let logger = self.logger
        processHandle.stderr.onDidReceiveData = { (pipeHandle) in
            let content = pipeHandle.contentAsString
            pipeHandle.content = Data()
            logger.warn("CompilerCompanion: \(content)")
        }
        processHandle.stdout.onDidReceiveData = { [weak self] (pipeHandle) in
            self?.didReceiveData(pipeHandle: pipeHandle)
        }
        try processHandle.runAsync { [weak self] _ in
            guard let strongSelf = self else { return }

            strongSelf.queue.async {
                let processHandle = strongSelf.processHandle
                strongSelf.processExited = true
                strongSelf.processHandle = nil
                let errorMessage = processHandle?.stderr.contentAsString ?? ""
                let error = CompilerError("CompilerCompanion exited: \(errorMessage)")
                strongSelf.exitError = error

                for value in Array(strongSelf.pendingResponses.values) {
                    value.reject(error: error)
                }
                strongSelf.pendingResponses.removeAll()
            }
        }

        self.processHandle = processHandle
    }

    private func didReceiveData(pipeHandle: FileHandleReader) {
        while processData(pipeHandle: pipeHandle) {

        }
    }

    private func process(data: Data) {
        do {
            let decoder = JSONDecoder()
            let responseEnvelope = try decoder.decode(ResponseHeader.self, from: data)
            if let event = responseEnvelope.event {
                if event.type == "info" {
                    logger.info(event.message)
                } else if event.type == "debug" {
                    logger.debug(event.message)
                } else if event.type == "warn" {
                    logger.warn(event.message)
                } else if event.type == "error" {
                    logger.error(event.message)
                } else {
                    throw CompilerError("Could not resolve event type '\(event.type)'")
                }

                return
            }
            guard let id = responseEnvelope.id else {
                throw CompilerError("Received a response envelope with no event and no id")
            }
            if let pendingResponse = pendingResponses.removeValue(forKey: id) {
                if let errorMessage = responseEnvelope.error {
                    pendingResponse.reject(error: CompilerError(errorMessage))
                } else {
                    pendingResponse.resolve(data: data)
                }
            } else {
                logger.error("Could not resolve response for compiler companion request id \(id)")
            }
        } catch let error {
            logger.error("Failed to parse compiler companion response: \(error.legibleLocalizedDescription)")
        }
    }

    private func processData(pipeHandle: FileHandleReader) -> Bool {
        guard let newLineIndex = pipeHandle.content.fastFirstIndex(of: CompanionExecutable.newlineSeparator) else {
            return false
        }

        process(data: pipeHandle.content[..<newLineIndex])
        pipeHandle.trimContent(by: newLineIndex + 1)

        return true
    }

    private func submit<C: Command>(_ commandType: C.Type, _ body: C.Request) -> Promise<C.Response> {
        self.requestSequence += 1
        let sequence = self.requestSequence

        let envelope = RequestEnvelope<C>(id: sequence, body: body)

        do {
            let data = try encodeEnvelope(envelope)
            let innerPromise = Promise<Data>()
            self.pendingResponses[sequence] = innerPromise

            // Swift 5.3 and macOS 10.15 provides FileHandle methods that catch and translate Obj-C exceptions into Swift errors
            if #available(OSX 10.15.4, *) {
                try processHandle?.stdinFileHandle.write(contentsOf: data)
                try processHandle?.stdinFileHandle.write(contentsOf: "\n".data(using: .utf8)!)
            } else {
                processHandle?.stdinFileHandle.write(data)
                processHandle?.stdinFileHandle.write("\n".data(using: .utf8)!)
            }

            logger.trace("Sending companion request >>> \(String(data: data, encoding: .utf8) ?? "<none>")")

            return innerPromise.then { (data) -> Promise<C.Response> in
                self.pendingResponses.removeValue(forKey: sequence)

                self.logger.trace("Received companion response <<< \(String(data: data, encoding: .utf8) ?? "<none>")")

                return DispatchQueue.global(qos: .userInitiated).asyncPromise { () throws -> C.Response in
                    return try decodeData(data, commandType)
                }
            }
        } catch let error {
            return Promise(error: error)
        }
    }

    private func processRequest<C: Command>(_ commandType: C.Type, _ request: C.Request) -> Promise<C.Response> {
        let promise = Promise<C.Response>()
        queue.async {
            do {
                try self.startProcessIfNeeded()

                let logger = self.logger
                self.submit(C.self, request)
                    .catch { (error) -> Void in
                        logger.error("Compiler companion command '\(commandType)' failed: \(error.legibleLocalizedDescription)")
                    }
                    .onComplete { (result) in
                        switch result {
                        case .success(let data):
                            promise.resolve(data: data)
                        case .failure(let error):
                            promise.reject(error: error)
                        }
                    }
            } catch let error {
                promise.reject(error: error)
            }
        }

        return promise
    }

    func prepareProcess() {
        queue.async {
            try? self.startProcessIfNeeded()
        }
    }

    func batchMinifyJS(inputURLs: [URL], minifier: Minifier, options: String?) -> Promise<[BatchMinifyResult]> {
        let request = BatchMinifyJS.Request(inputFiles: inputURLs.map { $0.absoluteString }, minifier: minifier.rawValue, options: options)
        return processRequest(BatchMinifyJS.self, request).then { return $0.results }
    }

    func createWorkspace(nativeApiMinVersion: Int?) -> Promise<Int> {
        let request = CreateWorkspace.Request(nativeApiMinVersion: nativeApiMinVersion)
        return processRequest(CreateWorkspace.self, request).then { response in response.workspaceId }
    }

    func destroyWorkspace(workspaceId: Int) -> Promise<Void> {
        let request = DestroyWorkspace.Request(workspaceId: workspaceId)
        return processRequest(DestroyWorkspace.self, request).then { response in () }
    }

    func initializeWorkspace(workspaceId: Int) -> Promise<Void> {
        let request = InitializeWorkspace.Request(workspaceId: workspaceId)
        return processRequest(InitializeWorkspace.self, request).then { _ in () }
    }

    func registerInMemoryFile(workspaceId: Int, filePath: String, fileContent: String) -> Promise<Void> {
        let request = RegisterFile.Request(workspaceId: workspaceId, fileName: filePath, absoluteDiskPath: nil, fileContent: fileContent)
        return processRequest(RegisterFile.self, request).then { _ in return () }
    }

    func registerDiskFile(workspaceId: Int, filePath: String, absoluteFileURL: URL) -> Promise<Void> {
        let request = RegisterFile.Request(workspaceId: workspaceId, fileName: filePath, absoluteDiskPath: absoluteFileURL.path, fileContent: nil)
        return processRequest(RegisterFile.self, request).then { _ in return () }
    }

    func openFile(workspaceId: Int, filePath: String) -> Promise<TS.OpenResponseBody> {
        let request = OpenFile.Request(workspaceId: workspaceId, fileName: filePath)
        return processRequest(OpenFile.self, request).then { $0.openResult }
    }

    func saveFile(workspaceId: Int, filePath: String) -> Promise<[TS.EmittedFile]> {
        let request = EmitFile.Request(workspaceId: workspaceId, fileName: filePath)
        return processRequest(EmitFile.self, request).then { $0.files }
    }

    func compileNative(workspaceId: Int, stripIncludePrefix: String, inputFiles: [String]) -> Promise<[TS.EmittedFile]> {
        let request = CompileNative.Request(workspaceId: workspaceId, stripIncludePrefix: stripIncludePrefix, inputFiles: inputFiles, outputFile: nil)
        return processRequest(CompileNative.self, request).then { $0.files }
    }

    func uploadArtifact(artifactName: String, artifactData: Data, sha256: String) -> Promise<ArtifactInfo> {
        let request = UploadArtifact.Request(artifactName: artifactName, artifactData: artifactData, sha256: sha256)
        return processRequest(UploadArtifact.self, request).then { ArtifactInfo (url: $0.url, sha256Digest: $0.sha256 )}
    }

    func instrumentJSFiles(files: [InstrumentationFileConfig]) -> Promise<[CodeInstrumentationResult]> {
        let request = AddCodeInstrumentation.Request(files: files)
        return processRequest(AddCodeInstrumentation.self, request).then { $0.results }
    }

    func getErrors(workspaceId: Int, filePath: String) -> Promise<GetErrorsResult> {
        let request = GetSyntacticDiagnostics.Request(workspaceId: workspaceId, fileName: filePath)

        return processRequest(GetSyntacticDiagnostics.self, request).then { response -> GetErrorsResult in
            if response.hasError {
                for diagnostic in response.diagnostics {
                    if diagnostic.category == "error" {
                        if let documentContent = response.fileContent {
                            return GetErrorsResult(error: CompilerError(type: "TypeScript error", message: diagnostic.text,
                                                                       atZeroIndexedLine: diagnostic.start.line - 1,
                                                                        column: diagnostic.start.offset, inDocument: documentContent), timeTakenMs: response.timeTakenMs)
                        } else {
                            return GetErrorsResult(error: CompilerError("TypeScript error: \(diagnostic.text)"), timeTakenMs: response.timeTakenMs)
                        }
                    }
                }
            }

            return GetErrorsResult(error: nil, timeTakenMs: response.timeTakenMs)
        }
    }

    func startDebuggingProxy() -> Promise<StartDebuggingProxy.Response> {
        let request = StartDebuggingProxy.Request()
        return processRequest(StartDebuggingProxy.self, request)
    }

    func updatedAndroidTargets(targets: [AndroidDebuggingTarget]) -> Promise<Void> {
        let request = UpdatedAndroidTargets.Request(targets: targets)
        return processRequest(UpdatedAndroidTargets.self, request).then { _ in }
    }

    func updatedDebuggerPorts(ports: [Int]) -> Promise<Void> {
        let request = UpdatedDebuggerPorts.Request(ports: ports)
        return processRequest(UpdatedDebuggerPorts.self, request).then { _ in }
    }

    func dumpSymbolsWithComments(workspaceId: Int, filePath: String) -> Promise<TS.DumpSymbolsWithCommentsResponseBody> {
        let request = DumpSymbolsWithComments.Request(workspaceId: workspaceId, fileName: filePath)
        return processRequest(DumpSymbolsWithComments.self, request).then { $0.dumpedSymbols }
    }

    func dumpInterface(workspaceId: Int, filePath: String, position: Int) -> Promise<TS.DumpInterfaceResponseBody> {
        let request = DumpInterface.Request(workspaceId: workspaceId, fileName: filePath, position: position)
        return processRequest(DumpInterface.self, request).then { $0.interface }
    }

    func dumpEnum(workspaceId: Int, filePath: String, position: Int) -> Promise<TS.DumpEnumResponseBody> {
        let request = DumpEnum.Request(workspaceId: workspaceId, fileName: filePath, position: position)
        return processRequest(DumpEnum.self, request).then { $0.enum }
    }

    func dumpFunction(workspaceId: Int, filePath: String, position: Int) -> Promise<TS.DumpFunctionResponseBody> {
        let request = DumpFunction.Request(workspaceId: workspaceId, fileName: filePath, position: position)
        return processRequest(DumpFunction.self, request).then { $0.function }
    }

    func generateIdsFiles(moduleName: String, iosHeaderImportPath: String, idFilePath: String) -> Promise<GenerateIdsFilesResult> {
        let request = GenerateIds.Request(moduleName: moduleName, filePath: idFilePath, iosHeaderImportPath: iosHeaderImportPath)
        return processRequest(GenerateIds.self, request).then { GenerateIdsFilesResult(android: $0.android,
                                                                                       iosHeader: $0.iosHeader,
                                                                                       iosImplementation: $0.iosImplementation,
                                                                                       typescriptDefinition: $0.typescriptDefinition,
                                                                                       typescriptImplementation: $0.typescriptImplementation) }
    }

    func exportAndroidStringsTranslations(moduleName: String, baseLocalePath: String, inputLocalePath: String) -> Promise<ExportTranslationStrings.Response> {
        let request = ExportTranslationStrings.Request(moduleName: moduleName, baseLocaleStringsPath: baseLocalePath, inputLocaleStringsPath: inputLocalePath, platform: "android")
        return processRequest(ExportTranslationStrings.self, request)
    }

    func exportIosStringsTranslations(moduleName: String, baseLocalePath: String, inputLocalePath: String) -> Promise<ExportTranslationStrings.Response> {
        let request = ExportTranslationStrings.Request(moduleName: moduleName, baseLocaleStringsPath: baseLocalePath, inputLocaleStringsPath: inputLocalePath, platform: "ios")
        return processRequest(ExportTranslationStrings.self, request)
    }
}

private func encodeEnvelope<C: Command>(_ envelope: RequestEnvelope<C>) throws -> Data {
    do {
        let encoder = JSONEncoder()
        encoder.outputFormatting = .sortedKeys
        let data = try encoder.encode(envelope)
        return data
    } catch EncodingError.invalidValue(let value, let context) {
        try throwEncodeError("Invalid value '\(value)'", context)
    } catch {
        throw error
    }
}

private func throwEncodeError(_ message: String, _ context: EncodingError.Context) throws -> Never {
    throw CompilerError("Failed to encode companion payload: \(message) - \(context.debugDescription)")
}

private func decodeData<C: Command>(_ data: Data, _ commandType: C.Type) throws -> C.Response {
    do {
        let decoder = JSONDecoder()
        let decoded = try decoder.decode(ResponseEnvelope<C>.self, from: data)
        return decoded.body
    } catch DecodingError.dataCorrupted(let context) {
        try throwDecodeError("Data corrupted", context)
    } catch DecodingError.keyNotFound(let key, let context) {
        try throwDecodeError("Key '\(key)' not found:", context)
    } catch DecodingError.valueNotFound(let value, let context) {
        try throwDecodeError("Value '\(value)' not found:", context)
    } catch DecodingError.typeMismatch(let type, let context) {
        try throwDecodeError("Type '\(type)' mismatch:", context)
    } catch {
        throw error
    }
}

private func throwDecodeError(_ message: String, _ context: DecodingError.Context) throws -> Never {
    throw CompilerError("Failed to decode companion payload: \(message) - \(context.debugDescription)")
}
