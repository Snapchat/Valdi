//
//  ValdiCompilerArguments.swift
//
//  Created by Simon Corsin on 4/16/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation
import ArgumentParser

struct ValdiCompilerArguments: ParsableCommand {
    @Option(help: "provide Valdi configuration file")
    var config: String?

    @Flag(help: "enter watch mode with active hot reloader")
    var monitor = false

    @Flag(help: "Generate static resources file from a list of input files")
    var genStaticRes = false

    @Option(help: "list of input files to use for the requested command")
    var input: [String] = []

    @Option(help: "list of devices that should be auto-reloaded")
    var deviceId: [String] = []

    @Option(help: "list of usernames that should be auto-reloaded")
    var username: [String] = []

    @Option(help: "list of IP address that should be auto-reloaded")
    var ip: [String] = []

    @Flag(help: "connect to the device over usb/localhost instead of the local network")
    var usb = false

    @Option(help: "output directory")
    var out: String?

    // TODO(vfomin): replace with enumerable to limit options to the actual log level
    @Option(help: "set the log level")
    var logLevel: String?

    // TODO(vfomin): right now it dumps info about all components, even if the list below is limited
    @Option(help: "dump components information in the JSON format")
    var dumpComponents: [String] = []

    @Option(help: "pack selected files into .valdimodule archive")
    var buildModule: [String] = []

    @Option(help: "unpack selected modules")
    var unpackModule: [String] = []

    @Option(help: "textconv selected modules")
    var textconvModule: [String] = []

    @Option(help: "upload selected modules")
    var uploadModule: [String] = []

    @Option(help: "the base URL to upload downloadable artifacts")
    var uploadBaseUrl: String?

    @Option(help: "module name")
    var module: [String] = []

    @Option(help: "The path to the build directory. Will be inferred from the config directory if not set")
    var buildDir: String?

    @Option(help: "The path where to store the prepared artifact meant to be uploaded")
    var preparedUploadArtifactOutput: String?

    @Option(help: "override a config value")
    var configValue: [String] = []

    @Option(help: "the maximum number of concurrent requests to process module uploads")
    var maxUploadConcurrentRequests: Int = 4

    @Option(help: "Whether to compile for debug, release, or both", transform: {
        if $0 == "debug" {
            return OutputTarget.debug
        } else if $0 == "release" {
            return OutputTarget.release
        } else if $0 == "all" {
            return OutputTarget.all
        } else {
            throw ValidationError("Unsupported output target '\($0)', must be one of 'debug', 'release' or 'all'")
        }
    })
    var outputTarget: OutputTarget?

    @Option
    var outputDumpedSymbolsDir: String?

    @Option
    var onlyCompileTsForModule: [String] = []

    @Option
    var onlyGenerateNativeCodeForModule: [String] = []

    @Option
    var onlyProcessResourcesForModule: [String] = []

    @Option
    var onlyFocusProcessingForModule: [String] = []

    @Flag(help: "Dump modules information in JSON format to use for the Valdi module attribution in C++. See generate_attribution.py for more details")
    var dumpModulesInfo = false

    @Flag(help: "start compilation")
    var compile = false

    @Flag(help: "generate iOS output")
    var ios = false

    @Flag(help: "generate android output")
    var android = false

    @Flag(help: "Experimental: generate web output")
    var web = false

    @Flag(help: "generate C++ output")
    var cpp = false

    @Flag
    var disableDiskCache = false

    @Flag
    var emitDiagnostics = false

    @Flag(help: "run the code generator only")
    var codegenOnly = false

    @Flag(help: "generate module's code coverage")
    var codeCoverage = false

    @Flag
    var forceMinify = false

    @Flag(help: "disable minification + obfuscation for web")
    var disableMinifyWeb = false

    @Flag(help: "disable compiler's concurrency. useful for debugging")
    var disableConcurrency = false

    @Flag
    var ignoreModuleDeps = false

    @Flag(help: "start companion app with --inspect option to be able to debug")
    var debugCompanion = false

    @Flag(help: "disable debugging proxy")
    var noDebuggingProxy = false

    @Flag(help: "disable TS compiler cache")
    var disableTsCompilerCache = false

    @Flag
    var skipRemoveOrphanFiles = false

    @Flag(help: "set 'declaration: true' in the tconfig.json")
    var tsEmitDeclarationFiles = false

    @Flag(help: "set 'removeComments: false' in the tconfig.json")
    var tsKeepComments = false

    @Flag
    var tsSkipVerifyingImports = false

    @Flag
    var disableDownloadableModules = false

