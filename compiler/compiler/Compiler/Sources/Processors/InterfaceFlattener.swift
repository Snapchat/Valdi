//
//  InterfaceFlattener.swift
//  Compiler
//
//  Flattens TypeScript interface inheritance for @ExportModel / @ExportProxy interfaces.
//  Walks the `extends` chain, merges each parent interface's members into the child, and
//  translates parent-file typeReferenceIndex values into the child file's reference table.
//

import Foundation

/// Policy for handling inherited member name collisions with the child interface.
/// Only `.error` is honored in v1; the other cases are declared so future implementations
/// can slot in without changing the flattener's public surface.
enum InterfaceInheritanceCollisionPolicy: String {
    case error
    case childWins
    case sameTypeOnly

    static func parse(annotationValue: String?) -> InterfaceInheritanceCollisionPolicy {
        guard let raw = annotationValue, let policy = InterfaceInheritanceCollisionPolicy(rawValue: raw) else {
            return .error
        }
        return policy
    }
}

/// Per-file entry consumed by the flattener when looking up a parent interface.
struct InterfaceFlattenerSymbolEntry {
    let dumpedSymbols: [TS.DumpedSymbolWithComments]
    let references: [TS.AST.TypeReference]
}

/// Global index keyed by extension-stripped compilation path.
/// Extension normalization must match `TypeScriptNativeTypeResolver.makeSrcWithoutExtension`.
struct InterfaceFlattenerSymbolIndex {
    private let entriesByStrippedPath: [String: InterfaceFlattenerSymbolEntry]

    init(entriesByStrippedPath: [String: InterfaceFlattenerSymbolEntry]) {
        self.entriesByStrippedPath = entriesByStrippedPath
    }

    func lookup(fileName: String, symbolName: String) -> (dumpedSymbol: TS.DumpedSymbolWithComments, references: [TS.AST.TypeReference])? {
        let stripped = fileName.removing(suffixes: FileExtensions.typescriptFileExtensionsDotted)
        guard let entry = entriesByStrippedPath[stripped] else { return nil }
        guard let symbol = entry.dumpedSymbols.first(where: { $0.text == symbolName }) else { return nil }
        return (symbol, entry.references)
    }
}

final class InterfaceFlattener {

    /// Flattens `interface` by merging all transitively-reachable parents' members.
    ///
    /// - Parameters:
    ///   - interface: The child interface to flatten. Must have non-empty `supertypes`.
    ///   - fileName: The child interface's own file path (as it would appear in a `TypeReference.fileName`).
    ///   - references: The child file's reference table (used to resolve `supertypes[i].type.typeReferenceIndex`).
    ///   - mergeReference: Closure that integrates a foreign `TypeReference` into the child file's reference table
    ///     and returns its new index in that table. Callers typically back this with a mutating append on
    ///     `TypeScriptCommentedFile`.
    ///   - index: Global symbol index used to resolve parents by `(fileName, symbolName)`.
    ///   - collisionPolicy: Policy for handling name collisions between inherited and child members.
    /// - Returns: A new `Interface` with the union of members and `supertypes = nil`.
    static func flatten(interface: TS.AST.Interface,
                        fileName: String,
                        references: [TS.AST.TypeReference],
                        mergeReference: (TS.AST.TypeReference) -> Int,
                        index: InterfaceFlattenerSymbolIndex,
                        collisionPolicy: InterfaceInheritanceCollisionPolicy = .error) throws -> TS.AST.Interface {

        let rootKey = Self.visitKey(fileName: fileName, name: interface.name)
        var inheritedMembers: [TS.AST.PropertyLikeDeclaration] = []
        var visited: Set<String> = [rootKey]

        try collectInheritedMembers(supertypes: interface.supertypes ?? [],
                                    parentReferences: references,
                                    into: &inheritedMembers,
                                    mergeReference: mergeReference,
                                    index: index,
                                    visited: &visited,
                                    pathKeys: [rootKey],
                                    pathNames: [interface.name])

        var merged = interface
        merged.supertypes = nil
        merged.members = try mergeMembers(inherited: inheritedMembers,
                                          child: interface.members,
                                          collisionPolicy: collisionPolicy)
        return merged
    }

