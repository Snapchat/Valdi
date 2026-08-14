//
//  ValdiProjectConfig.swift
//  Compiler
//
//  Created by Simon Corsin on 5/15/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation
import Yams

struct ValdiOutputConfig {
    let ignoredFiles: [NSRegularExpression]
    let debugPath: UnresolvedPath?
    let releasePath: UnresolvedPath?
    let metadataPath: UnresolvedPath?
    let globalMetadataPath: UnresolvedPath?
    let codegenEnabled: Bool

    static let noop = ValdiOutputConfig(ignoredFiles: [], debugPath: nil, releasePath: nil, metadataPath: nil, globalMetadataPath: nil, codegenEnabled: false)
}

struct ValdiCodeCoverageConfig: Codable {
    let reportFolder: URL
    let reportResultFile: URL
    let instrumentedFilesResult: URL
}

struct GeneratedBuildFileConfig {
    let buildFileName: String
    let buildFileMacroPath: String
    let buildFileMacroName: String
}

struct ValdiProjectConfig {
    let configDirectoryUrl: URL
    let projectName: String
    let baseDir: URL

    var webEnabled: Bool

    var iosOutput: ValdiOutputConfig?
    var androidOutput: ValdiOutputConfig?
    var webOutput: ValdiOutputConfig?
    var cppOutput: ValdiOutputConfig?

    var iosBuildFileConfig: GeneratedBuildFileConfig?
    var androidBuildFileConfig: GeneratedBuildFileConfig?
    var webBuildFileConfig: GeneratedBuildFileConfig?

    var codeCoverageResultConfig: ValdiCodeCoverageConfig

    let iosDefaultModuleNamePrefix: String?
    let iosDefaultLanguage: IOSLanguage

    let ignoredFiles: [NSRegularExpression]
    let directoriesToKeepRegexes: [NSRegularExpression]

    let moduleUploadBaseURL: URL?
    let diagnosticsDir: URL
    let compilerCompanionBinaryURL: URL?
    var shouldDebugCompilerCompanion: Bool
    var shouldEmitDiagnostics: Bool
    var shouldSkipRemoveOrphanFiles: Bool

    let compilerToolboxURL: URL

    let minifyConfigURL: URL?

    let clientSqlURL: URL?
    
    let sqlExportPathTemplate: String?

    let androidDefaultClassPath: String?

    let cppDefaultClassPrefix: String?

    let cppImportPathPrefix: String?

    /**
     If set for the project, artifacts will not be uploaded, they
     will be stored as this location instead.
     */
    var preparedUploadArtifactOutput: URL?

    let buildDirectoryURL: URL
    let generatedTsDirectoryURL: URL

    let androidJsBytecodeFormat: String?

    let nodeModulesDir: URL?

    let openSourceDir: URL?

    let nodeModulesTarget: String?
    let nodeModulesWorkspace: String?
    let externalModulesTarget: String?
    let externalModulesWorkspace: String?
    let nativeApiMinVersion: Int?

    private static func parseOutputConfig(inputConfig: Yams.Node.Mapping?,
                                          ignoredFiles: [NSRegularExpression]?,
                                          baseDirUrl: URL,
                                          environment: [String: String]
    ) throws -> ValdiOutputConfig? {
        guard let configDict = inputConfig?["output"] else { return nil }
        guard let basePath = configDict["base"]?.string else {
            return nil
        }

        func parseAndResolve(_ key: String) -> UnresolvedPath? {
            return configDict[key]?.string.flatMap { UnresolvedPath(baseURL: baseDirUrl, components: [basePath, $0], variables: environment) }
        }

        let debugPath = parseAndResolve("debug_path")
        let releasePath = parseAndResolve("release_path")
        let metadataPath = parseAndResolve("metadata_path")
        let globalMetadataPath = parseAndResolve("global_metadata_path")
        let codegenEnabled = inputConfig?["codegen_enabled"]?.bool ?? true

        return ValdiOutputConfig(ignoredFiles: ignoredFiles ?? [],
                                    debugPath: debugPath,
                                    releasePath: releasePath,
                                    metadataPath: metadataPath,
                                    globalMetadataPath: globalMetadataPath,
                                    codegenEnabled: codegenEnabled)
    }

    private static func parseCodeCoverageConfig(codeCoverageConfig: Yams.Node.Mapping?,
                                                configDirectoryUrl: URL) -> ValdiCodeCoverageConfig {
        func parseAndResolve(_ key: String, _ defaultValue: String) -> URL {
            return configDirectoryUrl.resolving(path: (codeCoverageConfig?[key]?.string ?? defaultValue), isDirectory: true)
        }

        let reportFolder = parseAndResolve("report_folder", "./coverage")
        let reportResultFile = parseAndResolve("code_coverage_report_result", "./coverage/result.json")
        let instrumentedFilesResult = parseAndResolve("instrumented_files_result", "./coverage/files.json")

        return ValdiCodeCoverageConfig(reportFolder: reportFolder, reportResultFile: reportResultFile, instrumentedFilesResult: instrumentedFilesResult)
    }

