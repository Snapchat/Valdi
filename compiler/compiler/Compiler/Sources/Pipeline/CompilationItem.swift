//
//  CompilationItem.swift
//  Compiler
//
//  Created by Simon Corsin on 7/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation
import Yams

enum Platform: String, CaseIterable, Codable {
    case ios
    case android
    case web
    case cpp
}

enum ModuleOutputTarget: Int {
    case debugOnly
    case releaseReady
}

struct OutputTarget: OptionSet, Hashable {
    let rawValue: Int

    static let debug = OutputTarget(rawValue: 1 << 0)
    static let release = OutputTarget(rawValue: 1 << 1)
    static let all: OutputTarget = [.debug, .release]

    var description: String {
        if self == .debug {
            return "debug"
        } else if self == .release {
            return "release"
        } else {
            return "all"
        }
    }
}

struct CompilationItem {

    indirect enum Kind {
        /**
         A file that is not known yet by Valdi
         */
        case unknown(File)

        /**
         A Valdi document that isn't processed yet
         */
        case rawDocument(File)

        /**
         A parsed Valdi document
         */
        case parsedDocument(ValdiRawDocument)

        /**
         A Valdi document that was compiled, before
         it was fully assembled with its JavaScript
         */
        case document(CompilationResult)

        /**
         An exported type that needs to be converted to a native source.
         */
        case exportedType(ExportedType, ResolvedClassMapping, GeneratedSourceFilename)

        /**
         Dependency metadata that should be exported from the valdi module.
         */
        case exportedDependencyMetadata(ValdiModel, ResolvedClassMapping, GeneratedSourceFilename)

        /**
         A Native code source file (Objective-C or Kotlin)
         */
        case nativeSource(NativeSource)

        /**
         Diagnostics structure describing an exported type (class, interface, enum, function, view class).
         */
        case generatedTypeDescription(GeneratedTypeDescription)

        /**
         A diagnostics file to be emitted when we finish processing
         */
        case diagnosticsFile(File)

        /**
         A file containing the declared strings for a module
         */
        case stringsJSON(File)

        /**
         A file containing the ids declared for a module
         */
        case ids(File)

        /**
         A Kotlin file that needs to be compiled
         */
        case kotlin(File, URL)

        /**
         A raw resource file that was not transformed by Valdi
         */
        case rawResource(File)

        /**
         An Android resource file, meant to be added in the res directory
         */
        case androidResource(String, File)

        /**
         The raw module.yaml file from the module directory
         */
        case moduleYaml(File)

        /**
         The raw BUILD.bazel from the module directory
         */
        case buildFile(File)

        /**
         An image asset with all its available variants
         */
        case imageAsset(ImageAsset)

        /**
         An image resource which was processed from an image asset
         */
        case processedResourceImage(ImageResource)

        /**
         Dependency injection metadata.
         */
        case dependencyMetadata(DependencyMetadata)

        /**
         A Valdi document resource file
         */
        case resourceDocument(DocumentResource)

        /**
         A class mapping XML file
         */
        case classMapping(File)

        /**
         A JavaScript file
         */
        case javaScript(JavaScriptFile)

        /**
         A TypeScript declaration file
         */
        case declaration(JavaScriptFile)

        /**
         A TypeScript file
         */
        case typeScript(File, URL)

        /**
         Result of requesting dumped symbols from a given TypeScript file.
         */
        case dumpedTypeScriptSymbols(DumpedTypeScriptSymbolsResult)

        /**
         a CSS file
         */
        case css(File)

        /**
         A file that finished processing.
         */
        case finalFile(FinalFile)

        /**
         An item that failed to be processed
         */
        case error(Error, originalItemKind: Kind)

        /**
         A ClientSQL source file
         */
        case sql(File)
        case sqlManifest(File)

        /**
         Metadata file from a previous compilation run.
         These files are needed to allow the Compiler to resolve Components defined in .vue files from
         other Valdi modules that have been compiled separately (i.e. when the Compiler
         is driven by Bazel and some .vue file refers to a Component from a .vue file from another module).
         */
        case compilationMetadata(File)

        /**
         When a new downloadable artifact is generated we save this signature file into the module's directory.
         If an existing signature matches the newly generated downloadable artifact, we know we don't need to
         upload it to Bolt again.

         BundleResourcesProcessor can run in a special mode that will fail compilation if a produced downloadable artifact
         doesn't match an existing signature (see BundleResourcesProcessor.DownloadableArtifactsProcessingMode
         */
        case downloadableArtifactSignatures(DownloadableArtifactSignatures)

        /**
         Single iOS Strings resource file
         */
        case iosStringResource(String, File)
    }

