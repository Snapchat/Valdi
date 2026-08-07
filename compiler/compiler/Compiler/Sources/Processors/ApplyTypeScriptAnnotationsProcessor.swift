//
//  ApplyTypeScriptAnnotationsProcessor.swift
//  Compiler
//
//  Created by saniul on 13/06/2019.
//

import Foundation

final class ApplyTypeScriptAnnotationsProcessor: CompilationProcessor {

    var description: String {
        return "Processing TypeScript Annotations"
    }

    private let logger: ILogger
    private let typeScriptCompilerManager: TypeScriptCompilerManager
    private let typeScriptAnnotationsManager: TypeScriptAnnotationsManager
    private let nativeCodeGenerationManager: NativeCodeGenerationManager

    init(logger: ILogger, typeScriptCompilerManager: TypeScriptCompilerManager, typeScriptAnnotationsManager: TypeScriptAnnotationsManager, nativeCodeGenerationManager: NativeCodeGenerationManager) {
        self.logger = logger
        self.typeScriptCompilerManager = typeScriptCompilerManager
        self.typeScriptAnnotationsManager = typeScriptAnnotationsManager
        self.nativeCodeGenerationManager = nativeCodeGenerationManager
    }

    func extractErrors(items: [CompilationItem]) -> [URL: Error] {
        return items.compactMap { (item) -> (CompilationItem, Error)? in
            if case .error(let error, _) = item.kind {
                return (item, error)
            }
            return nil
            }.associate { (item, error) -> (URL, Error) in
                return (item.sourceURL, error)
        }
    }

    private func hasComponentAnnotation(parsedAnnotation: ParsedAnnotation) -> Bool {
        for annotation in parsedAnnotation.symbol.annotations {
            if let annotationType = ValdiAnnotationType(rawValue: annotation.name), annotationType == .component {
                return true
            }
        }
        return false
    }

    /// Fails fast when the same Valdi native-export annotation (e.g. `@ExportModel`) is attached more than once to one symbol.
    /// Common cause: text such as "due to @ExportModel limitations" — the scanner treats any `@ExportModel` substring as a real tag.
    private func throwIfDuplicateConflictingNativeExportAnnotations(_ allAnnotations: [(URL, ParsedAnnotation)]) throws {
        struct Key: Hashable {
            let filePath: String
            let symbol: String
            let annotationType: ValdiAnnotationType
        }
        var buckets: [Key: [(URL, ParsedAnnotation)]] = [:]
        for (url, pa) in allAnnotations {
            switch pa.type {
            case .exportModel, .generateNativeClass, .exportProxy, .generateNativeInterface, .exportEnum, .generativeNativeEnum, .exportFunction, .generativeNativeFunction:
                let key = Key(filePath: url.standardizedFileURL.path, symbol: pa.symbol.symbol.text, annotationType: pa.type)
                buckets[key, default: []].append((url, pa))
            default:
                break
            }
        }
        for (key, group) in buckets where group.count > 1 {
            let atTag = "@\(key.annotationType.rawValue)"
            var lines: [String] = []
            for (index, pair) in group.enumerated() {
                let (_, pa) = pair
                let text = pa.annotation.content.replacingOccurrences(of: "\n", with: " ")
                lines.append("\(index + 1). \(text)")
            }
            let proseHint = "Text in the same block that contains the substring \"\(atTag)\" (for example, \"due to \(atTag) limitations\") is parsed as a second export and must be removed. Only one real \(atTag) annotation may apply to this symbol."
            throw CompilerError(
                "Multiple \(atTag) annotations have been detected for \(key.symbol) in file \(key.filePath). Only one can be used and duplicates must be removed.\n\(lines.joined(separator: "\n"))\n\n\(proseHint)"
            )
        }
    }

