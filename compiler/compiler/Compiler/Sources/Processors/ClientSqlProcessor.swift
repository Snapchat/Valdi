//
//  ClientSqlProcessor.swift
//
//
//  Created by Li Feng on 2/20/23.
//

import Foundation
import Yams

final class ClientSqlProcessor: CompilationProcessor {

    var description: String {
        return "Generating code for SQL"
    }

    private let logger: ILogger
    private let fileManager: ValdiFileManager
    private let projectConfig: ValdiProjectConfig
    private let rootBundle: CompilationItem.BundleInfo

    // bundle=>file=>content
    private var bundleToSourceMap: Synchronized<[String: [RelativePath: String]]> = Synchronized(data: [:])
    private let diskCache: DiskCache?

    init(logger: ILogger, 
         fileManager: ValdiFileManager,
         diskCacheProvider: DiskCacheProvider,
         projectConfig: ValdiProjectConfig,
         rootBundle: CompilationItem.BundleInfo
    ) throws {
        self.logger = logger
        self.fileManager = fileManager
        self.projectConfig = projectConfig
        self.rootBundle = rootBundle

        if let clientSqlCompilerPath = projectConfig.clientSqlURL?.path, diskCacheProvider.isEnabled() {
            logger.debug("Resolving SQL version")
            let output = try SyncProcessHandle.run(logger: logger, command: clientSqlCompilerPath, arguments: ["-version"]).trimmed
            diskCache = diskCacheProvider.newCache(cacheName: "clientsql", outputExtension: "ts", metadata: ["version": output])
        } else {
            diskCache = nil
        }
    }