    /**
     Info about the bundle that a compilation item belongs to
     */
    class BundleInfo: Hashable {
        /**
         The name of the bundle
         */
        let name: String

        /**
         Absolute url to the bundle's base directory
         */
        let baseDir: URL

        /**
         The prefix to add to all items found within this bundle
         */
        let relativeProjectPathPrefix: String

        let disableHotReload: Bool
        let disableAnnotationProcessing: Bool
        let disableDependencyVerification: Bool
        let disableBazelBuildFileGeneration: Bool
        let asyncStrictMode: Bool

        let webNpmScope: String
        let webVersion: String
        let webPublishConfig: String
        let webMain: String

        let iosModuleName: String
        let iosLanguage: IOSLanguage
        let iosGeneratedContextFactories: [String]

        let isRoot: Bool

        let downloadableAssets: Bool
        let downloadableSources: Bool

        let inclusionConfig: InclusionConfig
        let excludeGlobs: [String]
        let dependencies: [BundleInfo]

        let compilationModeConfig: CompilationModeConfig

        let projectConfig: ValdiProjectConfig

        let iosReleaseOutputDirectories: OutDirectories?
        let androidReleaseOutputDirectories: OutDirectories?
        let webReleaseOutputDirectories: OutDirectories?
        let cppReleaseOutputDirectories: OutDirectories?
        let iosDebugOutputDirectories: OutDirectories?
        let androidDebugOutputDirectories: OutDirectories?
        let webDebugOutputDirectories: OutDirectories?
        let cppDebugOutputDirectories: OutDirectories?

        // for cleaning up if a Valdi module goes from releaseReady to debugOnly
        let iosExpectedReleaseOutputDirectories: OutDirectories?
        let androidExpectedReleaseOutputDirectories: OutDirectories?
        let webExpectedReleaseOutputDirectories: OutDirectories?
        let cppExpectedReleaseOutputDirectories: OutDirectories?

        let androidClassPath: String?
        let iosClassPrefix: String?
        let cppClassPrefix: String?

        let iosCodegenEnabled: Bool
        let androidCodegenEnabled: Bool
        let cppCodegenEnabled: Bool
        let disableCodeCoverage: Bool
        let singleFileCodegen: Bool

        private let iosOutputTarget: ModuleOutputTarget?
        private let androidOutputTarget: ModuleOutputTarget?
        private let webOutputTarget: ModuleOutputTarget?
        private let cppOutputTarget: ModuleOutputTarget?

        enum ModuleStringsConfig {
            case jsonDir(bundleRelativePath: String)

            var bundleRelativePath: String {
                switch self {
                case let .jsonDir(bundleRelativePath):
                    return bundleRelativePath
                }
            }
        }

        let stringsConfig: ModuleStringsConfig?

