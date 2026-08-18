//
//  TypeScriptCompiler.swift
//  Compiler
//
//  Created by Simon Corsin on 7/19/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

private struct TypeScriptProject {

    let path: String
    let name: String

}

private enum TypeScriptFileStatus {
    case upToDate
    case needOpen
    case needReload
}

struct ImportPath {
    let relative: String
    let absolute: String
}

enum EnsureOpenResult {
    case upToDate
    case opened(importPaths: [ImportPath])
}

struct CompiledTypeScriptFile {
    let jsFile: JavaScriptFile
    let declarationFile: FinalFile?
}

private struct TypeScriptFileDiagnostic {
    let error: CompilerError?
}

final class TypeScriptCompiler {
    static let configsDirectoryName = "_configs"
    static let baseConfigFileName = "base.tsconfig.json"
    static let workspaceRoot = URL(fileURLWithPath: "/")

    private let logger: ILogger
    private let companion: CompanionExecutable
    private let driver: TypeScriptCompilerCompanionDriver
    private let queue = DispatchQueue(label: "com.snap.valdi.TypeScriptCompiler")

    private let emitDebug: Bool
    private let projectConfig: ValdiProjectConfig
    private let openedFileCache = CompilationCache<Bool>()
    private let getErrorsCache = CompilationCache<Bool>()
    private let fileManager: ValdiFileManager

    init(logger: ILogger, 
         companion: CompanionExecutable,
         projectConfig: ValdiProjectConfig,
         fileManager: ValdiFileManager,
         emitDebug: Bool) {
        self.logger = logger
        self.companion = companion
        self.driver = TypeScriptCompilerCompanionDriver(
            logger: logger,
            companion: companion,
            nativeApiMinVersion: projectConfig.nativeApiMinVersion
        )
        self.projectConfig = projectConfig
        self.fileManager = fileManager
        self.emitDebug = emitDebug
    }

    func initializeWorkspace() -> Promise<Void> {
        return driver.initializeWorkspace().dispatch(on: queue)
    }

    func destroyWorkspace() -> Promise<Void> {
        return driver.destroyWorkspace()
    }

    private func getFileStatus(filePath: String, compileSequence: CompilationSequence) -> TypeScriptFileStatus {
        var status = TypeScriptFileStatus.upToDate
        _ = openedFileCache.getOrUpdate(key: filePath, compileSequence: compileSequence) { (_, previousValue) -> Bool in
            status = previousValue == nil ? .needOpen : .needReload
            return true
        }

        return status
    }

    private func openFile(filePath: String) -> Promise<TS.OpenResponseBody> {
        return driver.openFile(filePath: filePath)
    }

    private func getErrors(filePath: String, compileSequence: CompilationSequence) -> Promise<GetErrorsResult> {
        var shouldUpdate = false
        _ = getErrorsCache.getOrUpdate(key: filePath, compileSequence: compileSequence) { _, diagnostics in
            shouldUpdate = true
            return true
        }

        if shouldUpdate {
            return driver.getErrors(filePath: filePath).then { result -> Promise<GetErrorsResult> in
                if let error = result.error {
                    return Promise(error: error)
                } else {
                    return Promise(data: result)
                }
            }
        } else {
            return Promise(data: GetErrorsResult(error: nil, timeTakenMs: 0))
        }
    }

    private func saveFile(filePath: String, outputDirURL: URL) -> Promise<CompiledTypeScriptFile> {
        let logger = self.logger
        return driver.saveFile(filePath: filePath)
            .then { (files) throws -> CompiledTypeScriptFile in

                var emittedJsFile: TS.EmittedFile? = nil
                var emittedDeclarationFile: TS.EmittedFile? = nil

                for file in files {
                    if file.fileName.hasSuffix(".js") {
                        emittedJsFile = file
                    } else if file.fileName.hasSuffix(".d.ts") {
                        emittedDeclarationFile = file
                    }
                }

                guard let entry = emittedJsFile else {
                    throw CompilerError("Did not receive JS file output")
                }

                let relativePath = outputDirURL.relativePath(to: URL(fileURLWithPath: entry.fileName))
                logger.verbose("Compiled and save file \(relativePath)")
                let jsFile = JavaScriptFile(file: .string(entry.content), relativePath: relativePath)
                let declarationFile = emittedDeclarationFile.map { emittedFile -> FinalFile in
                    return FinalFile(outputURL: URL(fileURLWithPath: emittedFile.fileName), file: .string(emittedFile.content), platform: nil, kind: .compiledSource)
                } 

                return CompiledTypeScriptFile(jsFile: jsFile, declarationFile: declarationFile)

            }
            .catch { (error) -> Error in
                return CompilerError("Failed to save file: \(error.legibleLocalizedDescription)")
            }
    }

