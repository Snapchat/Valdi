import Foundation

extension Sequence where Element: Hashable {
    func uniqueElements() -> [Element] {
        var set = Set<Element>()
        return filter { set.insert($0).inserted }
    }
}

private struct ModuleBuildTargetConfig {
    let projectConfig: ValdiProjectConfig
    let name: String
    let iosModuleName: String
    let iosLanguage: IOSLanguage
    let iosGeneratedContextFactories: [String]

    let iosOutputTarget: ModuleOutputTarget?
    let androidOutputTarget: ModuleOutputTarget?

    let declarationTSSourceDirs: SourceDirTracking
    let nonDeclarationTSSourceDirs: SourceDirTracking
    let jsonSourceDirs: SourceDirTracking
    let jsSourceDirs: SourceDirTracking
    let legacyVueSourceDirs: SourceDirTracking
    let legacyStyleSourceDirs: SourceDirTracking
    let legacyModuleStyleSourceDirs: SourceDirTracking
    let resourceSourceDirs: SourceDirTracking
    let sqlSourceDirs: SourceDirTracking
    let sqlDatabaseNames: [String]?
    let protoDeclSourceDirs: SourceDirTracking

    let disableAnnotationProcessing: Bool
    let asyncStrictMode: Bool
    let stringsDir: String?
    let idsYaml: String?
    let noCompiledValdiModuleOutput: Bool
    let downloadableAssets: Bool
    let downloadableSources: Bool
    let singleFileCodegen: Bool
    let hasIOSExports: Bool
    let hasAndroidExports: Bool

    let iosDeps: [String]

    let excludePatterns: [NSRegularExpression]
    let excludeGlobs: [String]

    // Dependencies array
    let deps: [CompilationItem.BundleInfo]
}

private struct GlobPatterns {
    struct GlobContent {
        let srcs: [String]
        let exclude: [String]
        let files: [String]

        func toGlobCallString() -> String {
            return ""
        }
    }

    private var fileExtensions: Set<String> = []
    private var filenames: [String] = []

    init(fileExtensions: [String] = [], filenames: [String] = []) {
        self.fileExtensions = Set<String>(fileExtensions)
        self.filenames = filenames
    }

    mutating func append(_ fileExtension: String) {
        self.fileExtensions.insert(fileExtension)
    }

    mutating func append(fileExtensions newFileExtensions: [String]) {
        self.fileExtensions = self.fileExtensions.union(newFileExtensions)
    }

    mutating func append(filename: String) {
        self.filenames.append(filename)
    }

    func getGlobCallString(sourceDirectories: Set<String>, files: Set<String>, exclude: [String], separator: String = ", ") -> String {
        var sortedExtensions = Array(self.fileExtensions)
        sortedExtensions.sort()

        let globSrcs = sourceDirectories
            .sorted()
            .map { sourceDir in
                sortedExtensions.map { "\"\(sourceDir)/**/*.\($0)\"" } + filenames.map { "\"\(sourceDir)/**/\($0)\"" }
            }
            .flatMap { $0 }
            .joined(separator: separator)

        var globCall = "glob([\n\(globSrcs)]"

        if !exclude.isEmpty {
            let excludeString = exclude.joined(separator: separator)
            globCall += "\n,    exclude = [\(excludeString)]"
        }

        globCall += ")"

        if !files.isEmpty {
            let filesString = files
                .sorted()
                .map { file in "\"\(file)\"" }
                .joined(separator: separator)

            globCall += " + [\n\(filesString)\n]"
        }

        return globCall
    }
}

private struct ModuleBuildFile {
    let config: ModuleBuildTargetConfig

    func isExcludePatternsAndGlobsValid() -> Bool {
        // Validates that the number of entries in exclude_patterns and exclude_globs match
        return !config.excludeGlobs.isEmpty ? config.excludePatterns.count == config.excludeGlobs.count : true
    }

