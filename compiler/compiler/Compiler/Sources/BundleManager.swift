//
//  BundleManager.swift
//  Compiler
//
//  Created by Simon Corsin on 3/1/19.
//

import Foundation
import Yams

private struct CircularDependencyError: LocalizedError {
    let moduleChain: [String]

    init(moduleChain: [String]) {
        self.moduleChain = moduleChain
    }

    public var errorDescription: String? {
        let dependencyChain = moduleChain.joined(separator: " -> ")
        return """
        Circular dependency detected while loading module '\(moduleChain.first!)'
        Dependency chain: \(dependencyChain)\n
        """
    }
}

class BundleManager {
    struct RegisteredBundle {
        let url: URL
        let file: File
    }

    private var bundleURLByName = [String: RegisteredBundle]()
    private var bundleByName = [String: CompilationItem.BundleInfo]()
    private var bundleNameByDirURL = [URL: String]()
    private let lock = DispatchSemaphore.newLock()

    private let projectConfig: ValdiProjectConfig
    private let compilerConfig: CompilerConfig
    private var loadingBundleURLs = Set<URL>()
    private let baseDirURLs: [URL]
    // Used during module info resolving so that we can tell whether we
    // are still within the allowed base dirs while going through ancestor
    // directories.
    private var baseDirsTrie = Trie<Bool>()
    let rootBundle: CompilationItem.BundleInfo

    init(projectConfig: ValdiProjectConfig,
         compilerConfig: CompilerConfig,
         baseDirURLs: [URL]) throws {
        self.projectConfig = projectConfig
        self.compilerConfig = compilerConfig
        self.baseDirURLs = baseDirURLs.sorted(by: { left, right in
            return left.pathComponents.count > right.pathComponents.count
        })

        for baseDirURL in baseDirURLs {
            self.baseDirsTrie.insert(name: baseDirURL.pathComponents, value: true)
        }

        // Every module will have a dependency on the root
        rootBundle = try CompilationItem.BundleInfo(name: CompilationItem.BundleInfo.rootModuleName,
                                                    baseDir: projectConfig.baseDir,
                                                    relativeProjectPathPrefix: "",
                                                    disableHotReload: false,
                                                    disableAnnotationProcessing: false,
                                                    disableDependencyVerification: true,
                                                    disableBazelBuildFileGeneration: true,
                                                    asyncStrictMode: false,
                                                    webNpmScope: "",
                                                    webVersion: "",
                                                    webPublishConfig: "",
                                                    webMain: "",
                                                    iosModuleName: "",
                                                    iosLanguage: IOSLanguage.objc,
                                                    iosClassPrefix: nil,
                                                    androidClassPath: nil,
                                                    cppClassPrefix: nil,
                                                    iosCodegenEnabled: false,
                                                    androidCodegenEnabled: false,
                                                    cppCodegenEnabled: false,
                                                    disableCodeCoverage: false,
                                                    singleFileCodegen: false,
                                                    compilationModeConfig: CompilationModeConfig(js: nil, jsBytecode: nil, native: nil),
                                                    iosOutputTarget: .releaseReady,
                                                    iosGeneratedContextFactories: [],
                                                    androidOutputTarget: .releaseReady,
                                                    webOutputTarget: .releaseReady,
                                                    cppOutputTarget: .releaseReady,
                                                    downloadableAssets: false,
                                                    downloadableSources: false,
                                                    inclusionConfig: InclusionConfig.alwaysIncluded,
                                                    excludeGlobs: [],
                                                    dependencies: [],
                                                    stringsConfig: nil,
                                                    projectConfig: projectConfig,
                                                    outputTarget: compilerConfig.outputTarget
        )
        self.bundleByName[rootBundle.name] = rootBundle
        self.bundleNameByDirURL[projectConfig.baseDir] = rootBundle.name
        assert(rootBundle.isRoot)
    }

    func registerBundle(forName bundleName: String, bundleURL: URL, file: File) {
        lock.lock {
            self.bundleURLByName[bundleName] = RegisteredBundle(url: bundleURL, file: file)
            self.bundleNameByDirURL[bundleURL.deletingLastPathComponent()] = bundleName
        }
    }