    @Flag
    var inlineTranslations = false

    @Flag(help: "Whether assets should be packed inline within the .valdimodule")
    var inlineAssets = false

    @Flag
    var duplicateStderrToStdout = false

    @Option(help: "path to the companion app")
    var directCompanionPath: String?

    @Option(help: "path to the compiler toolbox app")
    var directCompilerToolboxPath: String?

    @Option(help: "path to the pngquant app")
    var directPngquantPath: String?

    @Option(help: "path to the minify config")
    var directMinifyConfigPath: String?

    @Option(help: "path to the clientsql app")
    var directClientSqlPath: String?

    @Option(help: "path to the file with the list of input files")
    var explicitInputListFile: String?

    @Option(help: "Path to where to store the compiler companion logs")
    var companionLogOutput: String?

    @Option(help: "The list of image variants to include for each platform")
    var imageVariantsFilter: String?

    @Option(help: "Sentry DSN for error tracking")
    var sentryDsn: String?

    @Flag(help: "regenerate per module BUILD.bazel build files")
    var regenerateValdiModulesBuildFiles = false

    @Flag(help: "verify artifact files with uploaded signatures")
    var verifyDownloadableArtifacts = false

    @Flag(help: "generate TS files from strings and resources")
    var generateTSResFiles = false

    @Flag(help: "indicates whether the compiler was invoked from Bazel. Used while we're migrated to Bazel based builds to track usage")
    var bazel = false

    @Option(help: "specifies in seconds, how long each compiler task is allowed to wait until completion. Defaults to \(LockConfig.waitTimeoutSeconds) seconds")
    var taskTimeout: Int?

    @Option(help: "specifies in seconds, how long the compiler can run before being automatically killed")
    var timeout: Int?

    var parsedLogLevel: LogLevel? = nil

    var parsedImageVariantsFilter: ImageVariantsFilter? = nil

    mutating func validate() throws {
        let selectedCount = [self.compile, self.monitor, !self.buildModule.isEmpty, !self.unpackModule.isEmpty, !self.textconvModule.isEmpty, !self.uploadModule.isEmpty, self.dumpModulesInfo, self.genStaticRes].filter{$0}.count
        guard selectedCount == 1 else {
            throw ValidationError("Must provide only one of: --compile OR --monitor OR --build-module OR --unpack-module OR --textconv-module OR --upload-module OR --dump-modules-info")
        }
        
        let requiresOutput = [!self.buildModule.isEmpty, !self.unpackModule.isEmpty, !self.textconvModule.isEmpty, !self.uploadModule.isEmpty, self.dumpModulesInfo, self.genStaticRes].filter{$0}.count > 0
        if requiresOutput && self.out == nil {
            throw ValidationError("Must provide --out")
        }
        
        if !self.uploadModule.isEmpty && self.uploadBaseUrl == nil {
            throw ValidationError("Must provide --upload-base-url for --upload-module")
        }
                
        if let logLevel = self.logLevel {
            guard let parsedLevel = LogLevel(description: logLevel.uppercased()) else {
                throw ValidationError("Unrecognized log level. Supported values are: \(LogLevel.allCases.map { $0.description })")
            }
            self.parsedLogLevel = parsedLevel
        }

        if let imageVariantsFilter {
            do {
                self.parsedImageVariantsFilter = try ImageVariantsFilter.parse(str: imageVariantsFilter)
            } catch let error {
                throw ValidationError("Failed to parse --image-variants-filter: \(error.legibleLocalizedDescription)")
            }
        }
    }

    private func doRun() -> Bool {
        let logger = Logger(output: FileHandleLoggerOutput(stdout: FileHandle.standardOutput, stderr: FileHandle.standardError))
        let companionExecutableProvider = CompanionExecutableProvider(logger: logger)
        defer {
            // Ensure logs are flushed even when logs are displayed asynchronously
            logger.flush()
        }

        return ValdiCompilerRunner(logger: logger,
                                      arguments: self,
                                      companionExecutableProvider: companionExecutableProvider,
                                      workingDirectoryPath: FileManager.default.currentDirectoryPath,
                                      runningAsWorker: false).run()
    }

    mutating func run() throws {
        let success = doRun()

#if os(Linux)
        // HACK(simon): Exit process immediately on linux after finishing compiling.
        // This is a workaround of a Swift runtime bug on Linux where the process was
        // segfaulting during cleanup.
        _exit(success ? 0 : 1)
#else
        guard success else {
            throw ExitCode.failure
        }
#endif
    }
}
