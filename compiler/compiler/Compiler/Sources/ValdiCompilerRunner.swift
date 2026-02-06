//
//  ValdiCompilerRunner.swift
//
//
//  Created by Simon Corsin on 12/16/20.
//

import Foundation

struct ValdiPipelineBuilder {

    var preprocessors: [CompilationProcessor] = []
    var processors: [CompilationProcessor] = []
    var postprocessors: [CompilationProcessor] = []

    private let logger: ILogger

    init(logger: ILogger) {
        self.logger = logger
    }

    mutating func append(preprocessor: CompilationProcessor) {
        preprocessors.append(preprocessor)
        onAppend(preprocessor)
    }

    mutating func append(processor: CompilationProcessor) {
        processors.append(processor)
        onAppend(processor)
    }

    mutating func append(postprocessor: CompilationProcessor) {
        postprocessors.append(postprocessor)
        onAppend(postprocessor)
    }

    func build() -> [CompilationProcessor] {
        return preprocessors + processors + postprocessors
    }

    private func onAppend(_ processor: CompilationProcessor) {
        logger.trace("Appending pipeline processor '\(processor.description)'")
    }

}

class ValdiCompilerRunner {

    private let logger: ILogger
    private let arguments: ValdiCompilerArguments
    private let deferredWarningCollector: DeferredWarningCollector
    private let errorDumper: CompilationPipelineErrorDumper
    private let companionExecutableProvider: CompanionExecutableProvider
    private let workingDirectoryPath: String
    private let runningAsWorker: Bool

    init(logger: ILogger, 
         arguments: ValdiCompilerArguments,
         companionExecutableProvider: CompanionExecutableProvider,
         workingDirectoryPath: String,
         runningAsWorker: Bool) {
        self.logger = logger
        self.arguments = arguments
        self.deferredWarningCollector = DeferredWarningCollector(logger: logger)
        self.errorDumper = CompilationPipelineErrorDumper(deferredWarningCollector: self.deferredWarningCollector)
        self.companionExecutableProvider = companionExecutableProvider
        self.workingDirectoryPath = workingDirectoryPath
        self.runningAsWorker = runningAsWorker
    }