    func getBundleInfo(forName name: String) throws -> CompilationItem.BundleInfo {
        return try lock.lock {
            try getBundleInfo(bundleName: name)
        }
    }

    func getBundleInfo(fileURL: URL) throws -> CompilationItem.BundleInfo {
        let directoryURL = fileURL.deletingLastPathComponent()

        return try lock.lock {
            return try resolveBundleInfo(itemURL: fileURL, dirURL: directoryURL)
        }
    }

    static func parseOutputTarget(node: Yams.Node?) throws -> ModuleOutputTarget? {
        return try parseOutputTarget(string: node?["output_target"]?.string)
    }

    static func parseOutputTarget(mapping: Yams.Node.Mapping?) throws -> ModuleOutputTarget? {
        return try parseOutputTarget(string: mapping?["output_target"]?.string)
    }

    static func parseOutputTarget(string: String?) throws -> ModuleOutputTarget? {
        guard let outputTargetString = string?.lowercased() else {
            return nil
        }

        let outputTarget: ModuleOutputTarget
        switch outputTargetString {
        case "debug":
            outputTarget = .debugOnly
        case "release":
            outputTarget = .releaseReady
        default:
            throw CompilerError("Unrecognized output target '\(outputTargetString)', supported values are 'debug' and 'release'")
        }
        return outputTarget
    }

    static func getBundleNameComponents(bundleName: String) -> (moduleName: String, scope: String) {
        // Resolves scoped bundleName to its moduleName and scope components
        // Assume scoped bundleName uses '@'
        return bundleName.split(at: "@")
    }

    static func resolveBundleName(bundleName: String) -> String {
        // Resolves concatenated intermediate module names in the form of <name><scope> to <scope>/<name> for path resolution purposes
        // Ex: "module@scope" -> "@scope/module"
        let (moduleName, scope) = getBundleNameComponents(bundleName: bundleName)
        return scope.isEmpty ? moduleName : "@\(scope)/\(moduleName)"
    }

    private static func resolveCodegenEnabled(config: Yams.Node.Mapping?, outputConfig: ValdiOutputConfig?) -> Bool {
        guard let outputConfig else { return false }

        return config?["codegen_enabled"]?.bool ?? outputConfig.codegenEnabled
    }
    
    private static func resolveGeneratedContextFactories(config: Yams.Node.Mapping?) -> [String] {
        guard let config = config else { return [] }
        
        return config["generated_context_factories"]?.array().compactMap({ $0.string }) ?? []
    }