    func generateContents() throws -> String {
        var srcsPatterns = GlobPatterns(fileExtensions: [])

        if !config.nonDeclarationTSSourceDirs.sourceDirs.isEmpty || !config.declarationTSSourceDirs.sourceDirs.isEmpty {
            srcsPatterns.append(fileExtensions: [
                FileExtensions.typescript,
                FileExtensions.typescriptXml
            ])
        }
        if !config.jsSourceDirs.sourceDirs.isEmpty {
            srcsPatterns.append(FileExtensions.javascript)
        }
        if !config.jsonSourceDirs.sourceDirs.isEmpty {
            srcsPatterns.append(FileExtensions.json)
        }
        let allSourceDirsToCheck = [config.nonDeclarationTSSourceDirs, config.declarationTSSourceDirs, config.jsSourceDirs, config.jsonSourceDirs]
        var srcsSourceDirectories = Set<String>()
        allSourceDirsToCheck.forEach { srcsSourceDirectories.formUnion($0.sourceDirs) }
        var files = Set<String>()
        allSourceDirsToCheck.forEach { files.formUnion($0.files) }

        let excludePaths = config.excludeGlobs.uniqueElements().map{#""\#($0)""#}

        let srcGlobCallString = srcsPatterns.getGlobCallString(sourceDirectories: srcsSourceDirectories,
                                                               files: files,
                                                               exclude: excludePaths,
                                                               separator: ",\n        ")

        func outputTargetString(_ outputTarget: ModuleOutputTarget?) -> String {
            switch outputTarget {
            case .debugOnly?:
                return "\"debug\""
            case .releaseReady?:
                return "\"release\""
            case nil:
                return "None"
            }
        }

        let iosOutputTargetString = outputTargetString(config.iosOutputTarget)
        let androidOutputTargetString = outputTargetString(config.androidOutputTarget)

        var contents = "load(\"@valdi//bzl/valdi:valdi_module.bzl\", \"valdi_module\")\n"
        if !config.sqlSourceDirs.isEmpty {
            contents.append(
            """
            load("@rules_pkg//:mappings.bzl", "pkg_files", "strip_prefix")\n
            load("@rules_pkg//:pkg.bzl", "pkg_zip")\n
            """)
        }

        if (!isExcludePatternsAndGlobsValid()) {
            throw CompilerError("Module \'\(config.name)\' has mismatched number of entries for exclude_patterns (\(config.excludePatterns.count)) and exclude_globs (\(config.excludeGlobs.count)) in 'module.yaml'.")
        }

        contents.append(
        """

        ####################
        ## THIS IS AN AUTOGENERATED FILE
        ##
        ## Regenerate using ./scripts/regenerate_valdi_modules_build_bazel_files.sh
        ####################

        valdi_module(
            name = "\(config.name)",
            ios_module_name = "\(config.iosModuleName)",
            ios_output_target = \(iosOutputTargetString),
            android_output_target = \(androidOutputTargetString),
            srcs = \(srcGlobCallString),
            module_yaml = "module.yaml",
            single_file_codegen = \(config.singleFileCodegen ? "True" : "False"),
        """)

        if !config.hasIOSExports || !config.hasAndroidExports {
            contents.append("""
            has_ios_exports = \(config.hasIOSExports ? "True" : "False"),
            has_android_exports = \(config.hasAndroidExports ? "True" : "False"),
        """)
        }

        func maybeAppendDepsSequence(_ sequence: [String], attributeName: String) {
            if !sequence.isEmpty {
                let sequenceString = sequence.joined(separator: ",\n        ")
                contents.append("""
                \n    \(attributeName) = [\n        \(sequenceString)\n    ],
                """)
            }
        }

        var combinedDeps: [String] = []

        for dep in config.deps {
            let bazelTarget = try dep.toBazelTarget(projectConfig: self.config.projectConfig, currentWorkspace: "snap_client")
            combinedDeps.append("\"\(bazelTarget)\"")
        }

        maybeAppendDepsSequence(combinedDeps.sorted(), attributeName: "deps")

        func maybeAppendAttributeGlobPatternsSequence(fileExtensions: [String], filenames: [String] = [], sourceDirs: SourceDirTracking, attributeName: String) {
            guard !sourceDirs.isEmpty else {
                return
            }
            let globCall = GlobPatterns(fileExtensions: fileExtensions, filenames: filenames)
                .getGlobCallString(sourceDirectories: sourceDirs.sourceDirs, files: [], exclude: [], separator: ",\n        ")
            contents.append("""
            \n    \(attributeName) = \(globCall),
            """)
        }

        maybeAppendAttributeGlobPatternsSequence(fileExtensions: FileExtensions.images + FileExtensions.exportedImages + FileExtensions.fonts,
                                                 sourceDirs: config.resourceSourceDirs,
                                                 attributeName: "res")

        maybeAppendAttributeGlobPatternsSequence(fileExtensions: FileExtensions.legacyStyles,
                                                 sourceDirs: config.legacyStyleSourceDirs,
                                                 attributeName: "legacy_style_srcs")

        maybeAppendAttributeGlobPatternsSequence(fileExtensions: FileExtensions.legacyModuleStyles,
                                                 sourceDirs: config.legacyModuleStyleSourceDirs,
                                                 attributeName: "legacy_module_style_srcs")

        maybeAppendAttributeGlobPatternsSequence(fileExtensions: [FileExtensions.vue],
                                                 sourceDirs: config.legacyVueSourceDirs,
                                                 attributeName: "legacy_vue_srcs")

        maybeAppendAttributeGlobPatternsSequence(fileExtensions: FileExtensions.sql,
                                                 filenames: [Files.sqlTypesYaml, Files.sqlManifestYaml],
                                                 sourceDirs: config.sqlSourceDirs,
                                                 attributeName: "sql_srcs")

        maybeAppendAttributeGlobPatternsSequence(fileExtensions: [FileExtensions.protoDecl],
                                                 sourceDirs: config.protoDeclSourceDirs,
                                                 attributeName: "protodecl_srcs")

        if let sqlDatabaseNames = config.sqlDatabaseNames {
            let outputSqlDatabaseNames = sqlDatabaseNames.map { "\"\($0)\"" }.joined(separator: ", ")
            contents.append("""
            \n    sql_db_names = [\(outputSqlDatabaseNames)],
            """)
        }

        if !config.iosGeneratedContextFactories.isEmpty {
            let iosGeneratedContextFactories = config.iosGeneratedContextFactories

            let outputGeneratedContextFactoryNames = iosGeneratedContextFactories.map { "\"\($0)\"" }.joined(separator: ", ")
            contents.append("""
            \n    ios_generated_context_factories = [\(outputGeneratedContextFactoryNames)],
            """)
        }

        func maybeAppendStringAttribute(_ attributeName: String, value: String?) {
            guard let value = value else {
                return
            }

            contents.append("""
            \n    \(attributeName) = \"\(value)\",
            """)
        }

        func maybeAppendStringListAttribute(_ attributeName: String, values: [String]?) {
            guard let values = values else {
                return
            }
            let quoted = values.map { "\"\($0)\"" }.joined(separator: ", ")
            contents.append("""
            \n    \(attributeName) = [\(quoted)],
            """)
        }

        enum AppendOnlyIf {
            case valueTrue
            case valueFalse
            case always
        }

        func maybeAppendBoolAttribute(_ attributeName: String, value: Bool, appendOnlyIf: AppendOnlyIf) {
            switch appendOnlyIf {
            case .valueTrue:
                if !value {
                    return
                }
            case .valueFalse:
                if value {
                    return
                }
            case .always:
                break
            }

            contents.append("""
            \n    \(attributeName) = \(value ? "True" : "False"),
            """)
        }

        maybeAppendStringAttribute("strings_dir", value: config.stringsDir)
        maybeAppendStringAttribute("ids_yaml", value: config.idsYaml)

        switch config.iosLanguage {
        case .objc: break // default is "objc", no need to add anything
        case .swift: maybeAppendStringAttribute("ios_language", value: "swift")
        case .both: maybeAppendStringListAttribute("ios_language", values: ["objc", "swift"])
        }

        maybeAppendBoolAttribute("disable_annotation_processing", value: config.disableAnnotationProcessing, appendOnlyIf: .valueTrue)

        maybeAppendBoolAttribute("async_strict_mode", value: config.asyncStrictMode, appendOnlyIf: .valueTrue)

        maybeAppendBoolAttribute("downloadable_assets", value: config.downloadableAssets, appendOnlyIf: .valueFalse)

        maybeAppendBoolAttribute("no_compiled_valdimodule_output", value: config.noCompiledValdiModuleOutput, appendOnlyIf: .valueTrue)

        contents.append("""
        \n    visibility = ["//visibility:public"],
        )\n
        """)
        if !config.sqlSourceDirs.isEmpty {
            contents.append(
            """
            pkg_files(
                name = "\(config.name)_sql_srcs",
                srcs = glob(["**/*.sq", "**/*.sqm", "**/*.yaml"]),
                strip_prefix = strip_prefix.from_pkg(),
                visibility = ["//visibility:public"],
            )

            pkg_zip(
                name = "\(config.name)_sql",
                srcs = [":\(config.name)_sql_srcs"],
                visibility = ["//visibility:public"],
                package_dir = "com/snap",
            )
            """)
        }
        return contents
    }
}

private struct SourceDirTracking {
    private let supportedSourceDirectories: Set<String>