    func run() -> Bool {
        if let parsedLogLevel = arguments.parsedLogLevel {
            logger.minLevel = parsedLogLevel
        }

        var killOnTimeout: KillOnTimeout?

        if !runningAsWorker {
            Concurrency.enableConcurrency = !self.arguments.disableConcurrency

            if let taskTimeout = self.arguments.taskTimeout {
                LockConfig.waitTimeoutSeconds = TimeInterval(taskTimeout)
            }
            if let timeout = arguments.timeout {
                killOnTimeout = KillOnTimeout(logger: logger, timeoutSeconds: TimeInterval(timeout))
                killOnTimeout?.start()
            }
        }

        defer {
            killOnTimeout?.stop()
        }

        do {
            logger.duplicateStderrToStdout = self.arguments.duplicateStderrToStdout

            let baseUrl = URL(fileURLWithPath: self.workingDirectoryPath).standardizedFileURL
            let fileManager = try ValdiFileManager(bazel: self.arguments.bazel)

            if !self.arguments.buildModule.isEmpty {
                try ValdiModuleUtils.buildModule(logger: logger, fileManager: fileManager, paths: self.arguments.buildModule, baseUrl: baseUrl, out: self.arguments.out)
                return true
            } else if !self.arguments.unpackModule.isEmpty {
                try ValdiModuleUtils.unpackModule(logger: logger, fileManager: fileManager, paths: self.arguments.unpackModule, baseUrl: baseUrl, out: self.arguments.out)
                return true
            } else if !self.arguments.textconvModule.isEmpty {
                try ValdiModuleUtils.textconvModule(logger: logger, fileManager: fileManager, paths: self.arguments.textconvModule, baseUrl: baseUrl, out: self.arguments.out)
                return true
            } else if !self.arguments.uploadModule.isEmpty {
                let configs = try ResolvedConfigs.from(logger: logger, baseURL: baseUrl, userConfigURL: URL.valdiUserConfigURL, args: self.arguments)
                let diskCacheProvider = DiskCacheProvider(logger: logger, fileManager: fileManager, projectConfig: configs.projectConfig, disableDiskCache: configs.compilerConfig.disableDiskCache)
                let compilerCompanion = try getCompanionExecutable(configs: configs, diskCacheProvider: diskCacheProvider)

                try ArtifactUploader.uploadFiles(fileManager: fileManager,
                                                 paths: self.arguments.uploadModule,
                                                 baseURL: baseUrl,
                                                 out: self.arguments.out,
                                                 moduleBaseUploadURL: self.arguments.uploadBaseUrl.flatMap(URL.init(string:)),
                                                 companionExectuable: compilerCompanion)
                return true
            } else if self.arguments.dumpModulesInfo {
                let configs = try ResolvedConfigs.from(logger: logger, baseURL: baseUrl, userConfigURL: URL.valdiUserConfigURL, args: self.arguments)
                try DumpModulesInfo.dump(fileManager: fileManager, configs: configs, to: self.arguments.out!)
                return true
            } else if self.arguments.genStaticRes {
                try StaticResGenerator.generate(baseUrl: baseUrl, inputFiles: self.arguments.input, to: self.arguments.out!)
                return true
            } else if self.arguments.out != nil {
                throw CompilerError("Specifying --out only makes sense if you're using one of the utility commands: --build OR --build-module OR --unpack-module OR --upload-module")
            }

            let configs = try ResolvedConfigs.from(logger: logger, baseURL: baseUrl, userConfigURL: URL.valdiUserConfigURL, args: self.arguments)
            let diskCacheProvider = DiskCacheProvider(logger: logger, fileManager: fileManager, projectConfig: configs.projectConfig, disableDiskCache: configs.compilerConfig.disableDiskCache)
            let compilerCompanion = try getCompanionExecutable(configs: configs, diskCacheProvider: diskCacheProvider)

            let hotReloadingEnabled = configs.compilerConfig.hotReloadingEnabled
            let baseDirURLs: [URL]
            if hotReloadingEnabled {
                if let explicitInputList = configs.compilerConfig.explicitInputList {
                    var resolvedBaseDirs: [URL] = []
                    var uniqueModuleDirPaths = Set<URL>()
                    logger.debug("Registering unique module directory paths for watchman")
                    for entry in explicitInputList.entries {
                        guard let monitor = entry.monitor, monitor else { continue }

                        let moduleDirURL = baseUrl.resolving(path: entry.modulePath, isDirectory: true).standardizedFileURL.absoluteURL
                        if !uniqueModuleDirPaths.contains(moduleDirURL) {
                            uniqueModuleDirPaths.insert(moduleDirURL)
                            resolvedBaseDirs.append(moduleDirURL)
                        }
                    }
                    baseDirURLs = resolvedBaseDirs
                } else {
                    baseDirURLs = [configs.projectConfig.baseDir, configs.projectConfig.nodeModulesDir, configs.projectConfig.openSourceDir].compactMap { $0 }
                }
            } else {
                baseDirURLs = [configs.projectConfig.baseDir, configs.projectConfig.openSourceDir].compactMap { $0 }
            }

            let bundleManager = try BundleManager(projectConfig: configs.projectConfig, compilerConfig: configs.compilerConfig, baseDirURLs: baseDirURLs)

            // Register known modules if they are provided by the explicit input list
            if let explicitInputList = configs.compilerConfig.explicitInputList {
                logger.debug("Registering known bundle from explicit input list")
                for entry in explicitInputList.entries {
                    let moduleURL = baseUrl.resolving(path: entry.modulePath).appendingPathComponent(Files.moduleYaml).standardizedFileURL.absoluteURL
                    let moduleFile: File
                    if let moduleContent = entry.moduleContent {
                        moduleFile = .string(moduleContent)
                    } else if let moduleFileEntry = entry.files.first(where: { $0.file.lastPathComponent() == Files.moduleYaml }) {
                        let fileURL = baseUrl.resolving(path: moduleFileEntry.file).standardizedFileURL.absoluteURL
                        moduleFile = .url(fileURL)
                    } else {
                        moduleFile = .url(moduleURL)
                    }
                    bundleManager.registerBundle(forName: entry.moduleName, bundleURL: moduleURL, file: moduleFile)
                }
            }

            var resolvedModulesFilter: ModulesFilter?
            if !self.arguments.module.isEmpty {
                resolvedModulesFilter = try ModulesFilter(bundleManager: bundleManager, modulesFilter: self.arguments.module, ignoreModuleDeps: self.arguments.ignoreModuleDeps)
            }

            var daemonService: DaemonService?
            if hotReloadingEnabled {
                daemonService = try DaemonService(logger: logger,
                                                  fileManager: fileManager,
                                                  userConfig: configs.userConfig,
                                                  projectConfig: configs.projectConfig,
                                                  compilerConfig: configs.compilerConfig,
                                                  companion: compilerCompanion,
                                                  reloadOverUSB: self.arguments.usb)
            }

            let dumpComponentURLs = self.arguments.dumpComponents.map { baseUrl.resolving(path: $0) }

            let fileDependenciesManager = FileDependenciesManager()
            logger.debug("Configuring compilation pipeline")

            let pipeline = try buildPipeline(
                configs: configs,
                fileManager: fileManager,
                diskCacheProvider: diskCacheProvider,
                deferredWarningCollector: self.deferredWarningCollector,
                compilerCompanion: compilerCompanion,
                daemonService: daemonService,
                fileDependenciesManager: fileDependenciesManager,
                maxUploadConcurrentRequests: self.arguments.maxUploadConcurrentRequests,
                codeGenOnly: self.arguments.codegenOnly,
                forceMinify: self.arguments.forceMinify,
                disableMinifyWeb: self.arguments.disableMinifyWeb,
                emitInlineTranslations: self.arguments.inlineTranslations,
                emitInlineAssets: self.arguments.inlineAssets,
                modulesFilter: resolvedModulesFilter,
                imageVariantsFilter: self.arguments.parsedImageVariantsFilter,
                bundleManager: bundleManager,
                rootBundle: bundleManager.rootBundle,
                dumpComponentURLs: dumpComponentURLs,
                codeCoverage: self.arguments.codeCoverage,
                bazel: self.arguments.bazel
            )
            logger.info("Configured the compilation pipeline")

            let compiler = ValdiCompiler(logger: logger, pipeline: pipeline, projectConfig: configs.projectConfig, bundleManager: bundleManager)

            if hotReloadingEnabled {
                logger.info("Hot reloading enabled - starting the AutoRecompiler")
                let filesFinder = ValdiFilesFinder(urls: baseDirURLs)
                let autoRecompiler = AutoRecompiler(logger: logger,
                                                    compiler: compiler,
                                                    filesFinder: filesFinder,
                                                    bundleManager: bundleManager,
                                                    fileDependenciesManager: fileDependenciesManager,
                                                    errorDumper: self.errorDumper)

                if let explicitInputList = configs.compilerConfig.explicitInputList {
                    try compiler.addFiles(fromInputList: explicitInputList, baseUrl: baseUrl)
                }

                try autoRecompiler.start()
                RunLoop.main.run()
                autoRecompiler.stop()
            } else {
                logger.info("Hot reloading disabled - finding all files")
                // Read all the files from the given list
                if let explicitInputList = configs.compilerConfig.explicitInputList {
                    try compiler.addFiles(fromInputList: explicitInputList, baseUrl: baseUrl)
                } else {
                    // Finding all the files from the given directories
                    let filesFinder = ValdiFilesFinder(urls: baseDirURLs)
                    let fileUrls = try filesFinder.allFiles()
                    // Adding them to the compiler
                    fileUrls.forEach { compiler.addFile(file: $0) }
                }
                logger.info("Hot reloading disabled - starting compilation")
                try compiler.compile()
            }

            if arguments.bazel && logger.emittedLogsCount > 0 {
                // With bazel, we don't want to make the process succeeds if we emitted logs
                // as it will thrown off the cache
                logger.error("Failing process as logs were emitted under a Bazel build")
                logger.error("Logs in Bazel will thrash the cache and must be avoided except for fatal errors")
                return true
                //return false
            }

            return true
        } catch let error {
            self.errorDumper.dump(logger: logger, error: error)
            return false
        }
    }