    private func loadBundle(bundleName: String, bundleURL: URL, bundleFile: File) throws -> CompilationItem.BundleInfo {
        if loadingBundleURLs.contains(bundleURL) {
            throw CircularDependencyError(moduleChain: [bundleName])
        }

        loadingBundleURLs.insert(bundleURL)
        defer {
            loadingBundleURLs.remove(bundleURL)
        }

        let resolvedBundleDir = try bundleURL.deletingLastPathComponent().resolvingSymlink()

        let configData: String
        do {
            configData = try bundleFile.readString()
        } catch let error {
            throw CompilerError("Failed to read module file at \(bundleURL.path): \(error.legibleLocalizedDescription)")
        }

        let config: Yams.Node
        do {
            guard let configMaybe = try Yams.compose(yaml: configData) else {
                throw CompilerError("YAML parsing returned nil")
            }
            config = configMaybe
        } catch let error {
            throw CompilerError("Failed to parse module file at \(bundleURL.path): \(error.legibleLocalizedDescription)")
        }

        let iosConfig = config["ios"]?.mapping
        let androidConfig = config["android"]?.mapping
        let webConfig = config["web"]?.mapping
        let cppConfig = config["cpp"]?.mapping

        let webNpmScope = webConfig?["scope"]?.string ?? ""
        let webVersion = webConfig?["version"]?.string ?? "1.0.0"
        let webPublishConfig = webConfig?["publish_config"]?.string ?? ""
        let webMain = webConfig?["main"]?.string ?? ""

        let iosClassPrefix = iosConfig?["class_prefix"]?.string
        let cppClassPrefix = cppConfig?["class_prefix"]?.string

        let disableHotReload = config["disable_hotreload"]?.bool ?? false
        let disableAnnotationProcessing = config["disable_annotation_processing"]?.bool ?? false
        let disableDependencyVerification = config["disable_dependency_verification"]?.bool ?? false
        let disableCodeCoverage = config["disable_code_coverage"]?.bool ?? false
        let disableBazelBuildFileGeneration = config["bazel_build_file_generation_disabled"]?.bool ?? false
        let asyncStrictMode = config["async_strict_mode"]?.bool ?? false

        let compilationModeConfig = try config["compilation_mode"].map { try CompilationModeConfig.parse(from: $0) } ?? CompilationModeConfig.forJsBytecode()

        let iosModuleName: String
        if let iosConfigName = iosConfig?["module_name"]?.string?.trimmed {
            iosModuleName = iosConfigName
        } else {
            let (moduleName, _) = BundleManager.getBundleNameComponents(bundleName: bundleName)
            iosModuleName = String.concatenate(projectConfig.iosDefaultModuleNamePrefix, moduleName.pascalCased) ?? moduleName.pascalCased
        }
        
        let iosLanguage: IOSLanguage
        if let language = iosConfig?["language"]?.string {
            iosLanguage = try IOSLanguage.parse(input: language)
        } else {
            iosLanguage = projectConfig.iosDefaultLanguage
        }

        let parsedCommonOutputTarget = try Self.parseOutputTarget(node: config)
        let parsedIosOutputTarget = try Self.parseOutputTarget(mapping: iosConfig)
        let parsedAndroidOutputTarget = try Self.parseOutputTarget(mapping: androidConfig)
        let parsedWebOutputTarget = try Self.parseOutputTarget(mapping: webConfig)

        let iosOutputTarget: ModuleOutputTarget?
        let androidOutputTarget: ModuleOutputTarget?
        let webOutputTarget: ModuleOutputTarget?
        switch (parsedCommonOutputTarget, parsedIosOutputTarget, parsedAndroidOutputTarget) {
        case (.none, .none, .none):
            throw CompilerError("No output_target in the \(bundleName) module.yaml. Supported values are 'debug' and 'release'")
        case (.some, .some, _), (.some, _, .some):
            throw CompilerError("If you specify a platform-specific output target, you need to specify it for both platforms.")
        case let (.some(commonOutputTarget), .none, .none):
            iosOutputTarget = commonOutputTarget
            androidOutputTarget = commonOutputTarget
            webOutputTarget = commonOutputTarget
        case (.none, .some, _), (.none, _, .some):
            iosOutputTarget = parsedIosOutputTarget
            androidOutputTarget = parsedAndroidOutputTarget
            webOutputTarget = parsedWebOutputTarget
        }

        let inclusionConfig = try InclusionConfig.parse(from: config)

        var excludeGlobs = [String]()

        if let excludeGlobsArray = config["exclude_globs"]?.array().compactMap({ $0.string }) {
            for glob in excludeGlobsArray {
                excludeGlobs.append(glob)
            }
        }

        // Enable downloadable assets by default
        var downloadableAssets = true
        var downloadableSources = false

        if let downloadable = config["downloadable"] {
            if let allDownloadable = downloadable.bool, allDownloadable {
                downloadableAssets = true
                downloadableSources = true
            } else {
                if let downloadableAssetsCfg = downloadable["assets"]?.bool {
                    downloadableAssets = downloadableAssetsCfg
                }
                downloadableSources = downloadable["sources"]?.bool ?? false
            }
        }

        if compilerConfig.disableDownloadableModules {
            downloadableAssets = false
            downloadableSources = false
        }

        if [iosOutputTarget, androidOutputTarget, webOutputTarget].contains(.debugOnly), downloadableSources {
            throw CompilerError("Module '\(bundleName)' is marked as downloadable, but the module's output target is set to 'debug'. A debug-only module cannot be downloadable to avoid easily leaking unreleased features.")
        }

        let androidClassPath = androidConfig?["class_path"]?.string

        let dependencyStrings = config["dependencies"]?.array().compactMap({ $0.string }) ?? [String]()
        var dependencies = try resolveDependencies(ofModule: bundleName,
                                                   moduleURL: bundleURL,
                                                   dependencyStrings: dependencyStrings)
        let debugOnlyDependencyStrings = config["allowed_debug_dependencies"]?.array().compactMap({ $0.string }) ?? [String]()
        let allowedDebugDependencies = try resolveDependencies(ofModule: bundleName,
                                                               moduleURL: bundleURL,
                                                               dependencyStrings: debugOnlyDependencyStrings)
        dependencies.append(contentsOf: Set(allowedDebugDependencies).subtracting(Set(dependencies)))

        dependencies.append(rootBundle)
        dependencies.sort {
            $0.name < $1.name
        }

        let stringsConfig: CompilationItem.BundleInfo.ModuleStringsConfig?
        if let stringsDir = config["strings_dir"]?.string {
            stringsConfig = .jsonDir(bundleRelativePath: stringsDir)
        } else {
            stringsConfig = nil
        }

        let iosCodegenEnabled = Self.resolveCodegenEnabled(config: iosConfig, outputConfig: projectConfig.iosOutput)
        let androidCodegenEnabled = Self.resolveCodegenEnabled(config: androidConfig, outputConfig: projectConfig.androidOutput)
        let cppCodegenEnabled = Self.resolveCodegenEnabled(config: cppConfig, outputConfig: projectConfig.cppOutput)


        let iosGeneratedContextFactories = Self.resolveGeneratedContextFactories(config: iosConfig)

        let relativeProjectPathPrefix = config["path_prefix"]?.string ?? "\(bundleName)/"

        let singleFileCodegen = config["single_file_codegen"]?.bool ?? false

        let bundleInfo = try CompilationItem.BundleInfo(name: bundleName,
                                                        baseDir: resolvedBundleDir,
                                                        relativeProjectPathPrefix: relativeProjectPathPrefix,
                                                        disableHotReload: disableHotReload,
                                                        disableAnnotationProcessing: disableAnnotationProcessing,
                                                        disableDependencyVerification: disableDependencyVerification,
                                                        disableBazelBuildFileGeneration: disableBazelBuildFileGeneration,
                                                        asyncStrictMode: asyncStrictMode,
                                                        webNpmScope: webNpmScope,
                                                        webVersion: webVersion,
                                                        webPublishConfig: webPublishConfig,
                                                        webMain: webMain,
                                                        iosModuleName: iosModuleName,
                                                        iosLanguage: iosLanguage,
                                                        iosClassPrefix: iosClassPrefix,
                                                        androidClassPath: androidClassPath,
                                                        cppClassPrefix: cppClassPrefix,
                                                        iosCodegenEnabled: iosCodegenEnabled,
                                                        androidCodegenEnabled: androidCodegenEnabled,
                                                        cppCodegenEnabled: cppCodegenEnabled,
                                                        disableCodeCoverage: disableCodeCoverage,
                                                        singleFileCodegen: singleFileCodegen,
                                                        compilationModeConfig: compilationModeConfig,
                                                        iosOutputTarget: iosOutputTarget,
                                                        iosGeneratedContextFactories: iosGeneratedContextFactories,
                                                        androidOutputTarget: androidOutputTarget,
                                                        webOutputTarget: webOutputTarget,
                                                        cppOutputTarget: .releaseReady,
                                                        downloadableAssets: downloadableAssets,
                                                        downloadableSources: downloadableSources,
                                                        inclusionConfig: inclusionConfig,
                                                        excludeGlobs: excludeGlobs,
                                                        dependencies: dependencies,
                                                        stringsConfig: stringsConfig,
                                                        projectConfig: projectConfig,
                                                        outputTarget: compilerConfig.outputTarget)

        try [Platform.ios, Platform.android, Platform.web].forEach { platform in
            guard case .releaseReady = bundleInfo.outputTarget(platform: platform) else {
                return
            }

            let unexpectedDebugDependencies = dependencies.filter { dep -> Bool in
                return dep.outputTarget(platform: platform) != .releaseReady && !allowedDebugDependencies.contains(dep)
            }
            guard unexpectedDebugDependencies.isEmpty else {
                throw CompilerError("Module '\(bundleName)''s output_target is set to 'release' when building for \(platform), but it depends on the following 'debug' targets: \(unexpectedDebugDependencies.map { $0.name }). If this is intentional, please list them in your module's module.yaml under 'allowed_debug_dependencies'")
            }
        }

        if let conflictingBundle = self.bundleByName[bundleName] {
            throw CompilerError("Found multiple module named '\(bundleName)': at \(bundleURL.path) and \(conflictingBundle.baseDir.path)")
        }

        self.bundleByName[bundleName] = bundleInfo
        self.bundleNameByDirURL[resolvedBundleDir] = bundleName

        return bundleInfo
    }

