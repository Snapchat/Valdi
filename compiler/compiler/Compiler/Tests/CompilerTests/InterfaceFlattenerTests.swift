import XCTest
import Foundation
@testable import Compiler

// MARK: - AST fixture helpers

private func makeType(name: String, refIndex: Int? = nil, typeArguments: [TS.AST.TypeArgument]? = nil) -> TS.AST.TSType {
    return TS.AST.TSType(name: name, typeReferenceIndex: refIndex, typeArguments: typeArguments)
}

private func makeMember(name: String, type: TS.AST.TSType, isOptional: Bool = false) -> TS.AST.PropertyLikeDeclaration {
    return TS.AST.PropertyLikeDeclaration(start: 0, end: 0, name: name, isOptional: isOptional, type: type, leadingComments: nil)
}

private func makeSuper(refIndex: Int, isImplements: Bool = false, typeArguments: [TS.AST.TypeArgument]? = nil) -> TS.AST.SuperTypeClause {
    return TS.AST.SuperTypeClause(start: 0, end: 0, isImplements: isImplements, type: makeType(name: "", refIndex: refIndex, typeArguments: typeArguments))
}

private func makeInterface(name: String,
                           members: [TS.AST.PropertyLikeDeclaration],
                           supertypes: [TS.AST.SuperTypeClause]? = nil) -> TS.AST.Interface {
    return TS.AST.Interface(start: 0, end: 0, name: name, members: members, typeParameters: nil, supertypes: supertypes)
}

private func makeDumpedInterface(name: String, interface: TS.AST.Interface) -> TS.DumpedSymbolWithComments {
    return TS.DumpedSymbolWithComments(nodeType: "interface",
                                       start: 0,
                                       leadingComments: nil,
                                       text: name,
                                       kind: .interfaceDeclaration,
                                       modifiers: nil,
                                       function: nil,
                                       enum: nil,
                                       interface: interface,
                                       variable: nil,
                                       exportedTypeAlias: nil)
}

private func makeDumpedClass(name: String, interface: TS.AST.Interface) -> TS.DumpedSymbolWithComments {
    return TS.DumpedSymbolWithComments(nodeType: "interface",
                                       start: 0,
                                       leadingComments: nil,
                                       text: name,
                                       kind: .classDeclaration,
                                       modifiers: nil,
                                       function: nil,
                                       enum: nil,
                                       interface: interface,
                                       variable: nil,
                                       exportedTypeAlias: nil)
}

// A tiny mutable-references shim mimicking what TypeScriptCommentedFile does.
private final class RefBox {
    var refs: [TS.AST.TypeReference]
    init(_ initial: [TS.AST.TypeReference]) { self.refs = initial }

    func merge(_ ref: TS.AST.TypeReference) -> Int {
        if let idx = refs.firstIndex(where: { $0.name == ref.name && $0.fileName == ref.fileName }) {
            return idx
        }
        refs.append(ref)
        return refs.count - 1
    }
}

// MARK: - Tests

final class InterfaceFlattenerTests: XCTestCase {