        init(name: String,
             baseDir: URL,
             relativeProjectPathPrefix: String,
             disableHotReload: Bool,
             disableAnnotationProcessing: Bool,
             disableDependencyVerification: Bool,
             disableBazelBuildFileGeneration: Bool,
             asyncStrictMode: Bool,
             webNpmScope: String,
             webVersion: String,
             webPublishConfig: String,
             webMain: String,
             iosModuleName: String,
             iosLanguage: IOSLanguage,
             iosClassPrefix: String?,
             androidClassPath: String?,
             cppClassPrefix: String?,
             iosCodegenEnabled: Bool,
             androidCodegenEnabled: Bool,
             cppCodegenEnabled: Bool,
             disableCodeCoverage: Bool,
             singleFileCodegen: Bool,
             compilationModeConfig: CompilationModeConfig,
             iosOutputTarget: ModuleOutputTarget?,
             iosGeneratedContextFactories: [String],
             androidOutputTarget: ModuleOutputTarget?,
             webOutputTarget: ModuleOutputTarget?,
             cppOutputTarget: ModuleOutputTarget?,
             downloadableAssets: Bool,
             downloadableSources: Bool,
             inclusionConfig: InclusionConfig,
             excludeGlobs: [String],
             dependencies: [BundleInfo],
             stringsConfig: ModuleStringsConfig?,
             projectConfig: ValdiProjectConfig,
             outputTarget: OutputTarget) throws {
            self.name = name
            self.baseDir = baseDir
            self.relativeProjectPathPrefix = relativeProjectPathPrefix
            self.disableHotReload = disableHotReload
            self.disableAnnotationProcessing = disableAnnotationProcessing
            self.disableDependencyVerification = disableDependencyVerification
            self.disableBazelBuildFileGeneration = disableBazelBuildFileGeneration
            self.asyncStrictMode = asyncStrictMode
            self.webNpmScope = webNpmScope
            self.webVersion = webVersion
            self.webPublishConfig = webPublishConfig
            self.webMain = webMain
            self.iosModuleName = iosModuleName
            self.iosLanguage = iosLanguage
            self.iosOutputTarget = iosOutputTarget
            self.iosGeneratedContextFactories = iosGeneratedContextFactories
            self.androidOutputTarget = androidOutputTarget
            self.webOutputTarget = webOutputTarget
            self.cppOutputTarget = cppOutputTarget
            self.disableCodeCoverage = disableCodeCoverage
            self.downloadableAssets = downloadableAssets
            self.downloadableSources = downloadableSources
            self.isRoot = name == BundleInfo.rootModuleName
            self.inclusionConfig = inclusionConfig
            self.excludeGlobs = excludeGlobs
            self.dependencies = dependencies
            self.allDependencies = Self.collectAllDependencies(dependencies)
            self.allDependenciesNames = Set(allDependencies.map { $0.name })

            self.compilationModeConfig = compilationModeConfig
            self.projectConfig = projectConfig

            if !isRoot {
                let iosExpectedDebugOutputDirectories = try projectConfig.iosOutput?.debugPath.flatMap { try Self.iosOutputDirectories(iosOutputPath: $0,
                                                                                                                                   moduleName: name,
                                                                                                                                   iosModuleName: iosModuleName,
                                                                                                                                   projectConfig: projectConfig) }
                self.iosExpectedReleaseOutputDirectories = try Self.iosOutputDirectories(iosOutputPath: projectConfig.iosOutput?.releasePath,
                                                                                         moduleName: name,
                                                                                         iosModuleName: iosModuleName,
                                                                                         projectConfig: projectConfig)

                (self.iosDebugOutputDirectories, self.iosReleaseOutputDirectories) = BundleInfo.getOutputDirectories(
                    moduleOutputTarget: iosOutputTarget,
                    compileOutputTarget: outputTarget,
                    expectedDebugOutputDirectories: iosExpectedDebugOutputDirectories,
                    expectedReleaseOutputDirectories: iosExpectedReleaseOutputDirectories)

                let androidExpectedDebugOutputDirectories = try projectConfig.androidOutput?.debugPath.flatMap {  try Self.androidOutputDirectories(androidOutputPath: $0,
                                                                                                                                                moduleName: name,
                                                                                                                                                projectConfig: projectConfig)  }
                self.androidExpectedReleaseOutputDirectories = try Self.androidOutputDirectories(androidOutputPath: projectConfig.androidOutput?.releasePath,
                                                                                                 moduleName: name,
                                                                                                 projectConfig: projectConfig)

                (self.androidDebugOutputDirectories, self.androidReleaseOutputDirectories) = BundleInfo.getOutputDirectories(
                    moduleOutputTarget: androidOutputTarget,
                    compileOutputTarget: outputTarget,
                    expectedDebugOutputDirectories: androidExpectedDebugOutputDirectories,
                    expectedReleaseOutputDirectories: androidExpectedReleaseOutputDirectories)

                let webExpectedDebugOutputDirectories = try projectConfig.webOutput?.debugPath.flatMap {
                    return try Self.webOutputDirectories(webOutputPath: $0,
                                                         moduleName: name,
                                                         projectConfig: projectConfig)
                }
                self.webExpectedReleaseOutputDirectories = try Self.webOutputDirectories(webOutputPath: projectConfig.webOutput?.releasePath,
                                                                                         moduleName: name,
                                                                                         projectConfig: projectConfig)

                (self.webDebugOutputDirectories, self.webReleaseOutputDirectories) = BundleInfo.getOutputDirectories(
                    moduleOutputTarget: webOutputTarget,
                    compileOutputTarget: outputTarget,
                    expectedDebugOutputDirectories: webExpectedDebugOutputDirectories,
                    expectedReleaseOutputDirectories: webExpectedReleaseOutputDirectories)

                let cppExpectedDebugOutputDirectories = try projectConfig.cppOutput?.debugPath.flatMap {
                    return try Self.cppOutputDirectories(cppOutputPath: $0,
                                                         moduleName: name,
                                                         projectConfig: projectConfig)
                }
                self.cppExpectedReleaseOutputDirectories = try Self.cppOutputDirectories(cppOutputPath: projectConfig.cppOutput?.releasePath,
                                                                                         moduleName: name,
                                                                                         projectConfig: projectConfig)

                (self.cppDebugOutputDirectories, self.cppReleaseOutputDirectories) = BundleInfo.getOutputDirectories(
                    moduleOutputTarget: cppOutputTarget,
                    compileOutputTarget: outputTarget,
                    expectedDebugOutputDirectories: cppExpectedDebugOutputDirectories,
                    expectedReleaseOutputDirectories: cppExpectedReleaseOutputDirectories)
            } else {
                self.iosReleaseOutputDirectories = nil
                self.iosDebugOutputDirectories = nil
                self.iosExpectedReleaseOutputDirectories = nil
                self.androidReleaseOutputDirectories = nil
                self.androidDebugOutputDirectories = nil
                self.androidExpectedReleaseOutputDirectories = nil
                self.webReleaseOutputDirectories = nil
                self.webDebugOutputDirectories = nil
                self.webExpectedReleaseOutputDirectories = nil
                self.cppReleaseOutputDirectories = nil
                self.cppDebugOutputDirectories = nil
                self.cppExpectedReleaseOutputDirectories = nil
            }

            self.iosClassPrefix = iosClassPrefix
            self.androidClassPath = androidClassPath
            self.cppClassPrefix = cppClassPrefix

            self.iosCodegenEnabled = iosCodegenEnabled
            self.androidCodegenEnabled = androidCodegenEnabled
            self.cppCodegenEnabled = cppCodegenEnabled
            self.singleFileCodegen = singleFileCodegen

            self.stringsConfig = stringsConfig
        }

