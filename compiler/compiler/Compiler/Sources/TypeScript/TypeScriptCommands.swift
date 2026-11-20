//
//  TypeScriptCommands.swift
//  Compiler
//
//  Created by Simon Corsin on 7/19/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct TS {

    struct AST {
        struct Comments: Codable {
            var text: String
            var start: Int
            var end: Int
        }

        struct EnumMember: Codable {
            var start: Int
            var end: Int
            var name: String
            var numberValue: String?
            var stringValue: String?
            var leadingComments: Comments?
        }

        struct Enum: Codable {
            var start: Int
            var end: Int
            var name: String
            var members: [EnumMember]
        }

        struct FunctionType: Codable {
            var parameters: [PropertyLikeDeclaration]
            var returnValue: TSType
        }

        struct TypeArgument: Codable {
            var type: TSType
        }

        class TSType: Codable {
            var name: String
            var leadingComments: Comments?
            var function: FunctionType?
            var unions: [TSType]?
            var intersections: [TSType]?
            var typeReferenceIndex: Int?
            var array: TSType?
            var typeArguments: [TypeArgument]?
            var isTypeParameter: Bool?

            init(name: String, leadingComments: Comments? = nil, function: FunctionType? = nil, unions: [TSType]? = nil, intersections: [TSType]? = nil, typeReferenceIndex: Int? = nil, array: TSType? = nil, typeArguments: [TypeArgument]? = nil, isTypeParameter: Bool? = nil) {
                self.name = name
                self.leadingComments = leadingComments
                self.function = function
                self.unions = unions
                self.intersections = intersections
                self.typeReferenceIndex = typeReferenceIndex
                self.array = array
                self.typeArguments = typeArguments
                self.isTypeParameter = isTypeParameter
            }
        }

        struct PropertyLikeDeclaration: Codable {
            var start: Int
            var end: Int
            var name: String
            var isOptional: Bool
            var type: TSType
            var leadingComments: Comments?
        }

        struct TypeParameterLikeDeclaration: Codable {
            var start: Int
            var end: Int
            var name: String
        }

        struct SuperTypeClause: Codable {
            var start: Int
            var end: Int
            var isImplements: Bool
            var type: TSType
        }

        struct Interface: Codable {
            var start: Int
            var end: Int
            var name: String
            var members: [PropertyLikeDeclaration]
            var typeParameters: [TypeParameterLikeDeclaration]?
            var supertypes: [SuperTypeClause]?
        }

        struct Variable: Codable {
            var start: Int
            var end: Int
            var name: String
            var type: TSType
            var leadingComments: Comments?
        }

        struct Function: Codable {
            var start: Int
            var end: Int
            var name: String
            var type: FunctionType
            var leadingComments: Comments?
        }

        struct TypeReference: Codable {
            var name: String
            var fileName: String
        }
    }

    enum MessageType: String {
        case request = "request"
        case response = "response"
        case event = "event"
    }

    struct Request<T: Codable>: Codable {

        var type = "request"

        var seq: Int = 0
        var command: String?

        var arguments: T?

        init(command: String, arguments: T?) {
            self.command = command
            self.arguments = arguments
        }
    }

    struct Response<T: Codable>: Codable {

        var type: String
        var requestSeq: Int
        var success: Bool
        var command: String
        var message: String?

        var body: T
    }

    /**
     * Server-initiated event message
     */
    struct Event<T: Codable>: Codable {

        /**
         * Name of event
         */
        var event: String

        /**
         * Event-specific information
         */
        var body: T
    }

    // Specific requests and responses

    struct StatusResponseBody: Codable {

        var version: String

    }

    struct OpenRequestBody: Codable {

        /**
         * The file for the request (absolute pathname required).
         */
        var file: String

        var fileContent: String?
        var scriptKindName: String
        var projectRootPath: String
        var projectFileName: String?

    }

    struct TextSpan: Codable {
        /**
         * First character of the definition.
         */
        var start: Location?

        /**
         * One character past last character of the definition.
         */
        var end: Location?
    }

    struct SimpleTextSpan: Codable {
        var start: Int?
        var length: Int?
    }

    struct CompileOnSaveEmitFileRequestArgs: Codable {
        /**
         * The file for the request (absolute pathname required).
         */
        var file: String

        /**
         * if true - then file should be recompiled even if it does not have any changes.
         */
        var forced: Bool
    }

    /**
     * Arguments for geterr messages.
     */
    struct GeterrRequestArgs: Codable {
        /**
         * List of file names for which to compute compiler errors.
         * The files will be checked in list order.
         */
        var files: [String]

        /**
         * Delay in milliseconds to wait before starting to compute
         * errors for the files in the file list
         */
        var delay: Int
    }

    struct DiagnosticEventBody: Codable {
        /**
         * The file for which diagnostic information is reported.
         */
        var file: String
        /**
         * An array of diagnostic information items.
         */
        var diagnostics: [Diagnostic]
    }

    /**
     * Item of diagnostic information found in a DiagnosticEvent message.
     */
    struct Diagnostic: Codable {
        /**
         * Starting file location at which text applies.
         */
        var start: Location
        /**
         * The last file location at which the text applies.
         */
        var end: Location
        /**
         * Text of diagnostic message.
         */
        var text: String
        /**
         * The category of the diagnostic message, e.g. "error", "warning", or "suggestion".
         */
        var category: String

        /**
         * Any related spans the diagnostic may have, such as other locations relevant to an error, such as declarartion sites
         */
        var relatedInformation: [DiagnosticRelatedInformation]?
        /**
         * The error code of the diagnostic message.
         */
        var code: Int?
        /**
         * The name of the plugin reporting the message.
         */
        var source: String?
    }

    /**
     * Location in source code expressed as (one-based) line and (one-based) column offset.
     */
    struct Location: Codable {
        var line: Int
        var offset: Int
    }