    private let logger: ILogger
    private(set) var items: [CompilationItem] = []
    private(set) var sourceDirs: Set<String> = []
    private(set) var files: Set<String> = []

    init(logger: ILogger, _ supportedSourceDirectories: [String]) {
        self.logger = logger
        self.supportedSourceDirectories = Set(supportedSourceDirectories)
    }

    mutating func append(_ item: CompilationItem) {
        items.append(item)

        let pathComponents = item.relativeBundleURL.pathComponents
        let isTsConfig = item.sourceURL.lastPathComponent == "tsconfig.json"
        let isParentReference = pathComponents.first == ".."
        
        // Special handling for tsconfig.json:
        // - If it's in a parent directory (starts with ".."), treat as a file
        // - If it's at the root level (single component), treat as a file
        // - If it's nested in a source directory (e.g., src/lib/tsconfig.json), treat as part of that source dir
        if pathComponents.count > 1 && !(isTsConfig && isParentReference) {
            let sourceDir = pathComponents[0]
            if !supportedSourceDirectories.contains(sourceDir) {
                logger.warn("!!! Unusual source directory '\(sourceDir)' for item \(item.relativeProjectPath)")
            }
            sourceDirs.insert(sourceDir)
        } else {
            // must be root .d.ts files:
            //    StringMap.d.ts
            //    NativeTemplateElements.d.ts
            //    MapStrings.d.ts
            //    Drawing.d.ts
            //    StringSet.d.ts
            //    Strings.d.ts
            //    Valdi.ignore.ts
            //    Networking.d.ts
            // OR tsconfig.json that's either at root level or in parent directory (../tsconfig.json)
            if item.bundleInfo.isRoot {
                logger.warn("Unusual source directory '<root>' for item \(item.relativeProjectPath)")
            } else {
                files.insert(item.relativeBundleURL.path)
            }
        }
    }