        private static func getOutputDirectories(moduleOutputTarget: ModuleOutputTarget?,
                                                 compileOutputTarget: OutputTarget, 
                                                 expectedDebugOutputDirectories: OutDirectories?,
                                                 expectedReleaseOutputDirectories: OutDirectories?) -> (debug: OutDirectories?, release: OutDirectories?) {
            let debugCompilationEnabled = compileOutputTarget.contains(.debug)
            let releaseCompilationEnabled = compileOutputTarget.contains(.release)

            switch moduleOutputTarget {
            case .none, .debugOnly:
                return (debug: debugCompilationEnabled ? expectedDebugOutputDirectories : nil, release: nil)
            case .releaseReady:
                return (debug: debugCompilationEnabled ? expectedDebugOutputDirectories : nil, release: releaseCompilationEnabled ? expectedReleaseOutputDirectories: nil)
            }
        }

        func relativeProjectPath(forItemPath itemPath: String) -> String {
            return self.relativeProjectPathPrefix + itemPath
        }

        func relativeProjectPath(forItemURL itemURL: URL) -> String {
            let itemPath = self.itemPath(forItemURL: itemURL)
            return relativeProjectPath(forItemPath: itemPath)
        }

        func itemPath(forItemURL itemURL: URL) -> String {
            let resolvedItemURL = (try? itemURL.resolvingSymlink(resolveParentDirs: true)) ?? itemURL
            return baseDir.relativePath(to: resolvedItemURL)
        }