    private static func collectInheritedMembers(supertypes: [TS.AST.SuperTypeClause],
                                                parentReferences: [TS.AST.TypeReference],
                                                into out: inout [TS.AST.PropertyLikeDeclaration],
                                                mergeReference: (TS.AST.TypeReference) -> Int,
                                                index: InterfaceFlattenerSymbolIndex,
                                                visited: inout Set<String>,
                                                pathKeys: [String],
                                                pathNames: [String]) throws {
        for supertype in supertypes {
            if supertype.isImplements {
                throw CompilerError("@ExportModel / @ExportProxy inheritance does not support `implements` clauses (only `extends` is supported).")
            }

            guard let refIndex = supertype.type.typeReferenceIndex else {
                throw CompilerError("Could not resolve parent interface: supertype has no type reference.")
            }

            guard refIndex >= 0, refIndex < parentReferences.count else {
                throw CompilerError("Out of bounds type reference \(refIndex) resolving supertype.")
            }

            let typeRef = parentReferences[refIndex]

            if supertype.type.typeArguments?.isEmpty == false {
                throw CompilerError("Generic parents are not supported for @ExportModel / @ExportProxy inheritance (found 'extends \(typeRef.name)<...>'). This is a v1 limitation.")
            }

            let key = Self.visitKey(fileName: typeRef.fileName, name: typeRef.name)
            if pathKeys.contains(key) {
                // True cycle: the exact (fileName, name) we're about to visit sits on the path
                // we're currently descending. Compare by visit key, not by bare name, so two
                // legitimately different interfaces that share a name across files aren't
                // mistaken for a cycle.
                let cycle = (pathNames + [typeRef.name]).joined(separator: " -> ")
                throw CompilerError("Cyclic interface inheritance detected: \(cycle).")
            }
            if visited.contains(key) {
                // Already fully processed by an earlier sibling branch (diamond inheritance).
                // Its members have already been merged; skipping avoids duplicating them.
                continue
            }

            guard let resolved = index.lookup(fileName: typeRef.fileName, symbolName: typeRef.name) else {
                throw CompilerError("Could not resolve parent interface '\(typeRef.name)' declared in '\(typeRef.fileName)'. Make sure the parent is exported and visible to the compiler.")
            }

            guard resolved.dumpedSymbol.kind == TS.SyntaxKind.interfaceDeclaration else {
                throw CompilerError("Cannot flatten '\(typeRef.name)': only TypeScript interfaces can be used as parents of an @ExportModel / @ExportProxy interface.")
            }

            guard let parentInterface = resolved.dumpedSymbol.interface else {
                throw CompilerError("Parent '\(typeRef.name)' has no interface data.")
            }

            visited.insert(key)

            // Depth-first: grandparents contribute their members before the direct parent.
            try collectInheritedMembers(supertypes: parentInterface.supertypes ?? [],
                                        parentReferences: resolved.references,
                                        into: &out,
                                        mergeReference: mergeReference,
                                        index: index,
                                        visited: &visited,
                                        pathKeys: pathKeys + [key],
                                        pathNames: pathNames + [typeRef.name])

            for member in parentInterface.members {
                // Reject annotated parent members up front. Their leadingComments carry semantics
                // (`@Untyped`, `@UntypedMap`, `@SingleCall`, `@WorkerThread`, `@AllowSyncCall`,
                // `@ConstructorOmitted`, `@Injectable`) that we can't safely re-interpret against
                // the child file's source, so flattening them would silently drop those flags or
                // fail with a confusing "Unrecognized type" downstream. Force callers to opt into
                // `ignoreInheritance: 'true'` (or remove the annotation from the parent) rather
                // than land a silent semantic change.
                if let comments = member.leadingComments, Self.containsValdiAnnotation(commentsText: comments.text) {
                    throw CompilerError("Cannot flatten parent '\(typeRef.name)' member '\(member.name)': it carries a Valdi annotation (`@Untyped`, `@WorkerThread`, `@Injectable`, etc.) whose semantics can't be preserved through inheritance in v1. Set `ignoreInheritance: 'true'` on the child, or remove the annotation from the parent.")
                }
                let translated = translate(member: member,
                                           fromReferences: resolved.references,
                                           mergeReference: mergeReference)
                out.append(translated)
            }
        }
    }

    private static func mergeMembers(inherited: [TS.AST.PropertyLikeDeclaration],
                                     child: [TS.AST.PropertyLikeDeclaration],
                                     collisionPolicy: InterfaceInheritanceCollisionPolicy) throws -> [TS.AST.PropertyLikeDeclaration] {
        var seen: [String: TS.AST.PropertyLikeDeclaration] = [:]
        var order: [String] = []

        for member in inherited {
            if let previous = seen[member.name] {
                try resolveCollision(existing: previous, incoming: member, policy: collisionPolicy)
                // .error is the only wired branch in v1 — resolveCollision throws before reaching here.
            } else {
                seen[member.name] = member
                order.append(member.name)
            }
        }

        for member in child {
            if let previous = seen[member.name] {
                try resolveCollision(existing: previous, incoming: member, policy: collisionPolicy)
            } else {
                seen[member.name] = member
                order.append(member.name)
            }
        }

        return order.compactMap { seen[$0] }
    }