    private func resolveDependencies(ofModule moduleName: String, moduleURL: URL, dependencyStrings: [String]) throws -> [CompilationItem.BundleInfo] {
        return try dependencyStrings.map {
            do {
                return try getBundleInfo(bundleName: getDependencyModuleName(dependencyString: $0))
            } catch let error as CircularDependencyError {
                throw CircularDependencyError(moduleChain: [moduleName] + error.moduleChain)
            } catch let error {
                throw CompilerError("Unable to resolve dependency '\($0)' of module '\(moduleName)' at \(moduleURL.relativePath(from: projectConfig.baseDir)): \(error.legibleLocalizedDescription)")
            }
        }
    }

    private func getDependencyModuleName(dependencyString: String) -> String {
        // Dependency strings read from the 'dependencies' section of the module.yaml from the corresponding module
        let dependencyComponents = dependencyString.split(separator: "/")
        if (dependencyComponents.count >= 2) {
            let scope = dependencyComponents[dependencyComponents.count - 2]
            let moduleName = dependencyComponents[dependencyComponents.count - 1]

            return String(moduleName + scope)
        }

        return dependencyString
    }

    private func getBundleInfo(bundleName: String) throws -> CompilationItem.BundleInfo {
        if let bundleInfo = bundleByName[bundleName] {
            // Cached Bundle if bundleName was resolved previously
            return bundleInfo
        }

        let bundleURL: URL
        let bundleFile: File
        if let registeredBundle = self.bundleURLByName[bundleName] {
            // Bundle registered via explicit_input_list.json in [ValdiCompilerRunner.swift]
            bundleURL = registeredBundle.url
            bundleFile = registeredBundle.file
        } else {
            // Infer based on name from the base directory
            let resolvedBundlePath = BundleManager.resolveBundleName(bundleName: bundleName)

            // List of directories to look for modules in
            var possibleBundleURL: URL? = nil

            for urlPrefix in self.baseDirURLs.reversed() {
                let maybeBundleURL = urlPrefix.appendingPathComponent(resolvedBundlePath).appendingPathComponent(Files.moduleYaml)

                if FileManager.default.fileExists(atPath: maybeBundleURL.path) {
                    possibleBundleURL = maybeBundleURL
                    break
                }
            }

            if let possibleBundleURL = possibleBundleURL {
                bundleURL = possibleBundleURL
            } else {
                return rootBundle
            }

            bundleFile = .url(bundleURL)
        }

        return try loadBundle(bundleName: bundleName, bundleURL: bundleURL, bundleFile: bundleFile)
    }