        func itemPath(forRelativeProjectPath relativeProjectPath: String) throws -> String {
            guard relativeProjectPath.hasPrefix(self.relativeProjectPathPrefix) else {
                throw CompilerError("Expecting relative project path to start with '\(self.relativeProjectPathPrefix)'")
            }

            return String(relativeProjectPath[self.relativeProjectPathPrefix.endIndex...])
        }

        func relativeBundleURL(forRelativeProjectPath relativeProjectPath: String, sourceURL: URL) -> URL {
            if isRoot {
                // We're getting paths with <root> in them here
                guard let url = URL(string: relativeProjectPath) else {
                    return URL(string: relativeProjectPath.replacingOccurrences(of: BundleInfo.rootModuleName, with: ""))!
                }
                return url
            } else {
                guard relativeProjectPath.hasPrefix(name) else {
                    return URL(string: baseDir.relativePath(to: sourceURL))!
                }

                let bundlePath = String(relativeProjectPath[relativeProjectPath.index(name.endIndex, offsetBy: 1)..<relativeProjectPath.endIndex])

                return URL(string: bundlePath)!
            }
        }

        func absoluteURL(forRelativeProjectPath relativeProjectPath: String) -> URL {
            return baseDir.deletingLastPathComponent().appendingPathComponent(relativeProjectPath)
        }

        func resolvedBundleName() -> String {
            return BundleManager.resolveBundleName(bundleName: name)
        }

        private static func iosOutputDirectories(iosOutputPath: UnresolvedPath?, moduleName: String, iosModuleName: String, projectConfig: ValdiProjectConfig) throws -> OutDirectories? {
            let outDirectories = try BundleInfo.resolveIOSOutputDirectories(outputPath: iosOutputPath,
                                                                            metadataPath: projectConfig.iosOutput?.metadataPath,
                                                                            moduleName: moduleName,
                                                                            iosModuleName: iosModuleName,
                                                                            projectConfig: projectConfig)
            return outDirectories
        }

        private static func androidOutputDirectories(androidOutputPath: UnresolvedPath?, moduleName: String, projectConfig: ValdiProjectConfig) throws -> OutDirectories? {
            let outDirectories = try BundleInfo.resolveOutputDirectories(outputPath: androidOutputPath,
                                                                         metadataPath: projectConfig.androidOutput?.metadataPath,
                                                                         moduleName: moduleName,
                                                                         projectConfig: projectConfig,
                                                                         assetDir: "assets")
            return outDirectories
        }

        private static func webOutputDirectories(webOutputPath: UnresolvedPath?, moduleName: String, projectConfig: ValdiProjectConfig) throws -> OutDirectories? {
            let outDirectories = try BundleInfo.resolveWebOutputDirectories(outputPath: webOutputPath,
                                                                         metadataPath: projectConfig.webOutput?.metadataPath,
                                                                         moduleName: moduleName,
                                                                         projectConfig: projectConfig,
                                                                         assetDir: "assets")
            return outDirectories
        }