    // Child extends a single parent. Result: parent's members precede child's.
    func testSingleParentExtendsMergesMembers() throws {
        let parentInterface = makeInterface(name: "Parent",
                                            members: [makeMember(name: "a", type: makeType(name: "string"))])
        let parentDumped = makeDumpedInterface(name: "Parent", interface: parentInterface)
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parentDumped], references: [])
        ])

        let childRefs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [makeMember(name: "b", type: makeType(name: "number"))],
                                  supertypes: [makeSuper(refIndex: 0)])

        let flat = try InterfaceFlattener.flatten(interface: child,
                                                  fileName: "/proj/child.ts",
                                                  references: childRefs.refs,
                                                  mergeReference: { childRefs.merge($0) },
                                                  index: index)

        XCTAssertNil(flat.supertypes)
        XCTAssertEqual(flat.members.map { $0.name }, ["a", "b"])
    }

    // Grandparent -> Parent -> Child: deepest ancestor first, then intermediate, then child.
    func testMultiLevelExtendsChain() throws {
        let grandInterface = makeInterface(name: "Grand", members: [makeMember(name: "g", type: makeType(name: "string"))])
        let grand = makeDumpedInterface(name: "Grand", interface: grandInterface)

        let parentInterface = makeInterface(name: "Parent",
                                            members: [makeMember(name: "p", type: makeType(name: "string"))],
                                            supertypes: [makeSuper(refIndex: 0)])
        let parent = makeDumpedInterface(name: "Parent", interface: parentInterface)

        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/grand": InterfaceFlattenerSymbolEntry(dumpedSymbols: [grand], references: []),
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parent],
                                                          references: [TS.AST.TypeReference(name: "Grand", fileName: "/proj/grand.ts")])
        ])

        let childRefs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [makeMember(name: "c", type: makeType(name: "string"))],
                                  supertypes: [makeSuper(refIndex: 0)])

        let flat = try InterfaceFlattener.flatten(interface: child,
                                                  fileName: "/proj/child.ts",
                                                  references: childRefs.refs,
                                                  mergeReference: { childRefs.merge($0) },
                                                  index: index)

        XCTAssertEqual(flat.members.map { $0.name }, ["g", "p", "c"])
    }

    // Cycle: A extends B extends A. Must error, not stack-overflow.
    func testCycleIsRejected() {
        let aInterface = makeInterface(name: "A",
                                       members: [],
                                       supertypes: [makeSuper(refIndex: 0)])
        let aDumped = makeDumpedInterface(name: "A", interface: aInterface)

        let bInterface = makeInterface(name: "B",
                                       members: [],
                                       supertypes: [makeSuper(refIndex: 0)])
        let bDumped = makeDumpedInterface(name: "B", interface: bInterface)

        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/a": InterfaceFlattenerSymbolEntry(dumpedSymbols: [aDumped],
                                                     references: [TS.AST.TypeReference(name: "B", fileName: "/proj/b.ts")]),
            "/proj/b": InterfaceFlattenerSymbolEntry(dumpedSymbols: [bDumped],
                                                     references: [TS.AST.TypeReference(name: "A", fileName: "/proj/a.ts")])
        ])

        let entryRefs = RefBox([TS.AST.TypeReference(name: "B", fileName: "/proj/b.ts")])

        XCTAssertThrowsError(try InterfaceFlattener.flatten(interface: aInterface,
                                                            fileName: "/proj/a.ts",
                                                            references: entryRefs.refs,
                                                            mergeReference: { entryRefs.merge($0) },
                                                            index: index)) { error in
            XCTAssertTrue("\(error)".contains("Cyclic"), "expected cycle error, got: \(error)")
        }
    }

    // `implements` on an interface is rejected the same as before.
    func testImplementsClauseRejected() {
        let parent = makeDumpedInterface(name: "Parent",
                                         interface: makeInterface(name: "Parent", members: []))
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parent], references: [])
        ])

        let refs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [],
                                  supertypes: [makeSuper(refIndex: 0, isImplements: true)])

        XCTAssertThrowsError(try InterfaceFlattener.flatten(interface: child,
                                                            fileName: "/proj/child.ts",
                                                            references: refs.refs,
                                                            mergeReference: { refs.merge($0) },
                                                            index: index)) { error in
            XCTAssertTrue("\(error)".contains("implements"), "expected implements error, got: \(error)")
        }
    }

    // Parent not found in the index -> clear error.
    func testMissingParentRejected() {
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [:])
        let refs = RefBox([TS.AST.TypeReference(name: "Missing", fileName: "/proj/missing.ts")])
        let child = makeInterface(name: "Child",
                                  members: [],
                                  supertypes: [makeSuper(refIndex: 0)])

        XCTAssertThrowsError(try InterfaceFlattener.flatten(interface: child,
                                                            fileName: "/proj/child.ts",
                                                            references: refs.refs,
                                                            mergeReference: { refs.merge($0) },
                                                            index: index)) { error in
            XCTAssertTrue("\(error)".contains("resolve parent interface"), "expected missing-parent error, got: \(error)")
        }
    }

    // Parent resolves to a class-declaration symbol -> reject.
    func testClassParentRejected() {
        let parent = makeDumpedClass(name: "ParentClass",
                                     interface: makeInterface(name: "ParentClass", members: []))
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parent], references: [])
        ])
        let refs = RefBox([TS.AST.TypeReference(name: "ParentClass", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [],
                                  supertypes: [makeSuper(refIndex: 0)])

        XCTAssertThrowsError(try InterfaceFlattener.flatten(interface: child,
                                                            fileName: "/proj/child.ts",
                                                            references: refs.refs,
                                                            mergeReference: { refs.merge($0) },
                                                            index: index)) { error in
            XCTAssertTrue("\(error)".contains("only TypeScript interfaces"), "expected class-parent error, got: \(error)")
        }
    }

    // Duplicate member name between child and parent -> error under default policy.
    func testMemberNameCollisionRejected() {
        let parent = makeDumpedInterface(name: "Parent",
                                         interface: makeInterface(name: "Parent",
                                                                  members: [makeMember(name: "dup", type: makeType(name: "string"))]))
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parent], references: [])
        ])
        let refs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [makeMember(name: "dup", type: makeType(name: "number"))],
                                  supertypes: [makeSuper(refIndex: 0)])

        XCTAssertThrowsError(try InterfaceFlattener.flatten(interface: child,
                                                            fileName: "/proj/child.ts",
                                                            references: refs.refs,
                                                            mergeReference: { refs.merge($0) },
                                                            index: index)) { error in
            XCTAssertTrue("\(error)".contains("collides"), "expected collision error, got: \(error)")
        }
    }

    // `extends Parent<T>` is rejected in v1.
    func testGenericParentRejected() {
        let parent = makeDumpedInterface(name: "Parent",
                                         interface: makeInterface(name: "Parent", members: []))
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parent], references: [])
        ])
        let refs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [],
                                  supertypes: [makeSuper(refIndex: 0,
                                                         typeArguments: [TS.AST.TypeArgument(type: makeType(name: "T"))])])

        XCTAssertThrowsError(try InterfaceFlattener.flatten(interface: child,
                                                            fileName: "/proj/child.ts",
                                                            references: refs.refs,
                                                            mergeReference: { refs.merge($0) },
                                                            index: index)) { error in
            XCTAssertTrue("\(error)".contains("Generic parents"), "expected generic-parent error, got: \(error)")
        }
    }

    // Diamond inheritance: Child extends B and C; both B and C extend A.
    // A must be flattened only once (its members appear once, no false cycle error).
    func testDiamondInheritanceDedupes() throws {
        let aInterface = makeInterface(name: "A", members: [makeMember(name: "a", type: makeType(name: "string"))])
        let aDumped = makeDumpedInterface(name: "A", interface: aInterface)

        let bInterface = makeInterface(name: "B",
                                       members: [makeMember(name: "b", type: makeType(name: "string"))],
                                       supertypes: [makeSuper(refIndex: 0)])
        let bDumped = makeDumpedInterface(name: "B", interface: bInterface)

        let cInterface = makeInterface(name: "C",
                                       members: [makeMember(name: "c", type: makeType(name: "string"))],
                                       supertypes: [makeSuper(refIndex: 0)])
        let cDumped = makeDumpedInterface(name: "C", interface: cInterface)

        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/a": InterfaceFlattenerSymbolEntry(dumpedSymbols: [aDumped], references: []),
            "/proj/b": InterfaceFlattenerSymbolEntry(dumpedSymbols: [bDumped],
                                                     references: [TS.AST.TypeReference(name: "A", fileName: "/proj/a.ts")]),
            "/proj/c": InterfaceFlattenerSymbolEntry(dumpedSymbols: [cDumped],
                                                     references: [TS.AST.TypeReference(name: "A", fileName: "/proj/a.ts")])
        ])

        let childRefs = RefBox([
            TS.AST.TypeReference(name: "B", fileName: "/proj/b.ts"),
            TS.AST.TypeReference(name: "C", fileName: "/proj/c.ts")
        ])
        let child = makeInterface(name: "Child",
                                  members: [makeMember(name: "child", type: makeType(name: "string"))],
                                  supertypes: [makeSuper(refIndex: 0), makeSuper(refIndex: 1)])

        let flat = try InterfaceFlattener.flatten(interface: child,
                                                  fileName: "/proj/child.ts",
                                                  references: childRefs.refs,
                                                  mergeReference: { childRefs.merge($0) },
                                                  index: index)

        // Expected: A's members appear once (first branch B->A), then B's own, then C's own, then child's.
        XCTAssertEqual(flat.members.map { $0.name }, ["a", "b", "c", "child"])
    }

    // Property type carrying leadingComments must have them dropped after flattening so downstream
    // annotation extraction doesn't index into the child's fileContent with parent-file offsets.
    func testTranslatedTypeDropsLeadingComments() throws {
        let commented = TS.AST.Comments(text: "/** @Untyped */", start: 999999, end: 1000014)
        let parentMemberType = TS.AST.TSType(name: "string", leadingComments: commented)
        let parentMember = TS.AST.PropertyLikeDeclaration(start: 0, end: 0, name: "foo",
                                                         isOptional: false, type: parentMemberType,
                                                         leadingComments: nil)
        let parentInterface = TS.AST.Interface(start: 0, end: 0, name: "Parent",
                                               members: [parentMember], typeParameters: nil, supertypes: nil)
        let parentDumped = makeDumpedInterface(name: "Parent", interface: parentInterface)

        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parentDumped], references: [])
        ])
        let childRefs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [],
                                  supertypes: [makeSuper(refIndex: 0)])

        let flat = try InterfaceFlattener.flatten(interface: child,
                                                  fileName: "/proj/child.ts",
                                                  references: childRefs.refs,
                                                  mergeReference: { childRefs.merge($0) },
                                                  index: index)

        XCTAssertEqual(flat.members.count, 1)
        XCTAssertNil(flat.members[0].type.leadingComments,
                     "translated TSType must not retain parent-file leadingComments")
    }

    // Two files legitimately define an interface with the same name (e.g. `Error`) and one
    // extends the other. Cycle detection must key on (fileName, name), not name alone.
    func testSameNameParentsInDifferentFilesNotCycle() throws {
        // /proj/a/Error.ts declares `Error` with member `codeA`.
        let leafInterface = makeInterface(name: "Error",
                                          members: [makeMember(name: "codeA", type: makeType(name: "string"))])
        let leafDumped = makeDumpedInterface(name: "Error", interface: leafInterface)

        // /proj/b/Error.ts declares `Error` that extends the /proj/a version.
        let intermediateInterface = makeInterface(name: "Error",
                                                  members: [makeMember(name: "codeB", type: makeType(name: "string"))],
                                                  supertypes: [makeSuper(refIndex: 0)])
        let intermediateDumped = makeDumpedInterface(name: "Error", interface: intermediateInterface)

        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/a/Error": InterfaceFlattenerSymbolEntry(dumpedSymbols: [leafDumped], references: []),
            "/proj/b/Error": InterfaceFlattenerSymbolEntry(dumpedSymbols: [intermediateDumped],
                                                            references: [TS.AST.TypeReference(name: "Error", fileName: "/proj/a/Error.ts")])
        ])

        // Root child extends /proj/b/Error which in turn extends /proj/a/Error.
        let childRefs = RefBox([TS.AST.TypeReference(name: "Error", fileName: "/proj/b/Error.ts")])
        let child = makeInterface(name: "Child",
                                  members: [makeMember(name: "own", type: makeType(name: "string"))],
                                  supertypes: [makeSuper(refIndex: 0)])

        let flat = try InterfaceFlattener.flatten(interface: child,
                                                  fileName: "/proj/child.ts",
                                                  references: childRefs.refs,
                                                  mergeReference: { childRefs.merge($0) },
                                                  index: index)

        // No false cycle. Members from both Error files plus child's own.
        XCTAssertEqual(flat.members.map { $0.name }, ["codeA", "codeB", "own"])
    }

    // Parent member carrying a Valdi annotation (e.g. `@WorkerThread`) blocks flatten to prevent
    // silent semantic drift. Callers must opt into `ignoreInheritance: 'true'` or remove the tag.
    func testAnnotatedParentMemberRejected() {
        let annotatedMember = TS.AST.PropertyLikeDeclaration(
            start: 0, end: 0, name: "run", isOptional: false,
            type: makeType(name: "string"),
            leadingComments: TS.AST.Comments(text: "/** @WorkerThread */", start: 0, end: 20))
        let parentInterface = makeInterface(name: "Parent", members: [annotatedMember])
        let parentDumped = makeDumpedInterface(name: "Parent", interface: parentInterface)
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parentDumped], references: [])
        ])
        let refs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child", members: [], supertypes: [makeSuper(refIndex: 0)])

        XCTAssertThrowsError(try InterfaceFlattener.flatten(interface: child,
                                                            fileName: "/proj/child.ts",
                                                            references: refs.refs,
                                                            mergeReference: { refs.merge($0) },
                                                            index: index)) { error in
            XCTAssertTrue("\(error)".contains("Valdi annotation"),
                          "expected annotated-parent-member error, got: \(error)")
        }
    }

    // A non-Valdi comment on a parent member (e.g. jsdoc) must NOT trigger the guard.
    func testPlainJsdocOnParentMemberAllowed() throws {
        let jsdocMember = TS.AST.PropertyLikeDeclaration(
            start: 0, end: 0, name: "foo", isOptional: false,
            type: makeType(name: "string"),
            leadingComments: TS.AST.Comments(text: "/** Just a doc comment, no annotations. */", start: 0, end: 42))
        let parentInterface = makeInterface(name: "Parent", members: [jsdocMember])
        let parentDumped = makeDumpedInterface(name: "Parent", interface: parentInterface)
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parentDumped], references: [])
        ])
        let refs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child", members: [], supertypes: [makeSuper(refIndex: 0)])

        let flat = try InterfaceFlattener.flatten(interface: child,
                                                  fileName: "/proj/child.ts",
                                                  references: refs.refs,
                                                  mergeReference: { refs.merge($0) },
                                                  index: index)
        XCTAssertEqual(flat.members.map { $0.name }, ["foo"])
    }

    // Parent's property references a type via typeReferenceIndex; after flattening the
    // reference must be re-indexed against the child's reference table (and appended if missing).
    func testTypeReferenceIndexTranslation() throws {
        // Parent has: `foo: Bar` — Bar lives in /proj/bar.ts
        let parentInterface = makeInterface(name: "Parent",
                                            members: [makeMember(name: "foo",
                                                                 type: makeType(name: "", refIndex: 0))])
        let parentDumped = makeDumpedInterface(name: "Parent", interface: parentInterface)

        let parentRefs = [TS.AST.TypeReference(name: "Bar", fileName: "/proj/bar.ts")]
        let index = InterfaceFlattenerSymbolIndex(entriesByStrippedPath: [
            "/proj/parent": InterfaceFlattenerSymbolEntry(dumpedSymbols: [parentDumped], references: parentRefs)
        ])

        // Child only imports Parent (Bar is NOT already in the child's ref table).
        let childRefs = RefBox([TS.AST.TypeReference(name: "Parent", fileName: "/proj/parent.ts")])
        let child = makeInterface(name: "Child",
                                  members: [],
                                  supertypes: [makeSuper(refIndex: 0)])

        let flat = try InterfaceFlattener.flatten(interface: child,
                                                  fileName: "/proj/child.ts",
                                                  references: childRefs.refs,
                                                  mergeReference: { childRefs.merge($0) },
                                                  index: index)

        XCTAssertEqual(flat.members.count, 1)
        let member = flat.members[0]
        guard let newIndex = member.type.typeReferenceIndex else {
            return XCTFail("expected translated typeReferenceIndex on 'foo'")
        }
        XCTAssertEqual(childRefs.refs[newIndex].name, "Bar")
        XCTAssertEqual(childRefs.refs[newIndex].fileName, "/proj/bar.ts")
    }
}
