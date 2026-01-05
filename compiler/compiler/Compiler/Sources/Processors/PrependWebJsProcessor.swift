import Foundation

class PrependWebJSProcessor: CompilationProcessor {
    let logger: ILogger

    // Files are excluded because they run before module resolution is setup
    let excluded_files = [
        "web_renderer/src/ValdiWebRenderer.js", 
        "web_renderer/src/ValdiWebRuntime.js",
        "valdi_core/src/Init.js", 
        "valdi_core/src/ModuleLoader.js"
    ]

    init(logger: ILogger) {
        self.logger = logger
    }

    var description: String {
        return "Modify js files for web"
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        return items.select { (item) -> FinalFile? in
            switch item.kind {
            case let .finalFile(finalFile):
                if let platform = finalFile.platform, platform == .web, finalFile.outputURL.lastPathComponent.hasSuffix(".js") {
                    return finalFile
                }
                return nil
            default:
                return nil
            }
        }.transformEach { selected -> CompilationItem in
            let item = selected.item
            guard case let .finalFile(finalFile) = item.kind else {
                return item
            }

            let finalFileOutput = finalFile.outputURL.relativeString

            for name in excluded_files {
                if finalFileOutput.contains(name) {
                    return item
                }
            }

            var relativePath = item.relativeProjectPath
            // Strip TypeScript extensions (.tsx, .ts) from the path since compiled files are .js
            // This ensures module.path matches what the module loader expects
            if relativePath.hasSuffix(".tsx") {
                relativePath = String(relativePath.dropLast(4))
            } else if relativePath.hasSuffix(".ts") {
                relativePath = String(relativePath.dropLast(3))
            }
            
            logger.debug("PrependWebJSProcessor: ========== Processing Web JS File ==========")
            logger.debug("PrependWebJSProcessor: Output file path: \(finalFileOutput)")
            logger.debug("PrependWebJSProcessor: relativeProjectPath (original): '\(item.relativeProjectPath)'")
            logger.debug("PrependWebJSProcessor: relativeProjectPath (adjusted): '\(relativePath)'")
            logger.debug("PrependWebJSProcessor: relativeProjectPath length: \(relativePath.count)")
            logger.debug("PrependWebJSProcessor: sourceURL: \(item.sourceURL.absoluteString)")
            logger.debug("PrependWebJSProcessor: bundleInfo.name: \(item.bundleInfo.name)")
            logger.debug("PrependWebJSProcessor: outputURL.lastPathComponent: \(finalFile.outputURL.lastPathComponent)")
            
            var newFile = finalFile.file
            var contents: String? = try? newFile.readString()
            let contentsLength = contents?.count ?? 0
            logger.debug("PrependWebJSProcessor: File contents length: \(contentsLength)")
            
            // Count how many require( calls are in the file
            let requireCount = contents?.components(separatedBy: "require(").count ?? 1
            logger.debug("PrependWebJSProcessor: Found \(requireCount - 1) 'require(' occurrences")
            
            // Transform require( to customRequire( - this must happen for all web JS files
            // Note: TypeScript with module: "commonjs" already transforms import() to Promise.resolve().then(() => require(...)),
            // so we only need to transform require( to customRequire( and the import() transformation is handled automatically.
            contents = contents?.replacingOccurrences(of: "require(", with: "customRequire(")
            
            // Set up module.path for code that uses NavigationPage decorator
            // The module variable is provided by webpack as a function parameter, so we just set the path property
            // The module variable is declared in source code as: declare const module: { path: string; exports: unknown };
            // Note: We use the adjusted relativePath (without .tsx/.ts extension) so module resolution works correctly
            let moduleSetup = "module.path = \"\(relativePath)\";\n"
            let prefix = "\(moduleSetup)var customRequire = globalThis.moduleLoader.resolveRequire(\"\(relativePath)\");\n"
            logger.debug("PrependWebJSProcessor: Generated prefix (first 100 chars): \(String(prefix.prefix(100)))")
            logger.debug("PrependWebJSProcessor: ==========================================")
            if let data = (prefix + (contents ?? "" )).data(using: .utf8) {
                newFile = .data(data)
            }
            return item.with(newKind: .finalFile(FinalFile(outputURL: finalFile.outputURL, file: newFile, platform: .web, kind: finalFile.kind)))
        }
    }
}