        private static func cppOutputDirectories(cppOutputPath: UnresolvedPath?, moduleName: String, projectConfig: ValdiProjectConfig) throws -> OutDirectories? {
            let outDirectories = try BundleInfo.resolveCppOutputDirectories(outputPath: cppOutputPath,
                                                                            metadataPath: projectConfig.webOutput?.metadataPath,
                                                                            moduleName: moduleName,
                                                                            projectConfig: projectConfig)
            return outDirectories
        }

        private static func resolveModulePath(_ path: UnresolvedPath, moduleName: String) throws -> URL {
            return try path.appendingVariable(key: "MODULE_NAME", value: moduleName).resolve()
        }

        private static func resolveOutputAndMetadataURL(outputPath: UnresolvedPath?, metadataPath: UnresolvedPath?, moduleName: String) throws -> (outputURL: URL, metadataURL: URL?)? {
            guard let outputPath = outputPath else {
                return nil
            }

            let outputURL = try resolveModulePath(outputPath, moduleName: moduleName)

            let moduleMetadataURL: URL?
            if let metadataPath = metadataPath {
                moduleMetadataURL = try resolveModulePath(metadataPath, moduleName: moduleName)
            } else {
                moduleMetadataURL = nil
            }

            return (outputURL, moduleMetadataURL)
        }

        private static func resolveOutputDirectories(outputPath: UnresolvedPath?, metadataPath: UnresolvedPath?, moduleName: String, projectConfig: ValdiProjectConfig, assetDir: String) throws -> OutDirectories? {
            guard let (outputURL, metadataURL) = try resolveOutputAndMetadataURL(outputPath: outputPath, metadataPath: metadataPath, moduleName: moduleName) else {
                return nil
            }

            let assetsURL: URL = outputURL.appendingPathComponent(assetDir, isDirectory: true)

            let srcURL = outputURL.appendingPathComponent("src", isDirectory: true)
            let nativeSrcURL = outputURL.appendingPathComponent("native", isDirectory: true)
            let resourcesURL = outputURL.appendingPathComponent("res", isDirectory: true)

            return OutDirectories(
                baseURL: outputURL,
                assetsURL: assetsURL,
                srcURL: srcURL,
                nativeSrcURL: nativeSrcURL,
                resourcesURL: resourcesURL,
                metadataURL: metadataURL)
        }

        private static func resolveWebOutputDirectories(outputPath: UnresolvedPath?, metadataPath: UnresolvedPath?, moduleName: String, projectConfig: ValdiProjectConfig, assetDir: String) throws -> OutDirectories? {
            if !projectConfig.webEnabled {
                return nil
            }
            
            guard let (outputURL, metadataURL) = try resolveOutputAndMetadataURL(outputPath: outputPath, metadataPath: metadataPath, moduleName: moduleName) else {
                return nil
            }

            // All compiled typescript files are being saved as assets,
            // file naming overlap between modules is causing problems so let's shove them
            // into their own named directories
            // Including resources in this extra path thing
            // TODO(cholgate): fix this
            let assetsURL: URL = outputURL.appendingPathComponent(assetDir, isDirectory: true).appendingPathComponent(moduleName, isDirectory: true)
            let resourcesURL = outputURL.appendingPathComponent("res", isDirectory: true).appendingPathComponent(moduleName, isDirectory: true)


            let srcURL = outputURL.appendingPathComponent("src", isDirectory: true).appendingPathComponent(moduleName, isDirectory: true)
            let nativeSrcURL = outputURL.appendingPathComponent("native", isDirectory: true)

            return OutDirectories(
                baseURL: outputURL,
                assetsURL: assetsURL,
                srcURL: srcURL,
                nativeSrcURL: nativeSrcURL,
                resourcesURL: resourcesURL,
                metadataURL: metadataURL)
        }