    private func buildPipeline(configs: ResolvedConfigs,
                               fileManager: ValdiFileManager,
                               diskCacheProvider: DiskCacheProvider,
                               deferredWarningCollector: DeferredWarningCollector,
                               compilerCompanion: CompanionExecutable,
                               daemonService: DaemonService?,
                               fileDependenciesManager: FileDependenciesManager,
                               maxUploadConcurrentRequests: Int,
                               codeGenOnly: Bool,
                               forceMinify: Bool,
                               disableMinifyWeb: Bool,
                               emitInlineTranslations: Bool,
                               emitInlineAssets: Bool,
                               modulesFilter: ModulesFilter?,
                               imageVariantsFilter: ImageVariantsFilter?,
                               bundleManager: BundleManager,
                               rootBundle: CompilationItem.BundleInfo,
                               dumpComponentURLs: [URL],
                               codeCoverage: Bool,
                               bazel: Bool) throws -> CompilationPipeline {
        var teardownCallbacks = [() -> Void]()
        let hotReloadingEnabled = configs.compilerConfig.hotReloadingEnabled
        let enablePreviewInGeneratedTSFile = configs.compilerConfig.onlyProcessResourcesForModules.isEmpty

        let toolboxExecutable = ToolboxExecutable(logger: logger, compilerToolboxURL: configs.projectConfig.compilerToolboxURL)
        let userScriptManager = UserScriptManager()
        let hasSQL = configs.projectConfig.clientSqlURL != nil

        let projectClassMappingManager = ProjectClassMappingManager(allowMappingOverride: hotReloadingEnabled)

        let typeScriptCompiler = TypeScriptCompiler(logger: logger,
                                                    companion: compilerCompanion,
                                                    projectConfig: configs.projectConfig,
                                                    fileManager: fileManager,
                                                    emitDebug: hotReloadingEnabled)
        teardownCallbacks.append {
            let _ = try? typeScriptCompiler.destroyWorkspace().waitForData()
        }
        let jsCodeInstrumentation = JSCodeInstrumentation(companion: compilerCompanion)
        let typeScriptCompilerManager = TypeScriptCompilerManager(logger: logger,
                                                                  fileManager: fileManager,
                                                                  typeScriptCompiler: typeScriptCompiler,
                                                                  projectName: configs.projectConfig.projectName,
                                                                  rootURL: configs.projectConfig.baseDir,
                                                                  hotReloadingEnabled: hotReloadingEnabled,
                                                                  bundleManager: bundleManager,
                                                                  projectConfig: configs.projectConfig,
                                                                  compilerConfig: configs.compilerConfig)

        let typeScriptAnnotationsManager = TypeScriptAnnotationsManager()

        let nativeCodeGenerationManager = NativeCodeGenerationManager(logger: logger,
                                                                      globalIosImport: configs.projectConfig.iosDefaultModuleNamePrefix,
                                                                      rootURL: configs.projectConfig.baseDir)

        let imageToolbox = ImageToolbox(toolboxExecutable: toolboxExecutable)

        var builder = ValdiPipelineBuilder(logger: logger)

        builder.append(preprocessor: FilterItemsProcessor(modulesFilter: modulesFilter, hotreloadingEnabled: hotReloadingEnabled))
        builder.append(preprocessor: IdentifyItemsProcessor(logger: logger, ignoredFiles: configs.projectConfig.ignoredFiles))

        let regenerateValdiModulesBuildFilesOnly = configs.compilerConfig.regenerateValdiModulesBuildFiles

        if regenerateValdiModulesBuildFilesOnly {
            // Even in regenerate mode, we need annotation processing to detect native exports for correct output file generation
            // However, we skip type checking since generated files (res, Strings, etc.) may not exist
            builder.append(processor: DumpTypeScriptSymbolsProcessor(logger: logger, typeScriptCompilerManager: typeScriptCompilerManager, compilerConfig: configs.compilerConfig, skipTypeChecking: true))
            builder.append(processor: ParseTypeScriptAnnotationsProcessor(logger: logger,
                                                                          projectClassMappingManager: projectClassMappingManager,
                                                                          typeScriptCompilerManager: typeScriptCompilerManager,
                                                                          annotationsManager: typeScriptAnnotationsManager))
            builder.append(processor: ApplyTypeScriptAnnotationsProcessor(logger: logger,
                                                                          typeScriptCompilerManager: typeScriptCompilerManager,
                                                                          typeScriptAnnotationsManager: typeScriptAnnotationsManager,
                                                                          nativeCodeGenerationManager: nativeCodeGenerationManager))
            builder.append(processor: GenerateModuleBuildFileProcessor(logger: logger, 
                                                                       projectConfig: configs.projectConfig,
                                                                       compilerConfig: configs.compilerConfig,
                                                                       nativeCodeGenerationManager: nativeCodeGenerationManager))
            builder.append(preprocessor: GenerateGlobalMetadataProcessor(logger: logger,
                                                                         projectConfig: configs.projectConfig,
                                                                         rootBundle: rootBundle,
                                                                         shouldMergeWithExistingFile: false))
        } else {
            builder.append(preprocessor: try IdentifyImageAssetsProcessor(logger: logger,  imageToolbox: imageToolbox, compilerConfig: configs.compilerConfig, diskCacheProvider: diskCacheProvider))
            builder.append(preprocessor: IdentifyFontAssetsProcessor())
            builder.append(preprocessor: GenerateAssetCatalogProcessor(logger: logger, fileManager: fileManager, projectConfig: configs.projectConfig, enablePreviewInGeneratedTSFile: enablePreviewInGeneratedTSFile))
            builder.append(preprocessor: TranslationStringsProcessor(logger: logger, fileManager: fileManager, compilerConfig: configs.compilerConfig, projectConfig: configs.projectConfig, emitInlineTranslations: emitInlineTranslations, companion: compilerCompanion))
            builder.append(preprocessor: GenerateIdsFilesProcessor(logger: logger, fileManager: fileManager, projectConfig: configs.projectConfig, companion: compilerCompanion, nativeCodeGenerationManager: nativeCodeGenerationManager))
            if hasSQL {
                builder.append(preprocessor: try ClientSqlProcessor(
                    logger: logger,
                    fileManager: fileManager,
                    diskCacheProvider: diskCacheProvider,
                    projectConfig: configs.projectConfig,
                    rootBundle: rootBundle
                ))
                builder.append(preprocessor: ClientSqlExportProcessor(
                    logger: logger,
                    fileManager: fileManager,
                    projectConfig: configs.projectConfig,
                    rootBundle: rootBundle
                ))
            }

            if !configs.compilerConfig.generateTSResFiles {
                if configs.projectConfig.iosBuildFileConfig != nil || configs.projectConfig.androidBuildFileConfig != nil
                    || configs.projectConfig.webBuildFileConfig != nil{
                    let shouldMergeWithExistingFile = modulesFilter != nil
                    builder.append(preprocessor: GenerateGlobalMetadataProcessor(logger: logger,
                                                                                 projectConfig: configs.projectConfig,
                                                                                 rootBundle: rootBundle,
                                                                                 shouldMergeWithExistingFile: shouldMergeWithExistingFile))
                    builder.append(preprocessor: GenerateBuildFileProcessor(projectConfig: configs.projectConfig))
                }
                
                builder.append(processor: ParseDocumentsProcessor(logger: logger, globalIosImportPrefix: configs.projectConfig.iosDefaultModuleNamePrefix))
                builder.append(processor: CSSModulesProcessor(logger: logger, fileManager: fileManager, projectConfig: configs.projectConfig, fileDependenciesManager: fileDependenciesManager))
                builder.append(processor: DocumentUserScriptExtractionProcessor(logger: logger, fileManager: fileManager, userScriptManager: userScriptManager, projectConfig: configs.projectConfig))
                builder.append(processor: LoadCompilationMetadataProcessor(projectClassMappingManager: projectClassMappingManager, typeScriptCompilationManager: typeScriptCompilerManager, typeScriptNativeTypeResolver: nativeCodeGenerationManager.nativeTypeResolver))
                builder.append(processor: DumpTypeScriptSymbolsProcessor(logger: logger, typeScriptCompilerManager: typeScriptCompilerManager, compilerConfig: configs.compilerConfig))
                builder.append(processor: ParseTypeScriptAnnotationsProcessor(logger: logger,
                                                                              projectClassMappingManager: projectClassMappingManager,
                                                                              typeScriptCompilerManager: typeScriptCompilerManager,
                                                                              annotationsManager: typeScriptAnnotationsManager))
                builder.append(processor: CompileDocumentsProcessor(logger: logger,
                                                                    projectClassMappingManager: projectClassMappingManager,
                                                                    userScriptManager: userScriptManager,
                                                                    fileDependenciesManager: fileDependenciesManager))
                builder.append(processor: ApplyTypeScriptAnnotationsProcessor(logger: logger,
                                                                              typeScriptCompilerManager: typeScriptCompilerManager,
                                                                              typeScriptAnnotationsManager: typeScriptAnnotationsManager,
                                                                              nativeCodeGenerationManager: nativeCodeGenerationManager))
                builder.append(processor: DumpCompilationMetadataProcessor(projectConfig: configs.projectConfig,
                                                                           compilerConfig: configs.compilerConfig,
                                                                           projectClassMappingManager: projectClassMappingManager,
                                                                           typeScriptCompilationManager: typeScriptCompilerManager,
                                                                           typeScriptNativeTypeResolver: nativeCodeGenerationManager.nativeTypeResolver))

                if !codeGenOnly {
                    builder.append(processor: CompileTypeScriptProcessor(typeScriptCompilerManager: typeScriptCompilerManager, compilerConfig: configs.compilerConfig))
                }

                if codeCoverage {
                    builder.append(processor: CodeCoverageProcessor(logger: logger, jsCodeInstrumentation: jsCodeInstrumentation, diskCacheProvider: diskCacheProvider, projectConfig: configs.projectConfig, typeScriptCompilerManager: typeScriptCompilerManager, fileManager: fileManager))
                }

                if !dumpComponentURLs.isEmpty {
                    builder.append(processor: DumpComponentsProcessor(outFileURLs: dumpComponentURLs))
                }

                if !codeCoverage && !codeGenOnly && (!hotReloadingEnabled || forceMinify) {
                    builder.append(processor: MinifyJsProcessor(logger: logger, diskCacheProvider: diskCacheProvider, projectConfig: configs.projectConfig, companion: compilerCompanion, disableMinifyWeb: disableMinifyWeb))
                }

                if !hotReloadingEnabled && !codeGenOnly {
                    if !codeCoverage {
                        builder.append(processor: SourceMapProcessor(compilerConfig: configs.compilerConfig, disableMinifyWeb: disableMinifyWeb))
                    }

                    if configs.compilerConfig.outputTarget.contains(.release) {
                        builder.append(processor: try JavaScriptPreCompilerProcessor(
                            logger: logger,
                            fileManager: fileManager,
                            diskCacheProvider: diskCacheProvider,
                            toolboxExecutable: toolboxExecutable,
                            androidJsBytecodeFormat: configs.projectConfig.androidJsBytecodeFormat))
                    }
                }
            }
        }

        if !configs.compilerConfig.generateTSResFiles {
            if !hotReloadingEnabled && !regenerateValdiModulesBuildFilesOnly {
                builder.append(postprocessor: GenerateViewClassesProcessor(logger: logger, compilerConfig: configs.compilerConfig))
                builder.append(postprocessor: GenerateModelsProcessor(logger: logger, compilerConfig: configs.compilerConfig))
                // GenerateDependencyInjectionDataProcessor must run BEFORE CombineNativeSourcesProcessor
                // so that Factory classes are included in the combined output for single_file_codegen modules
                builder.append(postprocessor: GenerateDependencyInjectionDataProcessor(logger: logger, onlyFocusProcessingForModules: configs.compilerConfig.onlyFocusProcessingForModules))
                builder.append(postprocessor: CombineNativeSourcesProcessor(logger: logger, compilerConfig: configs.compilerConfig, projectConfig: configs.projectConfig, bundleManager: bundleManager))
                builder.append(postprocessor: GeneratedTypesVerificationProcessor(logger: logger, projectConfig: configs.projectConfig))
            }

            if !codeGenOnly && !regenerateValdiModulesBuildFilesOnly {
                builder.append(postprocessor: InvalidDocumentsProcessor(logger: logger, generateErrorDocuments: hotReloadingEnabled, compilationPipeline: builder.processors, projectConfig: configs.projectConfig))
                builder.append(postprocessor: try ImageResourcesProcessor(logger: logger, fileManager: fileManager,
                                                                          diskCacheProvider: diskCacheProvider,
                                                                          projectConfig: configs.projectConfig,
                                                                          compilerConfig: configs.compilerConfig,
                                                                          imageToolbox: imageToolbox,
                                                                          imageVariantsFilter: imageVariantsFilter,
                                                                          alwaysUseVariantAgnosticFilenames: hotReloadingEnabled || emitInlineAssets))
            }

            let isCompilingEntireProject = modulesFilter == nil

            builder.append(postprocessor: DiagnosticsProcessor(projectConfig: configs.projectConfig))
            builder.append(postprocessor: ResolveOutputPathsProcessor(logger: logger,
                                                                      projectConfig: configs.projectConfig,
                                                                      compilerConfig: configs.compilerConfig,
                                                                      shouldGenerateKeepXML: isCompilingEntireProject || bazel))
            
            if (configs.projectConfig.webEnabled) {
                builder.append(postprocessor: PrependWebJSProcessor(logger: logger))
            }

            let bundleResourceOutputMode: BundleResourcesProcessor.OutputMode
            if hotReloadingEnabled {
                bundleResourceOutputMode = .assetPackage
            } else if emitInlineAssets {
                bundleResourceOutputMode = .bundledAssetPackage
            } else {
                bundleResourceOutputMode = .uploadedArtifacts
            }
            let downloadableArtifactsProcessingMode: BundleResourcesProcessor.DownloadableArtifactsProcessingMode = configs.compilerConfig.verifyDownloadableArtifacts ? .verifySignatures : .uploadIfNeeded

            builder.append(postprocessor: BundleResourcesProcessor(logger: logger,
                                                                   fileManager: fileManager,
                                                                   diskCacheProvider: diskCacheProvider,
                                                                   projectConfig: configs.projectConfig,
                                                                   compilerConfig: configs.compilerConfig,
                                                                   maxUploadConcurrentRequests: maxUploadConcurrentRequests,
                                                                   outputMode: bundleResourceOutputMode,
                                                                   downloadableArtifactsProcessingMode: downloadableArtifactsProcessingMode,
                                                                   writeDownloadableArtifactsManifest: !bazel,
                                                                   rootBundle: bundleManager.rootBundle,
                                                                   companionExecutable: compilerCompanion
                                                                  ))

            if let daemonService = daemonService, hotReloadingEnabled {
                let hotReloadingProcessor = HotReloadingProcessor(logger: logger, daemonService: daemonService)
                builder.append(postprocessor: hotReloadingProcessor)
            }

            if !hotReloadingEnabled {
                builder.append(postprocessor: FinalFilesVerificationProcessor(logger: logger, projectConfig: configs.projectConfig))
            }
            // we don't do clean up of orphaned files during hot reloading - we keep the compiler generated files
            let removeOrphanFiles = modulesFilter == nil && !configs.projectConfig.shouldSkipRemoveOrphanFiles && !configs.compilerConfig.regenerateValdiModulesBuildFiles && !hotReloadingEnabled
            builder.append(postprocessor: SaveFilesProcessor(logger: logger,
                                                             fileManager: fileManager,
                                                             projectConfig: configs.projectConfig,
                                                             modulesFilter: modulesFilter,
                                                             removeOrphanFiles: removeOrphanFiles,
                                                             hotReloadingEnabled: hotReloadingEnabled))
        }

        // In regenerate mode, don't fail immediately on annotation processing errors
        // These errors are often due to missing type information (generated files don't exist yet)
        // Modules with errors will default to has_ios_exports=True, has_android_exports=True
        let failImmediatelyOnError = !hotReloadingEnabled && !configs.compilerConfig.regenerateValdiModulesBuildFiles
        
        let pipeline = CompilationPipeline(logger: logger,
                                   processors: builder.build(),
                                   deferredWarningCollector: self.deferredWarningCollector,
                                   failImmediatelyOnError: failImmediatelyOnError)

        teardownCallbacks.forEach(pipeline.onTeardown)

        return pipeline
    }

    private func getCompanionExecutable(configs: ResolvedConfigs, diskCacheProvider: DiskCacheProvider) throws -> CompanionExecutable {
        return try self.companionExecutableProvider.getCompanionExecutable(
            compilerConfig: configs.compilerConfig,
            projectConfig: configs.projectConfig,
            logsOutputPath: self.arguments.companionLogOutput, 
            disableTsCompilerCache: self.arguments.disableTsCompilerCache,
            diskCacheProvider: diskCacheProvider,
            isBazelBuild: self.arguments.bazel)
    }
}