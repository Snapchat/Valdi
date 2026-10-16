import Foundation

struct WebNativeModuleIdOverride {
    let implementationPath: String
    let moduleIds: [String]

    static func parse(_ raw: String) throws -> WebNativeModuleIdOverride {
        let parts = raw.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
        guard parts.count == 2 else {
            throw CompilerError("Invalid web native module override '\(raw)'. Expected implementation/path.js=module/id[,otherId]")
        }

        let implementationPath = String(parts[0]).trimmed
        let moduleIds = String(parts[1])
            .split(separator: ",", omittingEmptySubsequences: false)
            .map { String($0).trimmed }

        guard !implementationPath.isEmpty else {
            throw CompilerError("Invalid web native module override '\(raw)': implementation path is empty")
        }
        guard !moduleIds.isEmpty else {
            throw CompilerError("Invalid web native module override '\(raw)': module id list is empty")
        }
        guard moduleIds.allSatisfy({ !$0.isEmpty }) else {
            throw CompilerError("Invalid web native module override '\(raw)': module id list contains an empty value")
        }

        return WebNativeModuleIdOverride(implementationPath: implementationPath, moduleIds: moduleIds)
    }
}