        private static func resolveCppOutputDirectories(outputPath: UnresolvedPath?, metadataPath: UnresolvedPath?, moduleName: String, projectConfig: ValdiProjectConfig) throws -> OutDirectories? {
            guard let (outputURL, metadataURL) = try resolveOutputAndMetadataURL(outputPath: outputPath, metadataPath: metadataPath, moduleName: moduleName) else {
                return nil
            }

            let assetsURL: URL = outputURL.appendingPathComponent("assets", isDirectory: true)

            let srcURL = outputURL.appendingPathComponent("src", isDirectory: true)
            let resourcesURL = outputURL.appendingPathComponent("res", isDirectory: true)

            return OutDirectories(
                baseURL: outputURL,
                assetsURL: assetsURL,
                srcURL: srcURL,
                nativeSrcURL: srcURL,
                resourcesURL: resourcesURL,
                metadataURL: metadataURL)
        }

        private static func resolveIOSOutputDirectories(outputPath: UnresolvedPath?, metadataPath: UnresolvedPath?, moduleName: String, iosModuleName: String, projectConfig: ValdiProjectConfig) throws -> OutDirectories? {
            guard let (outputURL, metadataURL) = try resolveOutputAndMetadataURL(outputPath: outputPath, metadataPath: metadataPath, moduleName: iosModuleName) else {
                return nil
            }

            let moduleAssetsURL = outputURL.appendingPathComponent("assets", isDirectory: true)

            let srcURL = outputURL.appendingPathComponent("src", isDirectory: true).appendingPathComponent(iosModuleName, isDirectory: true)
            let nativeSrcURL = outputURL.appendingPathComponent("native", isDirectory: true).appendingPathComponent(iosModuleName, isDirectory: true)
            let resourcesURL = moduleAssetsURL.appendingPathComponent("\(moduleName).bundle", isDirectory: true)

            return OutDirectories(
                baseURL: outputURL,
                assetsURL: moduleAssetsURL,
                srcURL: srcURL,
                nativeSrcURL: nativeSrcURL,
                resourcesURL: resourcesURL,
                metadataURL: metadataURL)
        }

        static func==(lhs: BundleInfo, rhs: BundleInfo) -> Bool {
            return lhs === rhs
        }

        func hash(into hasher: inout Hasher) {
            hasher.combine(baseDir)
        }

        func outputTarget(platform: Platform) -> ModuleOutputTarget? {
            switch platform {
            case .ios:
                return iosOutputTarget
            case .android:
                return androidOutputTarget
            case .web:
                return webOutputTarget
            case .cpp:
                return cppOutputTarget
            }
        }

        let allDependencies: Set<BundleInfo>

        let allDependenciesNames: Set<String>

        /**
         Placeholder used for the root module
         */
        static let rootModuleName = "<root>"

        private static func collectAllDependencies(_ dependencies: [BundleInfo]) -> Set<BundleInfo> {
            var modules = Set<BundleInfo>()

            var queue = dependencies
            while !queue.isEmpty {
                let current = queue.removeLast()
                if !modules.contains(current) {
                    modules.insert(current)
                    queue.append(contentsOf: current.dependencies)
                }
            }

            return modules
        }
    }

    let sourceURL: URL
    let kind: Kind
    let bundleInfo: BundleInfo
    let platform: Platform?
    let outputTarget: OutputTarget

    /// URL relative to the bundle's base directory
    let relativeBundleURL: URL

    /**
     The path identifying this item within the compilation unit.
     From within the compiler, the file will be available to be imported with
     this given path.
     */
    let relativeProjectPath: String

    /**
     Returns the relative path from the bundle base's directory
     */
    var relativeBundlePath: RelativePath {
        return RelativePath(url: self.relativeBundleURL)
    }