    var isEmpty: Bool {
        return sourceDirs.isEmpty && files.isEmpty
    }
}

final class GenerateModuleBuildFileProcessor: CompilationProcessor {

    var description: String {
        return "Re-generating valdi_modules BUILD files"
    }

    private let logger: ILogger
    private let projectConfig: ValdiProjectConfig
    private let compilerConfig: CompilerConfig
    private let nativeCodeGenerationManager: NativeCodeGenerationManager?

    init(logger: ILogger, projectConfig: ValdiProjectConfig, compilerConfig: CompilerConfig, nativeCodeGenerationManager: NativeCodeGenerationManager? = nil) {
        self.logger = logger
        self.projectConfig = projectConfig
        self.compilerConfig = compilerConfig
        self.nativeCodeGenerationManager = nativeCodeGenerationManager
    }

    private func hasNonTSConfigJsonFile(jsonSourceDirs: SourceDirTracking) -> Bool {
        for file in jsonSourceDirs.items {
            if file.sourceURL.lastPathComponent != "tsconfig.json" {
                return true
            }
        }

        return false
    }

    private func generateModuleBuildFile(moduleName: String, bundleInfo: CompilationItem.BundleInfo, items: [CompilationItem]) throws -> ModuleBuildFile {
        let supportedSrcDirectories = ["src", "test"]
        var declarationTSSourceDirs = SourceDirTracking(logger: logger, supportedSrcDirectories)
        var nonDeclarationTSSourceDirs = SourceDirTracking(logger: logger, supportedSrcDirectories)
        var jsonSourceDirs = SourceDirTracking(logger: logger, supportedSrcDirectories)
        var jsSourceDirs = SourceDirTracking(logger: logger, supportedSrcDirectories)
        var legacyVueSourceDirs = SourceDirTracking(logger: logger, supportedSrcDirectories)
        var legacyStyleSourceDirs = SourceDirTracking(logger: logger, supportedSrcDirectories)
        var legacyModuleStyleSourceDirs = SourceDirTracking(logger: logger, supportedSrcDirectories)
        let supportedImageResourceDirectories = ["res"]
        var resourceSourceDirs = SourceDirTracking(logger: logger, supportedImageResourceDirectories)
        let supportedSqlDirectories = ["sql", "sqlite"]
        var sqlSourceDirs = SourceDirTracking(logger: logger, supportedSqlDirectories)
        var sqlItems: [CompilationItem] = []
        var protoDeclSourceDirs = SourceDirTracking(logger: logger, ["src", "test"])
        var idsYaml: String? = nil

        for item in items {
            let isTypescriptDeclarationSource = item.sourceURL.lastPathComponent.hasSuffix(FileExtensions.typescriptDeclaration)

            if isTypescriptDeclarationSource {
                declarationTSSourceDirs.append(item)
            } else if [FileExtensions.typescriptDeclaration, FileExtensions.typescript, FileExtensions.typescriptXml].contains(item.sourceURL.pathExtension) {
                nonDeclarationTSSourceDirs.append(item)
            }
            if item.sourceURL.pathExtension == FileExtensions.vue {
                legacyVueSourceDirs.append(item)
            }
            if FileExtensions.fonts.contains(item.sourceURL.pathExtension) {
                resourceSourceDirs.append(item)
            }
            if FileExtensions.legacyStyles.contains(item.sourceURL.pathExtension) {
                legacyStyleSourceDirs.append(item)
            }

            let isLegacyModuleStyleSource = FileExtensions.legacyModuleStyles.contains { item.sourceURL.lastPathComponent.hasSuffix($0) }
            if isLegacyModuleStyleSource {
                legacyModuleStyleSourceDirs.append(item)
            }
            if item.sourceURL.pathExtension == FileExtensions.javascript {
                jsSourceDirs.append(item)
            }
            if FileExtensions.images.contains(item.sourceURL.pathExtension) {
                resourceSourceDirs.append(item)
            }
            if Files.stringsJSON == item.sourceURL.lastPathComponent {
                // FIXME: implement strings.json detection here to validate a situation where it's present, but strings_dir is not configured?
            }
            if FileExtensions.sql.contains(item.sourceURL.pathExtension) || Files.sqlTypesYaml == item.sourceURL.lastPathComponent {
                if item.relativeBundleURL.path.hasPrefix("sql") {
                    sqlSourceDirs.append(item)
                    sqlItems.append(item)
                } else {
                    // We have cases with .sql/.sq files that are under src/ instead of sql/
                    // Those should _not_ be processed
                }
            }
            if item.sourceURL.pathExtension == FileExtensions.protoDecl {
                protoDeclSourceDirs.append(item)
            }

            if case .ids(_) = item.kind {
                // We currently only support one ids.yaml file within a valdi module
                idsYaml = item.relativeBundleURL.path
            } else if case .resourceDocument = item.kind {
                if item.sourceURL.pathExtension == FileExtensions.json {
                    jsonSourceDirs.append(item)
               }
            }
        }

        let noCompiledValdiModuleOutput = [
            // intentionally excluding declarationTSSourceDirs, since those do not produce output
            nonDeclarationTSSourceDirs,
            jsSourceDirs,
            legacyVueSourceDirs,
            legacyStyleSourceDirs,
            legacyModuleStyleSourceDirs,
            resourceSourceDirs,
            sqlSourceDirs,
            protoDeclSourceDirs
        ].allSatisfy(\.isEmpty) && !hasNonTSConfigJsonFile(jsonSourceDirs: jsonSourceDirs)

        let sqlDatabaseNames: [String]?
        if !sqlItems.isEmpty {
            // See database naming logic from ClientSqlProcessor.runSqlDelight...
            // This seems very fragile
            let sqlItemPathComponents: [[String]] = sqlItems
                .compactMap { item in
                    let components = item.relativeBundleURL.path.components(separatedBy: "/")
                    return components
                }
                .filter { $0.last != "sql_types.yaml" }
                .filter { $0[safe: $0.count-2] != "migration" }
            let dbPathComponents = sqlItemPathComponents
                .map { $0.dropFirst().first! }

            let uniquedDbPathComponents = Set(dbPathComponents).sorted()

            if uniquedDbPathComponents.isEmpty {
                throw CompilerError("Could not detect the sql database name for module '\(moduleName)' items: \(sqlItems.map(\.relativeProjectPath))")
            }
            sqlDatabaseNames = uniquedDbPathComponents
        } else {
            sqlDatabaseNames = nil
        }

        let sortedDeps = Set(bundleInfo.dependencies)
            .filter { !$0.isRoot }
            .sorted(by: { $0.resolvedBundleName() < $1.resolvedBundleName() })

        let iosDeps = sortedDeps.map { $0.iosModuleName }

        let iosOutputTarget = bundleInfo.outputTarget(platform: .ios)
        let androidOutputTarget = bundleInfo.outputTarget(platform: .android)

        // Determine if the module has native exports
        // Default to true for safety if annotation processing hasn't run or failed
        var hasIOSExports: Bool
        var hasAndroidExports: Bool
        if let manager = nativeCodeGenerationManager {
            hasIOSExports = manager.hasIOSExports(for: bundleInfo)
            hasAndroidExports = manager.hasAndroidExports(for: bundleInfo)
        } else {
            // No manager available (shouldn't happen in normal flow, but default to safe value)
            hasIOSExports = true
            hasAndroidExports = true
        }
        
        // Also check Vue files for native exports
        let vueExports = checkVueFilesForNativeExports(legacyVueSourceDirs)
        hasIOSExports = hasIOSExports || vueExports.hasIOSExports
        hasAndroidExports = hasAndroidExports || vueExports.hasAndroidExports

        let config = ModuleBuildTargetConfig(projectConfig: self.projectConfig,
                                             name: moduleName,
                                             iosModuleName: bundleInfo.iosModuleName,
                                             iosLanguage: bundleInfo.iosLanguage,
                                             iosGeneratedContextFactories: bundleInfo.iosGeneratedContextFactories,
                                             iosOutputTarget: iosOutputTarget,
                                             androidOutputTarget: androidOutputTarget,
                                             declarationTSSourceDirs: declarationTSSourceDirs,
                                             nonDeclarationTSSourceDirs: nonDeclarationTSSourceDirs,
                                             jsonSourceDirs: jsonSourceDirs,
                                             jsSourceDirs: jsSourceDirs,
                                             legacyVueSourceDirs: legacyVueSourceDirs,
                                             legacyStyleSourceDirs: legacyStyleSourceDirs,
                                             legacyModuleStyleSourceDirs: legacyModuleStyleSourceDirs,
                                             resourceSourceDirs: resourceSourceDirs,
                                             sqlSourceDirs: sqlSourceDirs,
                                             sqlDatabaseNames: sqlDatabaseNames,
                                             protoDeclSourceDirs: protoDeclSourceDirs,
                                             disableAnnotationProcessing: bundleInfo.disableAnnotationProcessing,
                                             asyncStrictMode: bundleInfo.asyncStrictMode,
                                             stringsDir: bundleInfo.stringsConfig?.bundleRelativePath,
                                             idsYaml: idsYaml,
                                             noCompiledValdiModuleOutput: noCompiledValdiModuleOutput,
                                             downloadableAssets: bundleInfo.downloadableAssets,
                                             downloadableSources: bundleInfo.downloadableSources,
                                             singleFileCodegen: bundleInfo.singleFileCodegen,
                                             hasIOSExports: hasIOSExports,
                                             hasAndroidExports: hasAndroidExports,
                                             iosDeps: iosDeps,
                                             excludePatterns: bundleInfo.inclusionConfig.excludePatterns,
                                             excludeGlobs: bundleInfo.excludeGlobs,
                                             deps: sortedDeps)
        let moduleBuildFile = ModuleBuildFile(config: config)
        return moduleBuildFile
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        let itemsByModuleName = items.allItems.groupBy { item in
            return item.bundleInfo.name
        }

        var generatedModuleBuildFiles: [String: ModuleBuildFile] = [:]
        let baseDir = projectConfig.baseDir

        for (moduleName, items) in itemsByModuleName {
            guard let bundleInfo = items.first?.bundleInfo else {
                // technically unreachable
                throw CompilerError("Each module must have at least one item")
            }

            if !bundleInfo.disableBazelBuildFileGeneration && !baseDir.relativePath(to: bundleInfo.baseDir).hasPrefix("../") {
                let generatedModuleBuildFile = try generateModuleBuildFile(moduleName: moduleName, bundleInfo: bundleInfo, items: items)
                generatedModuleBuildFiles[moduleName] = generatedModuleBuildFile
            }
        }


        return try items.select { (item) -> ModuleBuildFile? in
            guard case .moduleYaml = item.kind, let moduleBuildFile = generatedModuleBuildFiles[item.bundleInfo.name] else { return nil }
            return moduleBuildFile
        }.transformEach { (selectedItem: SelectedItem<ModuleBuildFile>) -> [CompilationItem] in
            let generatedFile = selectedItem.data
            let moduleName = selectedItem.item.bundleInfo.name

            let contents = try generatedFile.generateContents()
            let file = File.data(try contents.utf8Data())

            let finalFile = FinalFile(outputURL: baseDir.appendingPathComponent(moduleName).appendingPathComponent("BUILD.bazel"),
                                      file: file,
                                      platform: nil,
                                      kind: .unknown)
            let buildFileItem = selectedItem.item.with(newKind: .finalFile(finalFile))
            return [selectedItem.item, buildFileItem]
        }
    }
    
