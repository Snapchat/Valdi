//
//  ToolboxExecutable.swift
//  
//
//  Created by Simon Corsin on 9/2/22.
//

import Foundation

class ToolboxExecutable {

    struct ImageInfoOutput: Codable {
        var width: Int
        var height: Int
    }

    private let logger: ILogger
    private let compilerToolboxURL: URL
    private var version: String?
    private let lock = DispatchSemaphore.newLock()

    init(logger: ILogger, compilerToolboxURL: URL) {
        self.logger = logger
        self.compilerToolboxURL = compilerToolboxURL
    }

    private func run(arguments: [String]) throws -> String {
        let handle = SyncProcessHandle.usingEnv(logger: logger, command: [compilerToolboxURL.path] + arguments)
        try handle.run()
        if !handle.stderr.content.isEmpty {
            throw CompilerError("Compiler Toolbox error: " + handle.stderr.contentAsString)
        }
        return handle.stdout.contentAsString.trimmed
    }

    private func logIfNeeded(runOutput: String) {
        if !runOutput.isEmpty {
            logger.info("CompilerToolbox: \(runOutput)")
        }
    }

    func precompile(inputFilePath: String, outputFilePath: String, filename: String, engine: String) throws {
        let output = try run(arguments: ["precompile", "-i", inputFilePath, "-o", outputFilePath, "-f", filename, "-e", engine])
        logIfNeeded(runOutput: output)
    }

    func getImageInfo(inputFilePath: String) throws -> ImageInfoOutput {
        let output = try run(arguments: ["image_info", "-i", inputFilePath])

        return try ImageInfoOutput.fromJSON(try output.utf8Data(), keyDecodingStrategy: .convertFromSnakeCase)
    }

    func convertImage(inputFilePath: String, outputFilePath: String, outputWidth: Int?, outputHeight: Int?, qualityRatio: Double?) throws {
        var arguments = ["image_convert", "-i", inputFilePath, "-o", outputFilePath]

        if let outputWidth = outputWidth {
            arguments += ["-w", String(outputWidth)]
        }

        if let outputHeight = outputHeight {
            arguments += ["-h", String(outputHeight)]
        }

        if let qualityRatio = qualityRatio {
            arguments += ["-q", String(qualityRatio)]
        }

        let output = try run(arguments: arguments)
        logIfNeeded(runOutput: output)
    }

    func pngquant(arguments: [String]) throws -> String {
        return try run(arguments: ["pngquant"] + arguments)
    }

    func getVersionString() throws -> String {
        return try lock.lock {
            if let version {
                return version
            }
            logger.debug("Resolving Image Toolbox version")
            let version = try run(arguments: ["version"])
            self.version = version

            return version
        }
    }

}