    private func processAnnotations(items: [CompilationItem]) throws -> [CompilationItem] {
        let out = DocumentsIndexer(items: items)

        nativeCodeGenerationManager.clear()

        // Build the parent-interface lookup index before annotation processing so that
        // `InterfaceFlattener` can resolve `extends` chains across files during
        // `addNativeTypeToGenerate`.
        nativeCodeGenerationManager.setInterfaceFlattenerIndex(buildInterfaceFlattenerIndex(items: items))

        let allAnnotations = typeScriptAnnotationsManager.listAllAnnotations()
        try throwIfDuplicateConflictingNativeExportAnnotations(allAnnotations)
        for (sourceURL, parsedAnnotation) in allAnnotations {
            let commentedFile = parsedAnnotation.file
            let linesIndexer = commentedFile.linesIndexer
            let annotatedSymbol = parsedAnnotation.symbol
            let symbol = annotatedSymbol.symbol
            let annotation = parsedAnnotation.annotation
            let compilationItem = parsedAnnotation.compilationItem

            var documentAndIndex = out.findDocument(fromSourceURL: sourceURL)

            let isTSorTSX = !sourceURL.path.hasSuffix(".vue.ts")
                && !sourceURL.path.hasSuffix(".vue.tsx")
                && (sourceURL.path.hasSuffix(".ts") || sourceURL.path.hasSuffix(".tsx"))

            do {
                switch parsedAnnotation.type {
                case .exportModule:
                    try nativeCodeGenerationManager.addNativeModuleToGenerate(commentedFile: commentedFile, annotation: annotation, sourceURL: sourceURL, annotatedSymbol: annotatedSymbol, compilationItem: compilationItem, linesIndexer: linesIndexer)
                case .exportModel, .generateNativeClass:
                    let isComponent = hasComponentAnnotation(parsedAnnotation: parsedAnnotation)
                    try nativeCodeGenerationManager.addNativeTypeToGenerate(commentedFile: commentedFile, annotation: annotation, sourceURL: sourceURL, annotatedSymbol: annotatedSymbol, compilationItem: compilationItem, linesIndexer: linesIndexer, kind: .class, isComponent: isComponent)
                case .exportProxy, .generateNativeInterface:
                    try nativeCodeGenerationManager.addNativeTypeToGenerate(commentedFile: commentedFile, annotation: annotation, sourceURL: sourceURL, annotatedSymbol: annotatedSymbol, compilationItem: compilationItem, linesIndexer: linesIndexer, kind: .interface, isComponent: false)
                case .exportEnum, .generativeNativeEnum:
                    guard let dumpedEnum = symbol.enum else {
                        try throwAnnotationError(annotation, commentedFile, message: "[Shouldn't happen] Processing a @GenerateNativeEnum annotation, but there is no enum information")
                    }

                    let members = dumpedEnum.members
                    let stringMembers = members.filter { $0.stringValue != nil }

                    let anyContainQuotes = !stringMembers.isEmpty
                    let allContainQuotes = stringMembers.count == members.count

                    if anyContainQuotes != allContainQuotes {
                        try throwAnnotationError(annotation, commentedFile, message: "Invalid enum '\(symbol.text)' - Can't mix String and Int cases")
                    }

                    let kind: NativeTypeKind = anyContainQuotes ? .stringEnum : .enum

                    // TODO: Support non-generated enums
                    try nativeCodeGenerationManager.addNativeTypeToGenerate(commentedFile: commentedFile, annotation: annotation, sourceURL: sourceURL, annotatedSymbol: annotatedSymbol, compilationItem: compilationItem, linesIndexer: linesIndexer, kind: kind, isComponent: false)
                case .exportFunction, .generativeNativeFunction:
                    try nativeCodeGenerationManager.addNativeFuncToGenerate(commentedFile: commentedFile, annotation: annotation, sourceURL: sourceURL, annotatedSymbol: annotatedSymbol, compilationItem: compilationItem, linesIndexer: linesIndexer)
                case .nativeTypeConverter:
                    try nativeCodeGenerationManager.addNativeTypeConverter(commentedFile: commentedFile, annotation: annotation, sourceURL: sourceURL, annotatedSymbol: annotatedSymbol, compilationItem: compilationItem, linesIndexer: linesIndexer)
                case .nativeClass:
                    let nativeClass = try nativeCodeGenerationManager.registerNativeClass(commentedFile: commentedFile, annotation: annotation, symbol: symbol, shouldGenerateIOS: compilationItem.shouldOutputToIOS, shouldGenerateAndroid: compilationItem.shouldOutputToAndroid, kind: .class, bundleInfo: compilationItem.bundleInfo, isGenerated: false)
                    let description = nativeCodeGenerationManager.nativeTypeDescription(
                        annotatedSymbol: annotatedSymbol,
                        nativeClass: nativeClass,
                        compilationItem: compilationItem
                    )
                    out.append(item: compilationItem.with(
                        newKind: .generatedTypeDescription(
                            description,
                            src: commentedFile.src
                        ),
                        newPlatform: .none
                    ))
                case .nativeInterface:
                    let nativeClass = try nativeCodeGenerationManager.registerNativeClass(commentedFile: commentedFile, annotation: annotation, symbol: symbol, shouldGenerateIOS: compilationItem.shouldOutputToIOS, shouldGenerateAndroid: compilationItem.shouldOutputToAndroid, kind: .interface, bundleInfo: compilationItem.bundleInfo, isGenerated: false)
                    let description = nativeCodeGenerationManager.nativeTypeDescription(
                        annotatedSymbol: annotatedSymbol,
                        nativeClass: nativeClass,
                        compilationItem: compilationItem
                    )
                    out.append(item: compilationItem.with(
                        newKind: .generatedTypeDescription(
                            description,
                            src: commentedFile.src
                        ),
                        newPlatform: .none
                    ))
                case .component:
                    if isTSorTSX {
                        guard let symbolName = symbol.text.nonEmpty else {
                            try throwAnnotationError(annotation, commentedFile, message: "Couldn't get the JS symbol name for @Component")
                        }
                        // Hacking support for @Component in .tsx files
                        var docAndIndex = out.findOrCreateDocument(fromSourceURL: sourceURL, compilationItem: compilationItem)
                        let fileName = (compilationItem.relativeProjectPath as NSString).deletingPathExtension
                        docAndIndex.compilationResult.componentPath = ComponentPath(fileName: fileName, exportedMember: symbolName)
                        documentAndIndex = docAndIndex
                    } else {
                        guard documentAndIndex != nil else {
                            try throwAnnotationError(annotation, commentedFile, message: "@Component must be set in a .vue, .ts, or .tsx file")
                        }
                    }
                    try processComponent(sourceURL: sourceURL, commentedFile: commentedFile, annotation: annotation, symbol: symbol, document: &documentAndIndex!.compilationResult.originalDocument)
                case .action:
                    if isTSorTSX {
                        documentAndIndex = out.findOrCreateDocument(fromSourceURL: sourceURL, compilationItem: compilationItem)
                    } else {
                        guard documentAndIndex != nil else {
                            try throwAnnotationError(annotation, commentedFile, message: "@Action must be set in a .vue, .ts, or .tsx file")
                        }
                    }
                    guard let interface = symbol.interface else {
                        try throwAnnotationError(annotation, commentedFile, message: "@Action must be set on a class member function")
                    }
                    guard let memberIndex = parsedAnnotation.memberIndex, let actionProperty = interface.members[safe: memberIndex] else {
                        try throwAnnotationError(annotation, commentedFile, message: "Couldn't find the method member for the @Action annotation")
                    }

                    try processAction(commentedFile: commentedFile, annotation: annotation, actionDeclaration: actionProperty, actions: &documentAndIndex!.compilationResult.templateResult.actions)
                case .viewModel:
                    guard isTSorTSX || documentAndIndex != nil else {
                        try throwAnnotationError(annotation, commentedFile, message: "@ViewModel must be set in a .vue, .ts, or .tsx file")
                    }
                    guard symbol.kind == TS.SyntaxKind.interfaceDeclaration else {
                        try throwAnnotationError(annotation, commentedFile, message: "@ViewModel must be on a TypeScript interface")
                    }
                    // Prior versions rejected a second @ViewModel in a file to prevent silent
                    // overwrite in the URL-keyed storage. Storage is now TypeKey-keyed, so
                    // multiple @ViewModel interfaces in one file are fine as long as they're
                    // bound to different Components (or none). The "one VM per Component"
                    // invariant is enforced by `componentBinding` in NativeCodeGenerationManager.
                    nativeCodeGenerationManager.addViewModelSymbol(
                        sourceURL: sourceURL,
                        compilationPath: commentedFile.src.compilationPath,
                        symbol: symbol.text)
                case .context:
                    guard isTSorTSX || documentAndIndex != nil else {
                        try throwAnnotationError(annotation, commentedFile, message: "@Context must be set in a .vue, .ts, or .tsx file")
                    }
                    guard symbol.kind == TS.SyntaxKind.interfaceDeclaration else {
                        try throwAnnotationError(annotation, commentedFile, message: "@Context must be on a TypeScript interface")
                    }
                    nativeCodeGenerationManager.addContextSymbol(
                        sourceURL: sourceURL,
                        compilationPath: commentedFile.src.compilationPath,
                        symbol: symbol.text)
                case .constructorOmitted:
                    // ConstructorOmitted annotations get processed as part of @GenerateNativeClass
                    break
                case .nativeTemplateElement:
                    // @NativeTemplateElement is handled when parsing annotations
                    break
                case .injectable:
                    // Injectable annotations get processed as part of @GenerateNativeClass
                    break
                case .singleCall:
                    // SingleCall annotations get processed inside TypeScriptNativeTypeExporter
                    break
                case .workerThread:
                    // WorkerThread annotations get processed inside TypeScriptNativeTypeExporter
                    break
                case .allowSyncCall:
                    // AllowSyncCall annotations get processed inside TypeScriptNativeTypeExporter
                    break
                case .untyped:
                    // Untyped annotations get processed inside TypeScriptNativeTypeExporter
                    break
                case .untypedMap:
                    // UntypedMap annotations get processed inside TypeScriptNativeTypeExporter
                    break
                case .version:
                    // Version annotations are consumed by the companion's version validator.
                    break
                }

                // Replace the CompilationItem with possibly-updated document
                if let documentAndIndexUnwrapped = documentAndIndex {
                    out.replace(atIndex: documentAndIndexUnwrapped.itemIndex) { item in
                        return item.with(newKind: .document(documentAndIndexUnwrapped.compilationResult))
                    }
                }
            } catch let error {
                logger.info("Error processing annotations: \(error.legibleLocalizedDescription)")
                out.injectError(logger: logger, error, relatedItem: compilationItem)
            }
        }

        let allDumped: [(TypeScriptItem, TS.DumpSymbolsWithCommentsResponseBody)] = out.allItems.compactMap { item in
            if case let .dumpedTypeScriptSymbols(result) = item.kind {
                return (result.typeScriptItemAndSymbols.typeScriptItem, result.typeScriptItemAndSymbols.dumpedSymbols)
            } else {
                return nil
            }
        }
        let dumpedSymbolsGroupedBySourceURL = Dictionary(grouping: allDumped, by: { record in record.0.src.sourceURL })

        for (sourceURL, group) in dumpedSymbolsGroupedBySourceURL {
            let documentAndIndexMaybe = out.findDocument(fromSourceURL: sourceURL)

            guard var documentAndIndex = documentAndIndexMaybe else {
                continue
            }

            let allSymbolsInGroup = group.flatMap { $0.1.dumpedSymbols }

            for dumpedSymbol in allSymbolsInGroup {
                if dumpedSymbol.modifiers?.contains("export") == true {
                    let isDefault = dumpedSymbol.modifiers?.contains("default") == true
                    let symbol = TypeScriptSymbol(symbol: dumpedSymbol.text, isDefault: isDefault)
                    documentAndIndex.compilationResult.symbolsToImportsInGeneratedCode.append(symbol)
                }
            }

            if let tsClassName = documentAndIndex.compilationResult.originalDocument.template?.jsComponentClass,
                !documentAndIndex.compilationResult.symbolsToImportsInGeneratedCode.contains(where: { $0.symbol == tsClassName }) {
                out.injectError(logger: logger, CompilerError("Custom component class '\(tsClassName)' should be exported using the 'export' keyword"), relatedItem: group.first!.0.item)
            } else {
                out.replace(atIndex: documentAndIndex.itemIndex) { item in
                    return item.with(newKind: .document(documentAndIndex.compilationResult))
                }
            }
        }

        nativeCodeGenerationManager.createCompilationItems(existingItems: out)

        return out.allItems
    }

