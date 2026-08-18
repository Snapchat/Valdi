//
//  AutoRecompiler.swift
//  Compiler
//
//  Created by Simon Corsin on 4/7/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

final class AutoRecompiler {

    private let logger: ILogger
    private let compiler: ValdiCompiler
    private let filesFinder: ValdiFilesFinder
    private let bundleManager: BundleManager
    private let fileDependenciesManager: FileDependenciesManager
    private let errorDumper: CompilationPipelineErrorDumper
    private let onRecompilationSucceeded: ((Int) -> Void)?

    private let queue = DispatchQueue(label: "com.snap.valdi.reloader")

    init(logger: ILogger, compiler: ValdiCompiler, filesFinder: ValdiFilesFinder, bundleManager: BundleManager, fileDependenciesManager: FileDependenciesManager, errorDumper: CompilationPipelineErrorDumper, onRecompilationSucceeded: ((Int) -> Void)?) {
        self.logger = logger
        self.compiler = compiler
        self.filesFinder = filesFinder
        self.bundleManager = bundleManager
        self.fileDependenciesManager = fileDependenciesManager
        self.errorDumper = errorDumper
        self.onRecompilationSucceeded = onRecompilationSucceeded
    }

    private func filesDidChange(urls: [URL]) {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            for url in urls {
                self.compiler.addFile(file: url)

                let getDependents = self.fileDependenciesManager.getDependents(file: url)
                for dependent in getDependents {
                    self.compiler.addFile(file: dependent)
                }
            }

            do {
                self.logger.info("--- Files changed - starting recompilation pass")
                try self.compiler.compile()
                self.logger.flush()
                self.onRecompilationSucceeded?(urls.count)
                self.logger.info("Recompilation pass finished, waiting for file changes...")
                self.logger.info("--------------------------------------------------------")
            } catch let error {
                self.errorDumper.dump(logger: self.logger, error: error)
            }
        }
    }

    func start() throws {
        filesFinder.delegate = self
        try filesFinder.startMonitoring(logger: logger)
    }

    func stop() {
        if filesFinder.delegate === self {
            filesFinder.delegate = nil
        }
        filesFinder.stopMonitoring()
    }

}

extension AutoRecompiler: ValdiFilesFinderDelegate {

    func valdiFilesFinder(_ valdiFilesFinder: ValdiFilesFinder, filesDidChange files: [URL]) {
        queue.async {
            self.filesDidChange(urls: files)
        }
    }

}