    init(relativeProjectPath: String, kind: Kind, bundleInfo: BundleInfo, platform: Platform?, outputTarget: OutputTarget) {
        self.init(sourceURL: bundleInfo.absoluteURL(forRelativeProjectPath: relativeProjectPath),
                  relativeProjectPath: relativeProjectPath,
                  kind: kind,
                  bundleInfo: bundleInfo,
                  platform: platform,
                  outputTarget: outputTarget)
    }

    init(sourceURL: URL, relativeProjectPath: String?, kind: Kind, bundleInfo: BundleInfo, platform: Platform?, outputTarget: OutputTarget) {
        self.sourceURL = sourceURL

        if let relativeProjectPath {
            self.relativeBundleURL = bundleInfo.relativeBundleURL(forRelativeProjectPath: relativeProjectPath, sourceURL: sourceURL)
            self.relativeProjectPath = relativeProjectPath
        } else {
            let itemPath = bundleInfo.itemPath(forItemURL: sourceURL)
            let relativeProjectPath = bundleInfo.relativeProjectPath(forItemPath: itemPath)
            guard let relativeBundleURL = URL(string: itemPath) else {
                fatalError("Could not resolve URL from \(sourceURL)")
            }

            self.relativeBundleURL = relativeBundleURL
            self.relativeProjectPath = relativeProjectPath
        }

        self.kind = kind
        self.bundleInfo = bundleInfo
        self.platform = platform
        self.outputTarget = outputTarget
    }

    init(generatedFromBundleInfo bundleInfo: BundleInfo, kind: Kind, platform: Platform?, outputTarget: OutputTarget) {
        self.sourceURL = bundleInfo.baseDir
        self.relativeBundleURL = URL(string: ".")!
        self.relativeProjectPath = bundleInfo.relativeProjectPath(forItemPath: ".")

        self.kind = kind
        self.bundleInfo = bundleInfo
        self.platform = platform
        self.outputTarget = outputTarget
    }

    func with(newKind: Kind, newPlatform: Platform?) -> CompilationItem {
        return with(newKind: newKind, newPlatform: newPlatform, newOutputTarget: self.outputTarget)
    }

    func with(newKind: Kind, newPlatform: Platform?, newOutputTarget: OutputTarget) -> CompilationItem {
        return CompilationItem(sourceURL: sourceURL, relativeProjectPath: self.relativeProjectPath, kind: newKind, bundleInfo: bundleInfo, platform: newPlatform, outputTarget: newOutputTarget)
    }

    func with(newPlatform: Platform) -> CompilationItem {
        return with(newKind: self.kind, newPlatform: newPlatform)
    }

    func with(newKind: Kind) -> CompilationItem {
        return with(newKind: newKind, newPlatform: self.platform)
    }

    func with(error: Error) -> CompilationItem {
        return with(newKind: .error(error, originalItemKind: self.kind))
    }

    func with(newOutputTarget: OutputTarget) -> CompilationItem {
        return with(newKind: kind, newPlatform: self.platform, newOutputTarget: newOutputTarget)
    }

    func toString() -> String {
        // For debugging purposes
        return "url: \(sourceURL) platform: \(platform.debugDescription) kind: \(kind)"
    }
}

extension CompilationItem {

    func shouldOutput(to outputPlatform: Platform) -> Bool {
        return platform.map { $0 == outputPlatform } ?? true
    }

    var shouldOutputToAndroid: Bool {
        return shouldOutput(to: .android)
    }

    var shouldOutputToIOS: Bool {
        return shouldOutput(to: .ios)
    }

    var shouldOutputToWeb: Bool {
        return shouldOutput(to: .web)
    }

    var shouldOutputToCpp: Bool {
        return shouldOutput(to: .cpp)
    }

    /// Indicate whether the item is a test compilation item.
    /// Test items are located under "test" subdirectory.
    var isTestItem: Bool {
        // test sources will be in module/test directory
        return relativeBundleURL.pathComponents.first == "test"
    }

    var error: Error? {
        if case .error(let error, _) = self.kind {
            return error
        } else {
            return nil
        }
    }
}