    /// Builds a `(strippedCompilationPath) -> InterfaceFlattenerSymbolEntry` index from all
    /// TypeScript items whose symbols have been dumped this compilation. Consumed by
    /// `InterfaceFlattener` to walk `extends` chains that may cross file boundaries.
    private func buildInterfaceFlattenerIndex(items: [CompilationItem]) -> InterfaceFlattenerSymbolIndex {
        var entriesByStrippedPath: [String: InterfaceFlattenerSymbolEntry] = [:]
        for item in items {
            guard case let .dumpedTypeScriptSymbols(result) = item.kind else { continue }
            let src = result.typeScriptItemAndSymbols.typeScriptItem.src
            let strippedPath = src.compilationPath.removing(suffixes: FileExtensions.typescriptFileExtensionsDotted)
            let body = result.typeScriptItemAndSymbols.dumpedSymbols
            entriesByStrippedPath[strippedPath] = InterfaceFlattenerSymbolEntry(
                dumpedSymbols: body.dumpedSymbols,
                references: body.references)
        }
        return InterfaceFlattenerSymbolIndex(entriesByStrippedPath: entriesByStrippedPath)
    }

    private func processComponent(sourceURL: URL, commentedFile: TypeScriptCommentedFile, annotation: ValdiTypeScriptAnnotation, symbol: TS.DumpedSymbolWithComments, document: inout ValdiRawDocument) throws {
        guard symbol.kind == TS.SyntaxKind.classDeclaration else {
            try throwAnnotationError(annotation, commentedFile, message: "@Component must be set on a class")
        }
        let kindModifiers = (symbol.kindModifiers ?? "").split(separator: " ")

        guard kindModifiers.contains("export") else {
            try throwAnnotationError(annotation, commentedFile, message: "@Component must be set on a class that is exported")
        }

        document.template?.jsComponentClass = symbol.text

        // Register the Component ⇄ VM/Ctx binding so that VM/Ctx nativeClass generation
        // can attach models to this Component's document, even when they live in
        // different files. Two sources feed this:
        //   1. Explicit `viewModel:` / `context:` annotation params (cross-file support).
        //   2. Sugar fallback: for canonical Component / StatefulComponent base classes,
        //      read the type arguments of `extends Component<VM, Ctx>` directly.
        //      Preserves zero-TS-change behavior for existing same-file Components.
        let vmKey = try resolveComponentBoundKey(paramName: "viewModel",
                                                  commentedFile: commentedFile,
                                                  annotation: annotation,
                                                  symbol: symbol,
                                                  slotFor: { $0.vmSlot })
        let ctxKey = try resolveComponentBoundKey(paramName: "context",
                                                   commentedFile: commentedFile,
                                                   annotation: annotation,
                                                   symbol: symbol,
                                                   slotFor: { $0.ctxSlot })

        nativeCodeGenerationManager.registerComponentBinding(ComponentBindingInfo(
            componentSourceURL: sourceURL,
            componentSymbolName: symbol.text,
            viewModelKey: vmKey,
            contextKey: ctxKey))
    }

