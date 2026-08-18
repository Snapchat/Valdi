import Foundation

final class GenerateWebNativeModulePackageFilesProcessor: CompilationProcessor {
    private let compilerConfig: CompilerConfig
    private let bundleManager: BundleManager

    init(compilerConfig: CompilerConfig, bundleManager: BundleManager) {
        self.compilerConfig = compilerConfig
        self.bundleManager = bundleManager
    }

    var description: String {
        return "Generating web native module package files"
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        guard compilerConfig.outputForWeb,
              !compilerConfig.webNativeModuleIdOverrides.isEmpty else {
            return items
        }

        let implementationPathByModuleId = try buildImplementationPathByModuleId()
        var generatedItems = [CompilationItem]()

        for moduleId in implementationPathByModuleId.keys.sorted() {
            let implementationPath = implementationPathByModuleId[moduleId]!
            let bundleName = String(implementationPath.split(separator: "/", maxSplits: 1).first ?? "")
            let bundleInfo = try bundleManager.getBundleInfo(forName: bundleName)

            let contents = Self.packageFileContents(moduleId: moduleId, implementationPath: implementationPath)
            generatedItems += Self.makeItems(bundleInfo: bundleInfo,
                                             moduleId: moduleId,
                                             contents: contents)
        }

        return CompilationItems(compileSequence: items.compileSequence, items: items.allItems + generatedItems)
    }

    private func buildImplementationPathByModuleId() throws -> [String: String] {
        var implementationPathByModuleId = [String: String]()

        for override in compilerConfig.webNativeModuleIdOverrides {
            for moduleId in override.moduleIds {
                if let existing = implementationPathByModuleId[moduleId],
                   existing != override.implementationPath {
                    throw CompilerError("Web native module id '\(moduleId)' is mapped to both '\(existing)' and '\(override.implementationPath)'")
                }
                implementationPathByModuleId[moduleId] = override.implementationPath
            }
        }

        return implementationPathByModuleId
    }

    private static func makeItems(bundleInfo: CompilationItem.BundleInfo,
                                  moduleId: String,
                                  contents: String) -> [CompilationItem] {
        let file = File.string(contents)
        let relativeProjectPath = bundleInfo.relativeProjectPath(forItemPath: itemPath(bundleInfo: bundleInfo,
                                                                                       moduleId: moduleId))
        let sourceURL = bundleInfo.absoluteURL(forRelativeProjectPath: relativeProjectPath)
        var out = [CompilationItem]()

        if let webReleaseOutputDirectories = bundleInfo.webReleaseOutputDirectories {
            let outputURL = outputURL(outDirectories: webReleaseOutputDirectories,
                                      bundleInfo: bundleInfo,
                                      moduleId: moduleId)
            out.append(CompilationItem(sourceURL: sourceURL,
                                       relativeProjectPath: relativeProjectPath,
                                       kind: .finalFile(FinalFile(outputURL: outputURL,
                                                                  file: file,
                                                                  platform: .web,
                                                                  kind: .compiledSource)),
                                       bundleInfo: bundleInfo,
                                       platform: .web,
                                       outputTarget: .release))
        }

        if let webDebugOutputDirectories = bundleInfo.webDebugOutputDirectories {
            let outputURL = outputURL(outDirectories: webDebugOutputDirectories,
                                      bundleInfo: bundleInfo,
                                      moduleId: moduleId)
            out.append(CompilationItem(sourceURL: sourceURL,
                                       relativeProjectPath: relativeProjectPath,
                                       kind: .finalFile(FinalFile(outputURL: outputURL,
                                                                  file: file,
                                                                  platform: .web,
                                                                  kind: .compiledSource)),
                                       bundleInfo: bundleInfo,
                                       platform: .web,
                                       outputTarget: .debug))
        }

        return out
    }

    private static func itemPath(bundleInfo: CompilationItem.BundleInfo, moduleId: String) -> String {
        let ownedPrefix = "\(bundleInfo.name)/"
        if moduleId.hasPrefix(ownedPrefix) {
            return "\(String(moduleId.dropFirst(ownedPrefix.count))).js"
        }
        return "\(moduleId).js"
    }

    private static func outputURL(outDirectories: OutDirectories,
                                  bundleInfo: CompilationItem.BundleInfo,
                                  moduleId: String) -> URL {
        let ownedPrefix = "\(bundleInfo.name)/"
        if moduleId.hasPrefix(ownedPrefix) {
            let moduleRelativePath = String(moduleId.dropFirst(ownedPrefix.count))
            return outDirectories.assetsURL.appendingPathComponent("\(moduleRelativePath).js")
        }

        return outDirectories.baseURL
            .appendingPathComponent("assets", isDirectory: true)
            .appendingPathComponent("\(moduleId).js")
    }

    private static func packageFileContents(moduleId: String, implementationPath: String) -> String {
        let requirePath = relativeNativeRequirePath(to: implementationPath, from: moduleId)
        return """
        // AUTO-GENERATED by Valdi. Do not edit.
        module.exports = require('\(requirePath.jsonEscaped)');

        """
    }

    private static func relativeNativeRequirePath(to implementationPath: String, from moduleId: String) -> String {
        let packageFileDirectoryComponents = ["src"] + moduleId.split(separator: "/").dropLast().map(String.init)
        let nativeFileComponents = ["native"] + implementationPath.split(separator: "/").map(String.init)
        return RelativePath(from: packageFileDirectoryComponents, to: nativeFileComponents).description
    }
}
