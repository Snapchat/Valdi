import Foundation

// Structure for collecting configuration values for the current
// invocation of the Compiler.
struct CompilerConfig {
    let hotReloadingEnabled: Bool
    let disableDiskCache: Bool

    // Skip starting the debugging proxy when running in the hot reloader mode.
    let disableDebuggingProxy: Bool

    let skipCalculatingCompanionBinarySignature: Bool

    let tsSkipVerifyingImports: Bool
    let tsEmitDeclarationFiles: Bool

    // Currently, emitted declaration (.d.ts) files from dependencies must have their comments kept intact
    // because generating native Obj-C/Kotlin types requires parsing the @GenerateNativeXXX annotations on types
    // imported from dependencies.
    let tsKeepComments: Bool

    // HACK: Until we figure out how to get a fresh LCA token when executing a Bazel rule, we can't
    // use the artifact service to upload downloadable assets to Bolt.
    let disableDownloadableModules: Bool

    /// When specified, the compiler will serialize the result of the dumpAllSymbols command in the provided
    /// directory.
    let outputDumpedSymbolsDirectory: URL?

    /// If set, only TypeScript files from these modules will be included when issuing TypeScript
    /// open/check/compileAndSave commands to the companion.
    ///
    /// This allows us to minimize the amount of TypeScript operations that have to be performed when
    /// compiling modules in isolation.
    ///
    /// (Note: successful compilation requires dumped symbols from dependencies
    /// to be present. See `inputDumpedSymbolsDirectory`).
    let onlyCompileTypeScriptForModules: Set<String>

    /// When set, native types, functions, view classes will only be generated for the specified modules.
    let onlyGenerateNativeCodeForModules: Set<String>

    /// When set, image resources will only be processed for the specified modules
    let onlyProcessResourcesForModules: Set<String>

    /// When set, dependency data will only be processed for the specified modules
    let onlyFocusProcessingForModules: Set<String>

    /// Direct path to the companion app
    let directCompanionPath: String?

    /// List of input files to pass to the compiler instead of automatically discovering them
    let explicitInputList: CompilerFileInputList?

    /// Explicit image assets supplied by the build system.
    let explicitImageAssetManifest: ExplicitImageAssetManifest?

    /// When set, this will _rewrite_ the BUILD.bazel files within the sources of each Valdi module
    let regenerateValdiModulesBuildFiles: Bool

    /// When set, only the steps that generate TS code will run, which includes Strings.d.ts and res.d.ts files
    let generateTSResFiles: Bool

    /// When set, this wil validate artifacts against .downloadableArtifacts instead of uploading
    let verifyDownloadableArtifacts: Bool

    /// Whether we should process the outputs for iOS
    let outputForIOS: Bool

    /// Whether we should process the outputs for Android
    let outputForAndroid: Bool

    /// Whether we should process the outputs for Web
    let outputForWeb: Bool

    /// Whether we should process the outputs for C++
    let outputForCpp: Bool

    /// Whether to output for release, debug or both
    let outputTarget: OutputTarget

    static func from(args: ValdiCompilerArguments, baseURL: URL, environment: [String: String]) throws -> CompilerConfig {
        let hotReloadingEnabled = args.monitor

        let disableDiskCache = args.disableDiskCache
        let skipCalculatingCompanionBinarySignature = args.disableDiskCache

        let tsSkipVerifyingImports = hotReloadingEnabled || args.tsSkipVerifyingImports
        let tsEmitDeclarationFiles = args.tsEmitDeclarationFiles
        let tsKeepComments = args.tsKeepComments

        let disableDownloadableModules = args.disableDownloadableModules

        let outputDumpedSymbolsDirectory = args.outputDumpedSymbolsDir.map { baseURL.appendingPathComponent($0, isDirectory: true) }

        let explicitInputList = try args.explicitInputListFile
            .map {
                let url = baseURL.appendingPathComponent($0)
                let data = try File.url(url).readData()
                let parsed = try CompilerFileInputList.fromJSON(data, keyDecodingStrategy: .convertFromSnakeCase)

                return try parsed.resolvingVariables(environment)
            }

        let explicitImageAssetManifest = try args.explicitImageAssetManifest
            .map {
                let url = baseURL.appendingPathComponent($0)
                var isDirectory: ObjCBool = false
                guard FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory), !isDirectory.boolValue else {
                    throw CompilerError("Explicit image asset manifest file does not exist at \(url.path)")
                }

                do {
                    let data = try File.url(url).readData()
                    let parsed = try ExplicitImageAssetManifest.fromJSON(data, keyDecodingStrategy: .convertFromSnakeCase)
                    return try parsed.resolvingVariables(environment)
                } catch let error {
                    throw CompilerError("Failed to parse explicit image asset manifest at \(url.path): \(error.legibleLocalizedDescription)")
                }
            }

        return CompilerConfig(
            hotReloadingEnabled: hotReloadingEnabled,
            disableDiskCache: disableDiskCache,
            disableDebuggingProxy: args.noDebuggingProxy,
            skipCalculatingCompanionBinarySignature: skipCalculatingCompanionBinarySignature,
            tsSkipVerifyingImports: tsSkipVerifyingImports,
            tsEmitDeclarationFiles: tsEmitDeclarationFiles,
            tsKeepComments: tsKeepComments,
            disableDownloadableModules: disableDownloadableModules,
            outputDumpedSymbolsDirectory: outputDumpedSymbolsDirectory,
            onlyCompileTypeScriptForModules: Set(args.onlyCompileTsForModule),
            onlyGenerateNativeCodeForModules: Set(args.onlyGenerateNativeCodeForModule),
            onlyProcessResourcesForModules: Set(args.onlyProcessResourcesForModule),
            onlyFocusProcessingForModules: Set(args.onlyFocusProcessingForModule),
            directCompanionPath: args.directCompanionPath,
            explicitInputList: explicitInputList,
            explicitImageAssetManifest: explicitImageAssetManifest,
            regenerateValdiModulesBuildFiles: args.regenerateValdiModulesBuildFiles,
            generateTSResFiles: args.generateTSResFiles,
            verifyDownloadableArtifacts: args.verifyDownloadableArtifacts,
            outputForIOS: args.ios,
            outputForAndroid: args.android,
            outputForWeb: args.web,
            outputForCpp: args.cpp,
            outputTarget: args.outputTarget ?? OutputTarget.all
        )
    }
}
