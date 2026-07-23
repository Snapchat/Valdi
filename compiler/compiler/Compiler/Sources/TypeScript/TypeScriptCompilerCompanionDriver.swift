//
//  TypeScriptCompilerTSServerDriver.swift
//
//
//  Created by saniul on 31/12/2019.
//

import Foundation

class TypeScriptCompilerCompanionDriver: TypeScriptCompilerDriver {

    private let logger: ILogger
    private let companion: CompanionExecutable
    private let nativeApiMinVersion: Int?
    private var workspaceId: Int?
    private var lock = DispatchSemaphore.newLock()
    private var currentCreateWorkspacePromise: Promise<Int>?

    init(logger: ILogger, companion: CompanionExecutable, nativeApiMinVersion: Int?) {
        self.logger = logger
        self.companion = companion
        self.nativeApiMinVersion = nativeApiMinVersion
    }

    func destroyWorkspace() -> Promise<Void> {
        var workspaceId: Int?
        lock.lock {
            workspaceId = self.workspaceId
            self.workspaceId = nil
            self.currentCreateWorkspacePromise = nil
        }

        if let workspaceId {
            return companion.destroyWorkspace(workspaceId: workspaceId)
        } else {
            return Promise(data: Void())
        }
    }

    private func onWorkspaceCreated(workspaceId: Int) {
        logger.trace("Created new TypeScript Workspace project with id \(workspaceId)")
        lock.lock {
            self.workspaceId = workspaceId
            self.currentCreateWorkspacePromise = nil
        }
    }

    @discardableResult func getOrCreateWorkspace<T>(_ completion: @escaping (Int) -> Promise<T>) -> Promise<T> {
        var workspaceId: Int?
        var currentCreateWorkspacePromise: Promise<Int>?
        var needsOnWorkspaceCreatedCallback = false

        lock.lock {
            if self.workspaceId == nil && self.currentCreateWorkspacePromise == nil {
                logger.trace("Creating TypeScript Workspace project")
                self.currentCreateWorkspacePromise = companion.createWorkspace(nativeApiMinVersion: nativeApiMinVersion)
                needsOnWorkspaceCreatedCallback = true
            }

            workspaceId = self.workspaceId
            currentCreateWorkspacePromise = self.currentCreateWorkspacePromise
        }

        if let workspaceId {
            return completion(workspaceId)
        }

        if let currentCreateWorkspacePromise {
            if needsOnWorkspaceCreatedCallback {
                // We register the then() outside of the lock(), to prevent deadlocks where the createWorkspace() promise
                // resolves almost immediately while we have the lock() acquired
                currentCreateWorkspacePromise.then { [weak self] workspaceId in
                    self?.onWorkspaceCreated(workspaceId: workspaceId)
                    return workspaceId
                }
            }

            return currentCreateWorkspacePromise.then(completion)
        }

        fatalError("Should have either a workspaceId or a pending create workspace promise")
    }

    func initializeWorkspace() -> Promise<Void> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.initializeWorkspace(workspaceId: workspaceId)
        }
    }

    func registerInMemoryFile(filePath: String, fileContent: String) -> Promise<Void> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.registerInMemoryFile(workspaceId: workspaceId, filePath: filePath, fileContent: fileContent)
        }
    }

    func registerDiskFile(filePath: String, absoluteFileURL: URL) -> Promise<Void> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.registerDiskFile(workspaceId: workspaceId, filePath: filePath, absoluteFileURL: absoluteFileURL)
        }
    }

    func openFile(filePath: String) -> Promise<TS.OpenResponseBody> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.openFile(workspaceId: workspaceId, filePath: filePath)
        }
    }

    func saveFile(filePath: String) -> Promise<[TS.EmittedFile]> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.saveFile(workspaceId: workspaceId, filePath: filePath)
        }
    }

    func compileNative(filePaths: [String]) -> Promise<[TS.EmittedFile]> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.compileNative(workspaceId: workspaceId, stripIncludePrefix: "", inputFiles: filePaths)
        }
    }

    func getErrors(filePath: String) -> Promise<GetErrorsResult> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.getErrors(workspaceId: workspaceId, filePath: filePath)
        }
    }

    func dumpSymbolsWithComments(filePath: String) -> Promise<TS.DumpSymbolsWithCommentsResponseBody> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.dumpSymbolsWithComments(workspaceId: workspaceId, filePath: filePath)
        }
    }

    func dumpInterface(filePath: String, position: Int) -> Promise<TS.DumpInterfaceResponseBody> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.dumpInterface(workspaceId: workspaceId, filePath: filePath, position: position)
        }
    }

    func dumpFunction(filePath: String, position: Int) -> Promise<TS.DumpFunctionResponseBody> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.dumpFunction(workspaceId: workspaceId, filePath: filePath, position: position)
        }
    }

    func dumpEnum(filePath: String, position: Int) -> Promise<TS.DumpEnumResponseBody> {
        return getOrCreateWorkspace { workspaceId in
            return self.companion.dumpEnum(workspaceId: workspaceId, filePath: filePath, position: position)
        }
    }
}
