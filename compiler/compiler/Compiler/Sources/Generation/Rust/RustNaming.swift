// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

extension String {
    var rustIdentifier: String {
        var output = unicodeScalars.map { scalar -> Character in
            if CharacterSet.alphanumerics.contains(scalar) || scalar == "_" {
                return Character(String(scalar))
            }
            return "_"
        }
        if output.isEmpty {
            output = ["_"]
        }
        if output[0].isNumber {
            output.insert("_", at: 0)
        }
        let identifier = String(output)
        return RustValidation.keywords.contains(identifier) ? "r#\(identifier)" : identifier
    }

    var rustTypeIdentifier: String {
        let identifier = rustIdentifier
        return identifier.hasPrefix("r#") ? "\(identifier.dropFirst(2))_" : identifier
    }

    var rustCSymbol: String {
        return rustIdentifier.replacingOccurrences(of: "r#", with: "")
    }

    var escapedForRustStringLiteral: String {
        return
            jsonEscaped
            .replacingOccurrences(of: "\r", with: "\\r")
    }
}

final class RustIdentifierScope {
    private var originalByResolvedName: [String: String] = [:]

    func allocate(_ original: String, resolved: String) throws -> String {
        if let existing = originalByResolvedName[resolved], existing != original {
            throw CompilerError(
                "Rust code generation name collision: '\(existing)' and '\(original)' both resolve to '\(resolved)'"
            )
        }
        originalByResolvedName[resolved] = original
        return resolved
    }
}

struct RustCodeFormatting {
    private init() {}

    static func indentLines(_ source: String, spaces: Int) -> String {
        let indentation = String(repeating: " ", count: spaces)
        return source.components(separatedBy: "\n").map { line in
            line.isEmpty ? line : indentation + line
        }.joined(separator: "\n")
    }
}