    private static func getOSSpecificValue(config: Yams.Node, key: String) -> Yams.Node? {
        if let dict = config[key]?.mapping {
#if os(Linux)
            return dict["linux"]
#else
            return dict["macos"]
#endif
        } else {
            return config[key]
        }
    }

    static func from(logger: ILogger, configUrl: URL, currentDirectoryUrl: URL, environment: [String: String], args: ValdiCompilerArguments) throws -> ValdiProjectConfig {
        let configValueOverrides = args.configValue

        let configContent = try String(contentsOf: configUrl)
        guard var config = try Yams.compose(yaml: configContent) else {
            throw CompilerError("Failed to parse project config at: \(configUrl)")
        }

        for configValue in configValueOverrides {
            do {
                let parsedValue = try ConfigValue.parse(text: configValue)
                config = parsedValue.inject(into: config)
            } catch let error {
                throw CompilerError("Failed to parse config value '\(configValue)': \(error.legibleLocalizedDescription)")
            }
        }

        guard let baseDirPath = try config["base_dir"]?.string?.resolvingVariables(environment) else {
            throw CompilerError("Missing base_dir in config file")
        }

        let configDirectoryUrl = configUrl.deletingLastPathComponent()

        let baseDirUrl = configDirectoryUrl.resolving(path: baseDirPath, isDirectory: true)

        let projectName = try config["project_name"]?.string?.resolvingVariables(environment) ?? baseDirUrl.lastPathComponent

        let iosConfig = config["ios"]?.mapping

        let ignoredPatternsIOS = try iosConfig?["exclude_patterns"]?.array()
            .compactMap { try $0.string?.resolvingVariables(environment) }
            .map { try NSRegularExpression(pattern: $0, options: []) }

        let iosOutputConfig = try parseOutputConfig(inputConfig: iosConfig, ignoredFiles: ignoredPatternsIOS, baseDirUrl: baseDirUrl, environment: environment)
        let iosDefaultModuleNamePrefix = iosConfig?["default_module_name_prefix"]?.string
        let iosDefaultLanguage = try IOSLanguage.parse(input: iosConfig?["default_language"]?.string ?? "objc")

        let iosBuildFileEnabled = iosConfig?["build_file_enabled"]?.bool ?? false
        let iosBuildFileName = iosConfig?["build_file_name"]?.string ?? "BUILD.bazel"
        let iosBuildFileMacroPath = iosConfig?["build_file_macro_path"]?.string ?? "@macros//:DEFS.bzl"
        let iosBuildFileMacroName = iosConfig?["build_file_macro_name"]?.string ?? "modularised_valdi_module"
        let iosBuildFileConfig = iosBuildFileEnabled ? GeneratedBuildFileConfig(buildFileName: iosBuildFileName,
                                                                                buildFileMacroPath: iosBuildFileMacroPath,
                                                                                buildFileMacroName: iosBuildFileMacroName) : nil

        let androidConfig = config["android"]?.mapping

        let ignoredPatternsAndroid = try androidConfig?["exclude_patterns"]?.array()
            .compactMap { try $0.string?.resolvingVariables(environment) }
            .map { try NSRegularExpression(pattern: $0, options: []) }

        let androidOutputConfig = try parseOutputConfig(inputConfig: androidConfig, ignoredFiles: ignoredPatternsAndroid, baseDirUrl: baseDirUrl, environment: environment)
        let androidDefaultClassPath = androidConfig?["class_path"]?.string
        let androidJsBytecodeFormat = androidConfig?["js_bytecode_format"]?.string

        let androidBuildFileEnabled = androidConfig?["build_file_enabled"]?.bool ?? config["build_file_generation_enabled"]?.bool ?? false
        let androidBuildFileName = androidConfig?["build_file_name"]?.string ?? "BUILD.bazel"
        let androidBuildFileMacroPath = androidConfig?["build_file_macro_path"]?.string ?? "//bzl/valdi:valdi_module.bzl"
        let androidBuildFileMacroName = androidConfig?["build_file_macro_name"]?.string ?? "valdi_module"
        let androidBuildFileConfig = androidBuildFileEnabled ? GeneratedBuildFileConfig(buildFileName: androidBuildFileName,
                                                                                        buildFileMacroPath: androidBuildFileMacroPath,
                                                                                        buildFileMacroName: androidBuildFileMacroName) : nil

        let webConfig = config["web"]?.mapping

        let ignoredPatternsWeb = try webConfig?["exclude_patterns"]?.array()
            .compactMap { try $0.string?.resolvingVariables(environment) }
            .map { try NSRegularExpression(pattern: $0, options: []) }

        let webOutputConfig = try parseOutputConfig(inputConfig: webConfig, ignoredFiles: ignoredPatternsWeb, baseDirUrl: baseDirUrl, environment: environment)
        let webBuildFileEnabled = webConfig?["build_file_enabled"]?.bool ?? config["build_file_generation_enabled"]?.bool ?? true
        let webBuildFileName = webConfig?["build_file_name"]?.string ?? "package.json"
        let webBuildFileConfig = webBuildFileEnabled ? GeneratedBuildFileConfig(buildFileName: webBuildFileName,
                                                                                    buildFileMacroPath: "",
                                                                                    buildFileMacroName: "") : nil


        let codeCoverageConfig = config["code_coverage"]?.mapping

        let codeCoverageResultConfig = parseCodeCoverageConfig(codeCoverageConfig: codeCoverageConfig, configDirectoryUrl: configDirectoryUrl)

        let cppConfig = config["cpp"]?.mapping

        let ignoredPatternsCpp = try cppConfig?["exclude_patterns"]?.array()
            .compactMap { try $0.string?.resolvingVariables(environment) }
            .map { try NSRegularExpression(pattern: $0, options: []) }

        let cppOutputConfig = try parseOutputConfig(inputConfig: cppConfig, ignoredFiles: ignoredPatternsCpp, baseDirUrl: baseDirUrl, environment: environment)

        let cppDefaultClassPrefix = cppConfig?["default_class_prefix"]?.string
        let cppImportPathPrefix = cppConfig?["import_path_prefix"]?.string

        let ignoredFiles = try config["ignored_files"]?.array().compactMap { try $0.string?.resolvingVariables(environment) } ?? []

        let directoriesToKeepRegexes = try config["directories_to_keep_regexes"]?.array().compactMap { try $0.string?.resolvingVariables(environment) } ?? []

        let moduleUploadBaseURL = config["module_upload_base_url"]?.string.flatMap { URL(string: $0) }

        let diagnosticsDir = configDirectoryUrl.resolving(path: (config["diagnostics_dir"]?.string ?? "./diagnostics"), isDirectory: true)

        if let path = args.directCompanionPath {
            logger.info("Using direct companion toolbox: \(path)")
        }
        let compilerCompanionBinaryURL = try config["compiler_companion_binary"]?.string
            .map { try $0.resolvingVariables(environment) }
            .flatMap { configDirectoryUrl.resolving(path: $0, isDirectory: false)}

        var compilerToolboxURL: URL?
        if let path = args.directCompilerToolboxPath {
            logger.info("Using direct compiler toolbox: \(path)")
            compilerToolboxURL = currentDirectoryUrl.resolving(path: path, isDirectory: false)
        } else {
            compilerToolboxURL = try getOSSpecificValue(config: config, key: "compiler_toolbox_path")?.string
                .map { try $0.resolvingVariables(environment) }
                .flatMap { configDirectoryUrl.resolving(path: $0, isDirectory: false) }
        }

        guard let compilerToolboxURL = compilerToolboxURL else {
            throw CompilerError("compiler_toolbox_path not configured in the Valdi project config file")
        }

        var minifyConfigURL:URL?
        if let path = args.directMinifyConfigPath {
            logger.info("Using direct minify config: \(path)")
            minifyConfigURL = currentDirectoryUrl.resolving(path: path)
        } else {
            minifyConfigURL = try config["minify_config_path"]?.string
                .map { try $0.resolvingVariables(environment) }
                .flatMap { configDirectoryUrl.resolving(path: $0, isDirectory: false) }
        }

        var clientSqlURL: URL?
        if let path = args.directClientSqlPath {
            logger.info("Using direct clientsql: \(path)")
            clientSqlURL = currentDirectoryUrl.resolving(path: path)
        } else {
            clientSqlURL = try config["clientsql_binary_path"]?.string
                .map { try $0.resolvingVariables(environment) }
                .flatMap { configDirectoryUrl.resolving(path: $0, isDirectory: false) }
        }
        
        let sqlExportPathTemplate = config["sql_export_path_template"]?.string
        
        let hotReloadingEnabled = args.monitor

        let buildDirectoryURL: URL
        if let buildDir = args.buildDir {
            buildDirectoryURL = currentDirectoryUrl.resolving(path: try buildDir.resolvingVariables(environment))
        } else {
            let buildRootURL = configDirectoryUrl.appendingPathComponent(".valdi_build", isDirectory: true)
            buildDirectoryURL = buildRootURL.appendingPathComponent(hotReloadingEnabled ? "hotreload" : "compile", isDirectory: true)
        }

        let generatedTsDirectoryURL = buildDirectoryURL.appendingPathComponent("generated_ts", isDirectory: true)

        let nodeModulesDirectoryURL: URL? = try config["node_modules_dir"]?.string
            .map { try $0.resolvingVariables(environment) }
            .flatMap { configDirectoryUrl.resolving(path: $0, isDirectory: true)}

        let openSourceDirectoryURL: URL? = try config["open_source_dir"]?.string
            .map { try $0.resolvingVariables(environment) }
            .flatMap { configDirectoryUrl.resolving(path: $0, isDirectory: true)}

        let nodeModulesTarget = config["node_modules_target"]?.string
        let nodeModulesWorkspace = config["node_modules_workspace"]?.string
        let externalModulesTarget = config["external_modules_target"]?.string
        let externalModulesWorkspace = config["external_modules_workspace"]?.string
        let nativeApiMinVersion: Int?
        if let configuredNativeApiMinVersion = config["native_api_min_version"] {
            guard let parsedNativeApiMinVersion = configuredNativeApiMinVersion.int else {
                throw CompilerError("native_api_min_version must be an integer")
            }
            guard parsedNativeApiMinVersion >= 0 && parsedNativeApiMinVersion <= Int(Int32.max) else {
                throw CompilerError("native_api_min_version must be between 0 and \(Int32.max)")
            }
            nativeApiMinVersion = parsedNativeApiMinVersion
        } else {
            nativeApiMinVersion = nil
        }

        var projectConfig = ValdiProjectConfig(configDirectoryUrl: configDirectoryUrl,
                                                  projectName: projectName,
                                                  baseDir: baseDirUrl,
                                                  webEnabled: args.web,
                                                  iosOutput: iosOutputConfig,
                                                  androidOutput: androidOutputConfig,
                                                  webOutput:
                                                    webOutputConfig,
                                                  cppOutput: cppOutputConfig,
                                                  iosBuildFileConfig: iosBuildFileConfig,
                                                  androidBuildFileConfig: androidBuildFileConfig,
                                                  webBuildFileConfig: webBuildFileConfig,
                                                  codeCoverageResultConfig: codeCoverageResultConfig,
                                                  iosDefaultModuleNamePrefix: iosDefaultModuleNamePrefix,
                                                  iosDefaultLanguage: iosDefaultLanguage,
                                                  ignoredFiles: try ignoredFiles.map { try NSRegularExpression(pattern: $0, options: [])},
                                                  directoriesToKeepRegexes: try directoriesToKeepRegexes.map { try NSRegularExpression(pattern: $0, options: []) },
                                                  moduleUploadBaseURL: moduleUploadBaseURL,
                                                  diagnosticsDir: diagnosticsDir,
                                                  compilerCompanionBinaryURL: compilerCompanionBinaryURL,
                                                  shouldDebugCompilerCompanion: false,
                                                  shouldEmitDiagnostics: false,
                                                  shouldSkipRemoveOrphanFiles: false,
                                                  compilerToolboxURL: compilerToolboxURL,
                                                  minifyConfigURL: minifyConfigURL,
                                                  clientSqlURL: clientSqlURL,
                                                  sqlExportPathTemplate: sqlExportPathTemplate,
                                                  androidDefaultClassPath: androidDefaultClassPath,
                                                  cppDefaultClassPrefix: cppDefaultClassPrefix,
                                                  cppImportPathPrefix: cppImportPathPrefix,
                                                  preparedUploadArtifactOutput: nil,
                                                  buildDirectoryURL: buildDirectoryURL,
                                                  generatedTsDirectoryURL: generatedTsDirectoryURL,
                                                  androidJsBytecodeFormat: androidJsBytecodeFormat,
                                                  nodeModulesDir: nodeModulesDirectoryURL,
                                                  openSourceDir: openSourceDirectoryURL,
                                                  nodeModulesTarget: nodeModulesTarget,
                                                  nodeModulesWorkspace: nodeModulesWorkspace,
                                                  externalModulesTarget: externalModulesTarget,
                                                  externalModulesWorkspace: externalModulesWorkspace,
                                                  nativeApiMinVersion: nativeApiMinVersion)

        projectConfig.shouldEmitDiagnostics = args.emitDiagnostics
        projectConfig.shouldDebugCompilerCompanion = args.debugCompanion
        projectConfig.shouldSkipRemoveOrphanFiles = args.skipRemoveOrphanFiles
        if let preparedUploadArtifactOutput = args.preparedUploadArtifactOutput {
            projectConfig.preparedUploadArtifactOutput = currentDirectoryUrl.resolving(path: preparedUploadArtifactOutput)
        }

        return projectConfig
    }

    func resolve(url: URL, outURL: URL) -> URL {
        let relativePath = baseDir.relativePath(to: url)
        return outURL.appendingPathComponent(relativePath)
    }
}
