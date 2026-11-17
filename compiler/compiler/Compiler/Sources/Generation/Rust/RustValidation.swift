// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

enum RustValidation {
    static let keywords: Set<String> = [
        "Self", "abstract", "as", "async", "await", "become", "box", "break", "const", "continue",
        "crate", "do", "dyn", "else", "enum", "extern", "false", "final", "fn", "for", "gen", "if",
        "impl", "in", "let", "loop", "macro", "match", "mod", "move", "mut", "override", "priv", "pub",
        "ref", "return", "self", "static", "struct", "super", "trait", "true", "try", "type", "typeof",
        "union", "unsafe", "unsized", "use", "virtual", "where", "while", "yield",
    ]

    static func validateDeclarationIsLocal(tsType: String,
                                           declarationIncludeDir: String?,
                                           moduleIncludeDir: String?,
                                           bundleName: String) throws {
        guard declarationIncludeDir == moduleIncludeDir else {
            let declarationModule = declarationIncludeDir?.split(separator: "/").last.map(String.init) ?? "unknown"
            throw CompilerError("Rust native signature references foreign declaration '\(tsType)' from Valdi module '\(declarationModule)'. Rust native signatures may only reference declarations from '\(bundleName)'.")
        }
    }
}
