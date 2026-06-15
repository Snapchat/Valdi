import Foundation

class PrependWebJSProcessor: CompilationProcessor {
    let logger: ILogger

    // Files excluded from web transforms (module.path prefix, require stripping,
    // dynamic require conversion) because they run before moduleLoader exists.
    // Init.js bootstraps moduleLoader; the others are loaded during that bootstrap.
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
            
            var newFile = finalFile.file
            var contents: String = (try? newFile.readString()) ?? ""

            // Strip extra args from multi-arg require() calls.
            // Source-level require("name", true, true) → require("name").
            // Extra args (disableProxy, disableSyncDeps) are native-only;
            // bundlers only handle single-arg require().
            contents = PrependWebJSProcessor.stripMultiArgRequires(contents)

            // Add .js extension to relative requires that lack one.
            // Compiled output has require("../../res"), require("./utils/Foo"),
            // etc. Without the extension, webpack may resolve to a same-named
            // directory (e.g. res/) instead of the .js file. Explicit extensions
            // remove the ambiguity.
            contents = PrependWebJSProcessor.addJsExtToRelativeRequires(contents)

            // Convert annotated dynamic requires to moduleLoader.load().
            // The companion AST transformer annotates variable-arg require()
            // with /* @valdi-dynamic */. Replace the annotation + require(
            // with globalThis.moduleLoader.load(.
            contents = contents.replacingOccurrences(
                of: "/* @valdi-dynamic */ require(",
                with: "globalThis.moduleLoader.load("
            )

            // Set up module.path for code that uses NavigationPage decorator.
            let moduleSetup = "module.path = \"\(relativePath)\";\n"
            let prefix = moduleSetup
            if let data = (prefix + contents).data(using: .utf8) {
                newFile = .data(data)
            }
            return item.with(newKind: .finalFile(FinalFile(outputURL: finalFile.outputURL, file: newFile, platform: .web, kind: finalFile.kind)))
        }
    }

    /// Appends `.js` to relative require paths that have no file extension.
    /// `require("../../res")` → `require("../../res.js")`
    /// Leaves bare modules (`require("tslib")`) and already-extended paths alone.
    static func addJsExtToRelativeRequires(_ input: String) -> String {
        var result = input
        for quote in ["\"", "'"] {
            let target = "require(\(quote)"
            var searchFrom = result.startIndex
            while let range = result.range(of: target, range: searchFrom..<result.endIndex) {
                let pathStart = range.upperBound
                guard let quoteEnd = result[pathStart...].firstIndex(of: Character(quote)) else {
                    searchFrom = range.upperBound
                    continue
                }
                let path = String(result[pathStart..<quoteEnd])

                guard path.hasPrefix("./") || path.hasPrefix("../") else {
                    searchFrom = result.index(after: quoteEnd)
                    continue
                }
                if path.hasSuffix(".js") || path.hasSuffix(".json") {
                    searchFrom = result.index(after: quoteEnd)
                    continue
                }

                let offset = result.distance(from: result.startIndex, to: quoteEnd)
                result.insert(contentsOf: ".js", at: quoteEnd)
                searchFrom = result.index(result.startIndex, offsetBy: offset + 3)
            }
        }
        return result
    }

    // Strips extra args from string-literal multi-arg require() calls on web.
    // Companion's WebRequireTransformer wraps each such call in
    // /* @valdi-web-strip-start */ ... /* @valdi-web-strip-end */ sentinels
    // so we don't need to scan the whole file or paren-match nested args —
    // we just rewrite each sentinel-bounded region to a single-arg require.
    // A stray start without a matching end (or vice versa) is left alone.
    static func stripMultiArgRequires(_ input: String) -> String {
        let startMarker = "/* @valdi-web-strip-start */"
        let endMarker = "/* @valdi-web-strip-end */"
        var result = input
        var searchFrom = result.startIndex
        while let start = result.range(of: startMarker, range: searchFrom..<result.endIndex),
              let end = result.range(of: endMarker, range: start.upperBound..<result.endIndex) {
            let slice = String(result[start.upperBound..<end.lowerBound])
            guard let stripped = extractRequireWithFirstArgOnly(slice) else {
                // Sentinels present but content malformed — leave intact.
                // Should never happen since AST emits the pair, but fail-safe
                // rather than crash if a future refactor breaks the invariant.
                searchFrom = end.upperBound
                continue
            }
            let replacementRange = start.lowerBound..<end.upperBound
            result.replaceSubrange(replacementRange, with: stripped)
            searchFrom = result.index(start.lowerBound, offsetBy: stripped.count)
        }
        return result
    }

    // Sentinel-bounded slice is ` require("name", arg2, arg3) `. Find the
    // quoted module name (first string literal after the opening paren) and
    // return `require("name")`. AST guarantees the shape — no nested call /
    // paren handling needed.
    private static func extractRequireWithFirstArgOnly(_ s: String) -> String? {
        guard let openParen = s.firstIndex(of: "(") else { return nil }
        let afterOpen = s.index(after: openParen)
        guard afterOpen < s.endIndex else { return nil }
        let quote = s[afterOpen]
        guard quote == "\"" || quote == "'" else { return nil }
        var cursor = s.index(after: afterOpen)
        while cursor < s.endIndex, s[cursor] != quote {
            if s[cursor] == "\\" {
                cursor = s.index(cursor, offsetBy: 2, limitedBy: s.endIndex) ?? s.endIndex
            } else {
                cursor = s.index(after: cursor)
            }
        }
        guard cursor < s.endIndex else { return nil }
        let name = s[s.index(after: afterOpen)..<cursor]
        return "require(\(quote)\(name)\(quote))"
    }
}