    private func runSqlDelight(items: GroupedItems<CompilationItem.BundleInfo, SelectedItem<File>>) throws -> [CompilationItem] {
        let bundleInfo = items.key
        let sqlUrl = bundleInfo.baseDir.appendingPathComponent("sql")
        let sqlDir = sqlUrl.path

        // Update source map with incoming files (may happen concurrently from different bundles)
        let sourceMap = try bundleToSourceMap.data {bundleToSourceMap in
            var sourceMap = bundleToSourceMap[bundleInfo.name, default: Dictionary()]
            for item in items.items {
                let relativePath = item.item.relativeBundlePath
                if relativePath.components.first == "sql" {
                    sourceMap[relativePath] = try item.data.readString()
                }
            }
            bundleToSourceMap[bundleInfo.name] = sourceMap
            return sourceMap
        }

        if sourceMap.isEmpty {
            logger.verbose("No valid SQL sources found for module \(items.key.name)")
            return []
        }

        // Expected SQL source tree layout:
        // proj_root
        //   src  (TS source code)
        //   test (TS test code)
        //   sql  (SQL source code)
        //     my_db_class_name (schema and query definitions)
        //       my_db_stuff.sq
        //     migration (optional migration file directory)
        //       1.sqm
        //       2.sqm
        //     sql_types.yaml (optional type mapping file)
        let migrationDirName = "migration"
        let pkg = sourceMap.keys.reduce("", {
            // package/class/module = first dir under sqldir that's not "migration" or type mapping
            guard $1.components.count > 2 else { return $0 }
            let dir = $1.components[1]
            return ($0 == "" && dir != migrationDirName && dir != Files.sqlTypesYaml && dir != Files.sqlManifestYaml) ? dir : $0
        })
        if pkg == "" {
            return []
        }
        logger.verbose("-- CLIENTSQL generating code for package \(bundleInfo.name)/\(pkg)")
        let clazz = pkg
        let outdir = "src/sqlgen"
        let tmfile = "\(sqlDir)/\(Files.sqlTypesYaml)"
        let typemapping = FileManager.default.isReadableFile(atPath: tmfile) ?
        ["-tm", Files.sqlTypesYaml] : []

        // Locate the input files and expected output file path
        let outputDirectory = projectConfig.generatedTsDirectoryURL
            .appendingPathComponent("\(bundleInfo.name)")
            .appendingPathComponent(outdir)
            .resolvingSymlinksInPath()
        try fileManager.createDirectory(at: outputDirectory)

        // All input files sorted by path and put together as the key to locate the DiskCache entry
        let sortedSourcePaths = sourceMap.keys.sorted()
        let inputData = try sortedSourcePaths
            .reduce("", {$0 + "[\($1)]" + sourceMap[$1]!})
            .utf8Data()

        if let cachedData = diskCache?.getOutput(item: outputDirectory.path, inputData: inputData) {
            logger.verbose("-- CLIENTSQL use cached output: \(outputDirectory.path)")
            if let outputFiles:[String: Data] = try (NSKeyedUnarchiver.unarchivedObject(ofClass:NSDictionary.self, from:cachedData)) as? [String : Data] {
                var emittedItems: [CompilationItem] = []
                for (relativePath, tsContents) in outputFiles {
                    let relativeProjectPath = "\(bundleInfo.name)/\(outdir)/\(relativePath)"
                    let emittedTsSourceURL = bundleInfo.absoluteURL(forRelativeProjectPath: relativeProjectPath)
                    let item = CompilationItem(sourceURL: emittedTsSourceURL,
                                               relativeProjectPath: relativeProjectPath,
                                               kind: .typeScript(.data(tsContents), emittedTsSourceURL),
                                               bundleInfo: bundleInfo,
                                               platform: nil,
                                               outputTarget: .all)
                    emittedItems.append(item)
                }
                return emittedItems
            }
        }

        // Run the external compiler
        let compilerPath = try projectConfig.ensureClientSqlCompiler()

        // Clear any stale files from prior runs so the post-compile enumerator
        // only picks up files emitted by this invocation. The directory path is
        // Valdi-controlled (generatedTsDirectoryURL / <bundle> / src/sqlgen),
        // not user-supplied, so a scoped wipe is safe here.
        if let existing = try? FileManager.default.contentsOfDirectory(at: outputDirectory, includingPropertiesForKeys: nil) {
            for url in existing {
                try? FileManager.default.removeItem(at: url)
            }
        }

        let args = ["-s", sqlDir,
                    "-p", pkg,
                    "-c", clazz,
                    "-m", clazz,
                    "-o", outputDirectory.path,
                    "-l", "typescript"] + typemapping
        let output = try SyncProcessHandle.run(logger: logger, command: compilerPath, arguments: args)

        logger.verbose("-- CLIENTSQL compiler:")
        logger.info(output)

        var outputFiles: [String: Data] = [:]
        var emittedItems: [CompilationItem] = []
        if let enumerator = FileManager.default.enumerator(at: outputDirectory, includingPropertiesForKeys: [.isRegularFileKey], options: [.skipsHiddenFiles, .skipsPackageDescendants]) {
            for case let fileURL as URL in enumerator {
                do {
                    let fileAttributes = try fileURL.resourceValues(forKeys:[.isRegularFileKey])
                    if fileAttributes.isRegularFile! {
                        let relativePath = fileURL.relativePath(from: outputDirectory)
                        logger.verbose("-- CLIENTSQL file: \(relativePath)")
                        let relativeProjectPath = "\(bundleInfo.name)/\(outdir)/\(relativePath)"
                        let emittedTsSourceURL = bundleInfo.absoluteURL(forRelativeProjectPath: relativeProjectPath)
                        let tsContents = try String(contentsOfFile: fileURL.path).utf8Data()
                        outputFiles[relativePath] = tsContents
                        let item = CompilationItem(sourceURL: emittedTsSourceURL,
                                                   relativeProjectPath: relativeProjectPath,
                                                   kind: .typeScript(.data(tsContents), emittedTsSourceURL),
                                                   bundleInfo: bundleInfo,
                                                   platform: nil,
                                                   outputTarget: .all)
                        emittedItems.append(item)
                    }
                } catch { logger.error("-- CLIENTSQL error: \(error) at \(fileURL)") }
            }
        }

        if let cachedData = try? NSKeyedArchiver.archivedData(withRootObject: outputFiles, requiringSecureCoding: false) {
            try diskCache?.setOutput(item: outputDirectory.path, inputData: inputData, outputData: cachedData)
        }
        return emittedItems
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        // Select *.sq files, we will re-run the codegen if any of them has changed
        return try items.select { (item) -> File? in
            switch item.kind {
            case .sql(let file):
                return file
            default:
                return nil
            }
        }.groupBy { (item) -> CompilationItem.BundleInfo in
            return item.item.bundleInfo
        }.transformEachConcurrently(runSqlDelight)
    }
}

extension ValdiProjectConfig {
    func ensureClientSqlCompiler() throws -> String {
        guard let path = clientSqlURL?.path else {
            throw CompilerError("ClientSQL binary path is not defined")
        }

        return path
    }

    func globalMetadataURL(for platform: Platform) throws -> URL? {
        let output: ValdiOutputConfig?
        switch platform {
        case .ios:
            output = iosOutput
        case .android:
            output = androidOutput
        case .web:
            output = webOutput
        case .cpp:
            output = cppOutput
        }
        return try output?.globalMetadataPath?.resolve()
    }
}