    /// Resolves either the ViewModel or Context binding for a `@Component` class.
    ///
    /// Preference order:
    ///   1. If the annotation carries an explicit param (`viewModel: 'X'` /
    ///      `context: 'X'`), resolve the identifier via `commentedFile.references`.
    ///      Errors loudly if the identifier isn't visible in the file.
    ///   2. Otherwise, if the Component's base class is in `ComponentBaseRegistry.slots`,
    ///      pick the type argument at the appropriate slot. Skipped silently when the
    ///      slot doesn't exist (e.g. Context on a `Component<VM>` without a second arg).
    ///
    /// Returns nil when neither path produces a binding — leaving the Component with
    /// no VM/Ctx of that kind (a legitimate configuration for context-less Components).
    private func resolveComponentBoundKey(paramName: String,
                                          commentedFile: TypeScriptCommentedFile,
                                          annotation: ValdiTypeScriptAnnotation,
                                          symbol: TS.DumpedSymbolWithComments,
                                          slotFor: (ComponentBaseSlots) -> Int?) throws -> TSSymbolKey? {
        // Explicit param wins. Two resolution paths: (1) the identifier is imported or
        // otherwise referenced as a type in the file — it lands in commentedFile.references
        // and we can take the target file straight from there; (2) the identifier is a
        // local declaration in the same file that isn't referenced as a type elsewhere
        // (rare with the current codegen surface, but possible with a custom Component
        // base class), which the companion dumps in annotatedSymbols but not in references.
        if let identifier = annotation.parameters?[paramName]?.nonEmpty {
            if let ref = commentedFile.references.first(where: { $0.name == identifier }) {
                return TSSymbolKey.make(fileName: ref.fileName, symbolName: ref.name)
            }
            if commentedFile.annotatedSymbols.contains(where: { $0.symbol.text == identifier }) {
                return TSSymbolKey.make(fileName: commentedFile.src.compilationPath, symbolName: identifier)
            }
            try throwAnnotationError(annotation, commentedFile, message: "@Component \(paramName): '\(identifier)' — '\(identifier)' is not visible in this file. Import '\(identifier)' from another module, or declare it in this file with the appropriate annotation.")
        }

        // Sugar fallback via base class type arguments.
        guard let interface = symbol.interface else { return nil }
        guard let baseSupertype = interface.supertypes?.first else { return nil }
        guard let baseTypeRefIndex = baseSupertype.type.typeReferenceIndex,
              baseTypeRefIndex >= 0, baseTypeRefIndex < commentedFile.references.count else {
            return nil
        }
        let baseName = commentedFile.references[baseTypeRefIndex].name
        guard let baseSlots = ComponentBaseRegistry.slots[baseName] else { return nil }
        guard let slotIndex = slotFor(baseSlots) else { return nil }

        let typeArgs = baseSupertype.type.typeArguments ?? []
        guard slotIndex >= 0, slotIndex < typeArgs.count else { return nil }
        let arg = typeArgs[slotIndex].type
        guard let refIndex = arg.typeReferenceIndex,
              refIndex >= 0, refIndex < commentedFile.references.count else {
            // Inline type literal or unknown shape — silently skip; explicit params
            // remain the way out for callers with unusual type args.
            return nil
        }
        let ref = commentedFile.references[refIndex]
        return TSSymbolKey.make(fileName: ref.fileName, symbolName: ref.name)
    }