    private static func resolveCollision(existing: TS.AST.PropertyLikeDeclaration,
                                         incoming: TS.AST.PropertyLikeDeclaration,
                                         policy: InterfaceInheritanceCollisionPolicy) throws {
        switch policy {
        case .error:
            throw CompilerError("Inherited member '\(incoming.name)' collides with an existing member. Rename one, or set the `inheritanceCollisionPolicy` annotation parameter once override modes are supported.")
        case .childWins, .sameTypeOnly:
            // Reserved for future implementation; treated as .error in v1 so behavior is explicit.
            throw CompilerError("`inheritanceCollisionPolicy=\(policy.rawValue)` is reserved for a future release. Only `error` is supported today.")
        }
    }

    /// Rewrites every `typeReferenceIndex` inside `member.type` so it indexes into the child
    /// file's reference table instead of the parent's.
    private static func translate(member: TS.AST.PropertyLikeDeclaration,
                                  fromReferences: [TS.AST.TypeReference],
                                  mergeReference: (TS.AST.TypeReference) -> Int) -> TS.AST.PropertyLikeDeclaration {
        var copy = member
        copy.type = translate(type: member.type,
                              fromReferences: fromReferences,
                              mergeReference: mergeReference)
        // Drop the parent's leadingComments: their `start/end` positions refer to the parent file
        // and `extractAnnotations` would consult the child's `fileContent`, producing bad ranges
        // when parameter parsing kicks in. v1 limitation: inherited members lose comment-borne
        // annotations. Documented in the plan.
        copy.leadingComments = nil
        return copy
    }

    private static func translate(type: TS.AST.TSType,
                                  fromReferences: [TS.AST.TypeReference],
                                  mergeReference: (TS.AST.TypeReference) -> Int) -> TS.AST.TSType {
        var translatedTypeReferenceIndex = type.typeReferenceIndex
        if let idx = type.typeReferenceIndex, idx >= 0, idx < fromReferences.count {
            translatedTypeReferenceIndex = mergeReference(fromReferences[idx])
        }

        let translatedFunction = type.function.map { function -> TS.AST.FunctionType in
            let translatedParams = function.parameters.map { param -> TS.AST.PropertyLikeDeclaration in
                translate(member: param, fromReferences: fromReferences, mergeReference: mergeReference)
            }
            return TS.AST.FunctionType(parameters: translatedParams,
                                       returnValue: translate(type: function.returnValue,
                                                              fromReferences: fromReferences,
                                                              mergeReference: mergeReference))
        }

        let translatedUnions = type.unions?.map { union in
            translate(type: union, fromReferences: fromReferences, mergeReference: mergeReference)
        }

        let translatedArray = type.array.map { arrayType in
            translate(type: arrayType, fromReferences: fromReferences, mergeReference: mergeReference)
        }

        let translatedTypeArguments = type.typeArguments?.map { arg in
            TS.AST.TypeArgument(type: translate(type: arg.type,
                                                fromReferences: fromReferences,
                                                mergeReference: mergeReference))
        }

        // Drop leadingComments for the same reason as PropertyLikeDeclaration.leadingComments:
        // start/end offsets refer to the parent file, but downstream annotation extraction reads
        // against the child file's fileContent. Retaining them can crash LinesIndexer when
        // offsets exceed the child file's length. Losing inherited type-position annotations
        // (e.g. `foo: /* @AllowSyncCall */ () => void`) is the documented v1 tradeoff.
        return TS.AST.TSType(name: type.name,
                             leadingComments: nil,
                             function: translatedFunction,
                             unions: translatedUnions,
                             typeReferenceIndex: translatedTypeReferenceIndex,
                             array: translatedArray,
                             typeArguments: translatedTypeArguments,
                             isTypeParameter: type.isTypeParameter)
    }

    private static func visitKey(fileName: String, name: String) -> String {
        return "\(fileName.removing(suffixes: FileExtensions.typescriptFileExtensionsDotted))|\(name)"
    }

    /// Matches the companion's dump filter regex — any of these tags in the comment implies the
    /// author expects annotation semantics. Since we can't reliably re-parse annotation parameters
    /// against the child file's source, treat their presence on an inherited member as a hard error.
    private static let valdiAnnotationRegex = try! NSRegularExpression(
        pattern: "@(Generate|Export|Component|ViewModel|Context|Native|Untyped|UntypedMap|SingleCall|WorkerThread|AllowSyncCall|ConstructorOmitted|Injectable|Action)\\b")

    private static func containsValdiAnnotation(commentsText: String) -> Bool {
        let range = NSRange(commentsText.startIndex..<commentsText.endIndex, in: commentsText)
        return valdiAnnotationRegex.firstMatch(in: commentsText, options: [], range: range) != nil
    }
}