    private func resolveBundleInfo(itemURL: URL, dirURL: URL) throws -> CompilationItem.BundleInfo {
        if let bundleName = self.bundleNameByDirURL[try dirURL.resolvingSymlink()] {
            return try getBundleInfo(bundleName: bundleName)
        }

        if baseDirsTrie.get(name: dirURL.pathComponents).nearestValue == nil {
            if baseDirsTrie.get(name: itemURL.pathComponents).nearestValue != nil {
                // If our file is containing within the provided baseDir but we couldn't resolve a module
                // for it, we use the root module as fallback.
                return rootBundle
            }

            throw CompilerError("File \(itemURL.path) is not within any provided baseDirs at \(self.baseDirURLs)")
        }

        let possibleBundleURL = dirURL.appendingPathComponent(Files.moduleYaml)
        if FileManager.default.fileExists(atPath: possibleBundleURL.path) {
            return try loadBundle(bundleName: dirURL.lastPathComponent, bundleURL: possibleBundleURL, bundleFile: .url(possibleBundleURL))
        }

        let bundleInfoFromParentDir = try resolveBundleInfo(itemURL: itemURL, dirURL: dirURL.deletingLastPathComponent())

        self.bundleNameByDirURL[dirURL] = bundleInfoFromParentDir.name

        return bundleInfoFromParentDir
    }
}