    private func stringFromTextSpan(_ textSpan: TS.TextSpan?, in lines: [Substring]) throws -> String {
        guard let startSpan = textSpan?.start, let endSpan = textSpan?.end else {
            throw CompilerError("Unable to deserialize TextSpan")
        }

        guard startSpan.line <= endSpan.line, startSpan.line - 1 < lines.count, endSpan.line - 1 < lines.count else {
            throw CompilerError("TextSpan is out of bounds")
        }

        var out = ""

        for line in startSpan.line...endSpan.line {

            let adjustedLine = line - 1

            let currentLine = lines[adjustedLine]

            if line == startSpan.line {
                out += currentLine[currentLine.index(currentLine.startIndex, offsetBy: startSpan.offset - 1)...]
            } else if line == endSpan.line {
                out += currentLine[..<currentLine.index(currentLine.startIndex, offsetBy: endSpan.offset - 1)]
            } else {
                out += currentLine
            }
        }

        return out
    }

    func dumpSymbolsWithComments(filePath: String) -> Promise<TS.DumpSymbolsWithCommentsResponseBody> {
        return driver.dumpSymbolsWithComments(filePath: filePath)
    }

    func dumpInterface(filePath: String, position: Int) -> Promise<TS.DumpInterfaceResponseBody> {
        return driver.dumpInterface(filePath: filePath, position: position)
    }

    func dumpEnum(filePath: String, position: Int) -> Promise<TS.DumpEnumResponseBody> {
        return driver.dumpEnum(filePath: filePath, position: position)
    }

    func dumpFunction(filePath: String, position: Int) -> Promise<TS.DumpFunctionResponseBody> {
        return driver.dumpFunction(filePath: filePath, position: position)
    }

    func register(file: File, filePath: String) -> Promise<Void> {
        logger.verbose("Registering file \(filePath)")

        switch file {
        case .data(let data):
            let str = String(data: data, encoding: .utf8)!
            return driver.registerInMemoryFile(filePath: filePath, fileContent: str)
        case .string(let str):
            return driver.registerInMemoryFile(filePath: filePath, fileContent: str)
        case .lazyData(let block):
            do {
                let data = try block()
                let str = String(data: data, encoding: .utf8)!
                return driver.registerInMemoryFile(filePath: filePath, fileContent: str)
            } catch let error {
                return Promise(error: error)
            }
        case .url(let url):
            return driver.registerDiskFile(filePath: filePath, absoluteFileURL: url)
        }
    }

    func ensureOpenFile(filePath: String, outputDirURL: URL, compileSequence: CompilationSequence) -> Promise<EnsureOpenResult> {
        let fileStatus = getFileStatus(filePath: filePath, compileSequence: compileSequence)
        switch fileStatus {
        case .needOpen:
            logger.verbose("-- Opening TypeScript File \(filePath)")
            return self.openFile(filePath: filePath).then {
                return .opened(importPaths: $0.importPaths.map { ImportPath(relative: $0.relative, absolute: $0.absolute) })
            }
        case .needReload:
            logger.verbose("-- Reloading TypeScript File \(filePath)")
            return self.openFile(filePath: filePath).then {
                return .opened(importPaths: $0.importPaths.map { ImportPath(relative: $0.relative, absolute: $0.absolute) })
            }
        case .upToDate:
            return Promise(data: .upToDate)
        }
    }

    func checkErrors(filePath: String, compileSequence: CompilationSequence) -> Promise<GetErrorsResult> {
        return self.getErrors(filePath: filePath, compileSequence: compileSequence)
    }

    func compile(filePath: String, outputDirURL: URL, compileSequence: CompilationSequence) -> Promise<CompiledTypeScriptFile> {
        return self.getErrors(filePath: filePath, compileSequence: compileSequence)
            .then { (_) -> Promise<CompiledTypeScriptFile> in
                return self.saveFile(filePath: filePath, outputDirURL: outputDirURL)
            }
    }

    func compileNative(filePaths: [String], compileSequence: CompilationSequence) -> Promise<[NativeSource]> {
        return self.driver.compileNative(filePaths: filePaths).then { emittedFile in
            return emittedFile.map {
                return NativeSource(relativePath: nil, filename: $0.fileName, file: .string($0.content), groupingIdentifier: $0.fileName, groupingPriority: 0)
            }
        }
    }
}