    private func processAction(commentedFile: TypeScriptCommentedFile, annotation: ValdiTypeScriptAnnotation, actionDeclaration: TS.AST.PropertyLikeDeclaration, actions: inout [ValdiAction]) throws {
        let action = ValdiAction(name: actionDeclaration.name, type: .javaScript)
        if !actions.contains(action) {
            actions.append(action)
        }
    }

    private func generateTs(_ selectedItem: SelectedItem<CompilationResult>, typeScriptErrorBySourceURL: [URL: Error]) -> [CompilationItem] {
        if let userScriptSourceURL = selectedItem.data.userScriptSourceURL, let error = typeScriptErrorBySourceURL[userScriptSourceURL] {
            return [selectedItem.item.with(error: error)]
        }

        let result = selectedItem.data
        let cssModulePath = "\(selectedItem.item.relativeProjectPath).\(FileExtensions.valdiCss)"

        let tsGenerator = TypeScriptGenerator(logger: logger,
                                              customComponentClass: result.originalDocument.template?.jsComponentClass,
                                              elements: result.templateResult.rootElement,
                                              actions: result.templateResult.actions,
                                              useLegacyActions: false,
                                              hasUserScript: selectedItem.data.userScriptSourceURL != nil,
                                              sourceURL: selectedItem.item.sourceURL,
                                              symbolsToImport: result.symbolsToImportsInGeneratedCode,
                                              emitDebug: typeScriptCompilerManager.emitDebug,
                                              cssModulePath: cssModulePath)

        do {
            var out = [CompilationItem]()

            if let tsResult = try tsGenerator.generate() {
                guard let tsData = tsResult.typeScript.data(using: .utf8) else {
                    throw CompilerError("Failed to convert TS to data")
                }

                let sourceURL = TypeScriptCompilerManager.generatedUrl(url: selectedItem.item.sourceURL.appendingPathExtension(FileExtensions.typescript))
                logger.debug("Creating generated TypeScript file \(sourceURL.path)")
                out.append(CompilationItem(sourceURL: sourceURL, relativeProjectPath: nil, kind: .typeScript(.data(tsData), sourceURL), bundleInfo: selectedItem.item.bundleInfo, platform: selectedItem.item.platform, outputTarget: selectedItem.item.outputTarget))
            }

            out.append(selectedItem.item.with(newKind: .document(result)))

            return out
        } catch let error {
            return [selectedItem.item.with(error: error)]
        }
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        // process the previously-parsed file annotations
        let outItems = try processAnnotations(items: items.allItems)
        let failedFiles = extractErrors(items: outItems)

        // generate the TS files from the CompilationResult's
        let selectedItems = CompilationItems(compileSequence: items.compileSequence, items: outItems)
            .select { (item) -> CompilationResult? in
                if case .document(let result) = item.kind, result.scriptLang == FileExtensions.typescript {
                    return result
                }
                return nil
        }
        let newItems = selectedItems.transformEachConcurrently { (selectedItem: SelectedItem<CompilationResult>) -> [CompilationItem] in
            return generateTs(selectedItem, typeScriptErrorBySourceURL: failedFiles)
        }

        return CompilationItems(compileSequence: items.compileSequence, items: newItems.allItems)
    }
}