//    /**
//     * Object found in response messages defining a span of text in source code.
//     */
//    struct TextSpan: Codable {
//        /**
//         * First character of the definition.
//         */
//        var start: Location
//        /**
//         * One character past last character of the definition.
//         */
//        var end: Location
//    }

    struct FileRequestArgs: Codable {
        var file: String?
        var projectFileName: String?
    }

    struct DiagnosticsSyncRequestArgs: Codable {
        var file: String?
        var projectFileName: String?
        var includeLinePosition: Bool?
    }

    struct FileLocationRequestArgs: Codable {

        var file: String?
        var projectFileName: String?

        /**
         * The line number for the request (1-based).
         */
        var line: Int?
        /**
         * The character offset (on the line) for the request (1-based).
         */
        var offset: Int?

        /**
         The character position
         */
        var position: Int?
    }

    /**
     * Object found in response messages defining a span of text in source code.
     */
    struct FileSpan: Codable {

        /**
         * First character of the definition.
         */
        var start: Location?

        /**
         * One character past last character of the definition.
         */
        var end: Location?

        /**
         * File containing text span.
         */
        var file: String?
    }

    struct SimpleFileSpan: Codable {

        /**
         * First character of the definition.
         */
        var start: Int?

        /**
         * One character past last character of the definition.
         */
        var end: Int?

        /**
         * File containing text span.
         */
        var file: String?
    }

    struct TypeDefinition: Codable {
        var fileName: String
        var textSpan: SimpleTextSpan
        var kind: ScriptElementKind
        var name: String
    }

    enum OutliningSpanKind: String, Codable {
        /** Single or multi-line comments */
        case comment = "comment"
        /** Sections marked by '// #region' and '// #endregion' comments */
        case region = "region"
        /** Declarations and expressions */
        case code = "code"
        /** Contiguous blocks of import declarations */
        case imports = "imports"
    }

    struct OutliningSpan: Codable {
        /** The span of the document to actually collapse. */
        var textSpan: TextSpan?
        /** The span of the document to display when the user hovers over the collapsed span. */
        var hintSpan: TextSpan?
        /** The text to display in the editor for the collapsed region. */
        var bannerText: String?
        /**
         * Whether or not this region should be automatically collapsed when
         * the 'Collapse to Definitions' command is invoked.
         */
        var autoCollapse: Bool?
        /**
         * Classification of the contents of the span
         */
        var kind: OutliningSpanKind?
    }

    struct DefinitionInfo: Codable {

        /**
         * First character of the definition.
         */
        var start: Int?

        /**
         * One character past last character of the definition.
         */
        var end: Int?

        /**
         * File containing text span.
         */
        var file: String?

        var kind: ScriptElementKind
        var name: String
        var containerKind: ScriptElementKind?
        var containerName: String?
    }

    struct DefinitionInfoAndBoundSpan: Codable {
        var definitions: [DefinitionInfo]?
        var textSpan: SimpleTextSpan?
    }    

    struct OpenFileImportPath: Codable {
        var relative: String
        var absolute: String
    }

    struct OpenResponseBody: Codable {
        var importPaths: [OpenFileImportPath]
    }

    struct RegisterFileResponseBody: Codable {        
    }

    struct EmittedFile: Codable {
        var fileName: String
        var content: String
    }

    /**
     * Body of QuickInfoResponse.
     */
    struct QuickInfoResponseBody: Codable {
        /**
         * The symbol's kind (such as 'className' or 'parameterName' or plain 'text').
         */
        var kind: ScriptElementKind?

        var textSpan: SimpleTextSpan?

        /**
         * Optional modifiers for the kind (such as 'public').
         */
        var kindModifiers: String?

        var displayParts: [SymbolDisplayPart]?
        var documentation: [SymbolDisplayPart]?

        /**
         * JSDoc tags associated with symbol.
         */
        var tags: [JSDocTagInfo]?
    }

    enum DumpedSymbolNodeType: String, Codable {
        case `enum`
        case function
        case interface
        case exportedVariable
    }

    struct DumpedSymbolWithComments: Codable {
        var nodeType: String
        var start: Int

        var leadingComments: AST.Comments?
        var text: String
        var kind: SyntaxKind
        var modifiers: [String]?

        var function: AST.Function?
        var `enum`: AST.Enum?
        var interface: AST.Interface?
        var variable: AST.Variable?
        var exportedTypeAlias: String?

        var kindModifiers: String? {
            return modifiers?.joined(separator: " ") // FIXME: just leave it as an array
        }
    }

    struct DumpSymbolsWithCommentsResponseBody: Codable {
        var dumpedSymbols: [DumpedSymbolWithComments]
        var references: [AST.TypeReference]
    }

    struct DumpInterfaceResponseBody: Codable {
        var interface: AST.Interface
        var references: [AST.TypeReference]
    }

    struct DumpEnumResponseBody: Codable {
        var `enum`: AST.Enum
    }

    struct DumpFunctionResponseBody: Codable {
        var function: AST.Function
        var references: [AST.TypeReference]
    }

    struct NavigationBarItem: Codable {

        /**
         * The item's display text.
         */
        var text: String?

        /**
         * The symbol's kind (such as 'className' or 'parameterName').
         */
        var kind: ScriptElementKind?

        /**
         * Optional modifiers for the kind (such as 'public').
         */
        var kindModifiers: String?

        /**
         * The definition locations of the item.
         */
        var spans: [TextSpan]?

        /**
         * Optional children.
         */
        var childItems: [NavigationBarItem]?

        /**
         * Number of levels deep this item should appear.
         */
        var indent: Int?
    }

    /** protocol.NavigationTree is identical to ts.NavigationTree, except using protocol.TextSpan instead of ts.TextSpan */
    struct NavigationTree: Codable {
        var text: String?
        var kind: ScriptElementKind?
        var kindModifiers: String?
        var spans: [SimpleTextSpan]?
        var nameSpan: SimpleTextSpan?
        var childItems: [NavigationTree]?
    }

    /**
     * Part of a symbol description.
     */
    struct SymbolDisplayPart: Codable {
        /**
         * Text of an item describing the symbol.
         */
        var text: String?
        /**
         * The symbol's kind (such as 'className' or 'parameterName' or plain 'text').
         */
        var kind: String?
    }

    /**
     * Signature help information for a single parameter
     */
    struct SignatureHelpParameter: Codable {

        /**
         * The parameter's name
         */
        var name: String?
        /**
         * Documentation of the parameter.
         */
        var documentation: [SymbolDisplayPart]?
        /**
         * Display parts of the parameter.
         */
        var displayParts: [SymbolDisplayPart]?
        /**
         * Whether the parameter is optional or not.
         */
        var isOptional: Bool?
    }

    /**
     * Represents a single signature to show in signature help.
     */
    struct SignatureHelpItem: Codable {
        /**
         * Whether the signature accepts a variable number of arguments.
         */
        var isVariadic: Bool?
        /**
         * The prefix display parts.
         */
        var prefixDisplayParts: [SymbolDisplayPart]?
        /**
         * The suffix display parts.
         */
        var suffixDisplayParts: [SymbolDisplayPart]?
        /**
         * The separator display parts.
         */
        var separatorDisplayParts: [SymbolDisplayPart]?
        /**
         * The signature helps items for the parameters.
         */
        var parameters: [SignatureHelpParameter]?
        /**
         * The signature's documentation
         */
        var documentation: [SymbolDisplayPart]?

    }

    /**
     * Signature help items found in the response of a signature help request.
     */
    struct SignatureHelpItems: Codable {
        /**
         * The signature help items.
         */
        var items: [SignatureHelpItem]?
        /**
         * The span for which signature help should appear on a signature
         */
        var applicableSpan: TextSpan?
        /**
         * The item selected in the set of available help items.
         */
        var selectedItemIndex: Int?
        /**
         * The argument selected in the set of parameters.
         */
        var argumentIndex: Int?
        /**
         * The argument count
         */
        var argumentCount: Int?
    }

    /**
     * Arguments of a signature help request.
     */
    struct SignatureHelpRequestArgs: Codable {

        var file: String?
        var projectFileName: String?

        /**
         * The line number for the request (1-based).
         */
        var line: Int?
        /**
         * The character offset (on the line) for the request (1-based).
         */
        var offset: Int?

        /**
         The character position
         */
        var position: Int?

        /**
         * Reason why signature help was invoked.
         * See each individual possible
         */
        var triggerReason: [String: String]?
    }

    struct ImplementationLocation: Codable {
        var kind: ScriptElementKind
        var displayParts: [SymbolDisplayPart]
    }

    enum SyntaxKind: Int, Codable {
        case unknown = 0
        case variableStatement = 243
        case functionDeclaration = 262
        case classDeclaration = 263
        case interfaceDeclaration = 264
        case typeAliasDeclaration = 265
        case enumDeclaration = 266
    }

    enum ScriptElementKind: String, Codable {
        case unknown = ""
        case warning = "warning"
        /** predefined type (void) or keyword (class) */
        case keyword = "keyword"
        /** top level script node */
        case scriptElement = "script"
        /** module foo {} */
        case moduleElement = "module"
        /** class X {} */
        case classElement = "class"
        /** var x = class X {} */
        case localClassElement = "local class"
        /** interface Y {} */
        case interfaceElement = "interface"
        /** type T = ... */
        case typeElement = "type"
        /** enum E */
        case enumElement = "enum"
        case enumMemberElement = "enum member"
        /**
         * Inside module and script only
         * const v = ..
         */
        case variableElement = "var"
        /** Inside function */
        case localVariableElement = "local var"
        /**
         * Inside module and script only
         * function f() { }
         */
        case functionElement = "function"
        /** Inside function */
        case localFunctionElement = "local function"
        /** class X { [public|private]* foo() {} } */
        case memberFunctionElement = "method"
        /** class X { [public|private]* [get|set] foo:number; } */
        case memberGetAccessorElement = "getter"
        case memberSetAccessorElement = "setter"
        /**
         * class X { [public|private]* foo:number; }
         * interface Y { foo:number; }
         */
        case memberVariableElement = "property"
        /** class X { constructor() { } } */
        case constructorImplementationElement = "constructor"
        /** interface Y { ():number; } */
        case callSignatureElement = "call"
        /** interface Y { []:number; } */
        case indexSignatureElement = "index"
        /** interface Y { new():Y; } */
        case constructSignatureElement = "construct"
        /** function foo(*Y*: string) */
        case parameterElement = "parameter"
        case typeParameterElement = "type parameter"
        case primitiveType = "primitive type"
        case label = "label"
        case alias = "alias"
        case constElement = "const"
        case letElement = "let"
        case directory = "directory"
        case externalModuleName = "external module name"
        /**
         * <JsxTagName attribute1 attribute2={0} />
         */
        case jsxAttribute = "JSX attribute"
        /** String literal */
        case string = "string"
    }

    /**
     * Represents additional spans returned with a diagnostic which are relevant to it
     */
    struct DiagnosticRelatedInformation: Codable {
        /**
         * The category of the related information message, e.g. "error", "warning", or "suggestion".
         */
        var category: String
        /**
         * The code used ot identify the related information
         */
        var code: Int
        /**
         * Text of related or additional information.
         */
        var message: String
        /**
         * Associated location
         */
        var span: FileSpan?
    }

    /**
     * Arguments for reload request.
     */
    struct ReloadRequestArgs: Codable {
        /**
         * The file for the request (absolute pathname required).
         */
        var file: String

        var projectFileName: String?

        /**
         * Name of temporary file from which to reload file
         * contents. May be same as file.
         */
        var tmpfile: String
    }

    /**
     * An item found in a completion response.
     */
    struct CompletionEntry: Codable {
        /**
         * The symbol's name.
         */
        var name: String?
        /**
         * The symbol's kind (such as 'className' or 'parameterName').
         */
        var kind: ScriptElementKind?
        /**
         * Optional modifiers for the kind (such as 'public').
         */
        var kindModifiers: String?
        /**
         * A string that is used for comparing completion items so that they can be ordered.  This
         * is often the same as the name but may be different in certain circumstances.
         */
        var sortText: String
        /**
         * Text to insert instead of `name`.
         * This is used to support bracketed completions; If `name` might be "a-b" but `insertText` would be `["a-b"]`,
         * coupled with `replacementSpan` to replace a dotted access with a bracket access.
         */
        var insertText: String?
        /**
         * An optional span that indicates the text to be replaced by this completion item.
         * If present, this span should be used instead of the default one.
         * It will be set if the required span differs from the one generated by the default replacement behavior.
         */
        var replacementSpan: TextSpan?
        /**
         * Indicates whether commiting this completion entry will require additional code actions to be
         * made to avoid errors. The CompletionEntryDetails will have these actions.
         */
        var hasAction: Bool?
        /**
         * Identifier (not necessarily human-readable) identifying where this completion came from.
         */
        var source: String?
        /**
         * If true, this completion should be highlighted as recommended. There will only be one of these.
         * This will be set when we know the user should write an expression with a certain type and that type is an enum or constructable class.
         * Then either that enum/class or a namespace containing it will be the recommended symbol.
         */
        var isRecommended: Bool?
    }

    struct JSDocTagInfo: Codable {
        var name: String
        var text: String?
    }

    struct CodeAction: Codable {
//    /** Description of the code action to display in the UI of the editor */
//    var description: string;
//    /** Text changes to apply to each file as part of the code action */
//    var changes: FileCodeEdit
//    /** A command is an opaque object that should be passed to `ApplyCodeActionCommandRequestArgs` without modification.  */
//    commands?: {}[];
    }

    /**
     * Additional completion entry details, available on demand
     */
    struct CompletionEntryDetails: Codable {
        /**
         * The symbol's name.
         */
        var name: String
        /**
         * The symbol's kind (such as 'className' or 'parameterName').
         */
        var kind: ScriptElementKind
        /**
         * Optional modifiers for the kind (such as 'public').
         */
        var kindModifiers: String
        /**
         * Display parts of the symbol (similar to quick info).
         */
        var displayParts: [SymbolDisplayPart]
        /**
         * Documentation strings for the symbol.
         */
        var documentation: [SymbolDisplayPart]?
        /**
         * JSDoc tags for the symbol.
         */
        var tags: [JSDocTagInfo]?
        /**
         * The associated code actions for this entry
         */
        var codeActions: [CodeAction]?
        /**
         * Human-readable description of the `source` from the CompletionEntry.
         */
        var source: [SymbolDisplayPart]?
    }

    struct CompletionInfo: Codable {
        var isGlobalCompletion: Bool
        var isMemberCompletion: Bool
        var isNewIdentifierLocation: Bool
        var entries: [CompletionEntry]
    }

    /**
     * Represents a file in external project.
     * External project is project whose set of files, compilation options and open\close state
     * is maintained by the client (i.e. if all this data come from .csproj file in Visual Studio).
     * External project will exist even if all files in it are closed and should be closed explicitly.
     * If external project includes one or more tsconfig.json/jsconfig.json files then tsserver will
     * create configured project for every config file but will maintain a link that these projects were created
     * as a result of opening external project so they should be removed once external project is closed.
     */
    struct ExternalFile: Codable {
        /**
         * Name of file file
         */
        var fileName: String
        /**
         * Script kind of the file
         */
        var scriptKind: String?
        /**
         * Whether file has mixed content (i.e. .cshtml file that combines html markup with C#/JavaScript)
         */
        var hasMixedContent: Bool?
        /**
         * Content of the file
         */
        var content: String?
    }

    struct ExternalProject: Codable {
        /**
         * Project name
         */
        var projectFileName: String
        /**
         * List of root files in project
         */
        var rootFiles: [ExternalFile]
        /**
         * Compiler options for the project
         */
        var options: CompilerOptions

        /**
         * Explicitly specified type acquisition for the project
         */
        var typeAcquisition: TypeAcquisition?
    }

    struct TypeAcquisition: Codable {
        var enableAutoDiscovery: Bool
        var enable: Bool?
        var include: [String]?
        var exclude: [String]?
    }

    struct PluginImport: Codable {
        var name: String
    }

    struct ProjectReference: Codable {
        /** A normalized path on disk */
        var path: String
        /** The path as the user originally wrote it */
        var originalPath: String?
        /** True if the output of this reference should be prepended to the output of this project. Only valid for --outFile compilations */
        var prepend: Bool?
        /** True if it is intended that this reference form a circularity */
        var circular: Bool?
    }

    struct CompilerOptions: Codable {
        var allowJs: Bool?
        var allowSyntheticDefaultImports: Bool?
        var allowUnreachableCode: Bool?
        var allowUnusedLabels: Bool?
        var alwaysStrict: Bool?
        var baseUrl: String?
        var charset: String?
        var checkJs: Bool?
        var declaration: Bool?
        var declarationDir: String?
        var disableSizeLimit: Bool?
        var downlevelIteration: Bool?
        var emitBOM: Bool?
        var emitDecoratorMetadata: Bool?
        var experimentalDecorators: Bool?
        var forceConsistentCasingInFileNames: Bool?
        var importHelpers: Bool?
        var inlineSourceMap: Bool?
        var inlineSources: Bool?
        var isolatedModules: Bool?
        var jsx: String?
        var lib: [String]?
        var locale: String?
        var mapRoot: String?
        var maxNodeModuleJsDepth: Int?
        var module: String?
        var moduleResolution: String?
        var newLine: String?
        var noEmit: Bool?
        var noEmitHelpers: Bool?
        var noEmitOnError: Bool?
        var noErrorTruncation: Bool?
        var noFallthroughCasesInSwitch: Bool?
        var noImplicitAny: Bool?
        var noImplicitReturns: Bool?
        var noImplicitThis: Bool?
        var noUnusedLocals: Bool?
        var noUnusedParameters: Bool?
        var noImplicitUseStrict: Bool?
        var noLib: Bool?
        var noResolve: Bool?
        var out: String?
        var outDir: String?
        var outFile: String?
        var paths: [String: [String]]?
        var plugins: [PluginImport]?
        var preserveConstEnums: Bool?
        var preserveSymlinks: Bool?
        var project: String?
        var reactNamespace: String?
        var removeComments: Bool?
        var references: [ProjectReference]?
        var rootDir: String?
        var rootDirs: [String]?
        var skipLibCheck: Bool?
        var skipDefaultLibCheck: Bool?
        var sourceMap: Bool?
        var sourceRoot: String?
        var strict: Bool?
        var strictNullChecks: Bool?
        var suppressExcessPropertyErrors: Bool?
        var suppressImplicitAnyIndexErrors: Bool?
        var target: String?
        var traceResolution: Bool?
        var resolveJsonModule: Bool?
        var types: [String]?
        /** Paths used to used to compute primary types search locations */
        var typeRoots: [String]?

        init() {

        }
    }

    struct Empty: Codable {

    }

    /**
     * Arguments for ProjectInfoRequest request.
     */
    struct ProjectInfoRequestArgs: Codable {
        /**
         * The file for the request (absolute pathname required).
         */
        var file: String?
        var projectFileName: String?
        /**
         * Indicate if the file name list of the project is needed
         */
        var needFileNameList: Bool
    }

    /**
     * Response message body for "projectInfo" request
     */
    struct ProjectInfo: Codable {
        /**
         * For configured project, this is the normalized path of the 'tsconfig.json' file
         * For inferred project, this is undefined
         */
        var configFileName: String
        /**
         * The list of normalized file name in the project, including 'lib.d.ts'
         */
        var fileNames: [String]?
        /**
         * Indicates if the project has a active language service instance
         */
        var languageServiceDisabled: Bool?
    }

    struct TextInsertion: Codable {
        var newText: String?
        /** The position in newText the caret should point to after the insertion. */
        var caretOffset: Int?
    }

    struct Config: Codable {

        var extends: String?
        var compilerOptions: CompilerOptions?
        var files: [String]?
        var include: [String]?
        var exclude: [String]?

    }

}

private func doParse<T: Codable>(type: T.Type, data: Data) throws -> T {
    let decoder = Foundation.JSONDecoder()
    decoder.keyDecodingStrategy = .convertFromSnakeCase
    return try decoder.decode(type, from: data)
}

extension TS.Response {

    static func parse(from data: Data) throws -> TS.Response<T> {
        return try doParse(type: self, data: data)
    }

}

extension TS.Event {

    static func parse(from data: Data) throws -> TS.Event<T> {
        return try doParse(type: self, data: data)
    }

}

extension TS.Config {
    static func parse(from data: Data) throws -> TS.Config {
        return try doParse(type: self, data: data)
    }
}