    /// Scans Vue files for native exports that can be defined via:
    /// 1. <class-mapping> sections with ios= or android= attributes
    /// 2. Standard Typescript export annotations (from ValdiAnnotationType.nativeExportAnnotationNames)
    private func checkVueFilesForNativeExports(_ vueSourceDirs: SourceDirTracking) -> (hasIOSExports: Bool, hasAndroidExports: Bool) {
        var hasIOSExports = false
        var hasAndroidExports = false
        
        // Regex patterns to detect iOS and Android exports in Vue files
        // Pattern 1: <class-mapping> sections with ios= or android= attributes
        // Matches: <SomeView ios="SCClassName" android="com.package.ClassName"/>
        let classMappingIosPattern = try? NSRegularExpression(pattern: #"\bios\s*=\s*["\'][^"\']+["\']"#, options: [])
        let classMappingAndroidPattern = try? NSRegularExpression(pattern: #"\bandroid\s*=\s*["\'][^"\']+["\']"#, options: [])
        
        // Pattern 2: TypeScript annotations with ios/android parameters
        // Build regex pattern dynamically from ValdiAnnotationType.nativeExportAnnotationNames
        // This ensures that if new annotation types are added, they will be automatically included
        let annotationNames = ValdiAnnotationType.nativeExportAnnotationNames.joined(separator: "|")
        let annotationIosPattern = try? NSRegularExpression(pattern: "@(?:\(annotationNames))\\s*\\([^)]*\\bios\\s*:\\s*['\"][^'\"]+['\"]", options: [])
        let annotationAndroidPattern = try? NSRegularExpression(pattern: "@(?:\(annotationNames))\\s*\\([^)]*\\bandroid\\s*:\\s*['\"][^'\"]+['\"]", options: [])
        
        for item in vueSourceDirs.items {
            guard item.sourceURL.pathExtension == FileExtensions.vue else { continue }
            
            // Read the Vue file content
            guard let content = try? String(contentsOf: item.sourceURL, encoding: .utf8) else { continue }
            
            let range = NSRange(content.startIndex..<content.endIndex, in: content)
            
            // Check for iOS exports (class-mapping or annotations)
            if !hasIOSExports {
                if let pattern = classMappingIosPattern, pattern.firstMatch(in: content, options: [], range: range) != nil {
                    hasIOSExports = true
                } else if let pattern = annotationIosPattern, pattern.firstMatch(in: content, options: [], range: range) != nil {
                    hasIOSExports = true
                }
            }
            
            // Check for Android exports (class-mapping or annotations)
            if !hasAndroidExports {
                if let pattern = classMappingAndroidPattern, pattern.firstMatch(in: content, options: [], range: range) != nil {
                    hasAndroidExports = true
                } else if let pattern = annotationAndroidPattern, pattern.firstMatch(in: content, options: [], range: range) != nil {
                    hasAndroidExports = true
                }
            }
            
            // Early exit if both exports are found
            if hasIOSExports && hasAndroidExports {
                break
            }
        }
        
        return (hasIOSExports, hasAndroidExports)
    }

}