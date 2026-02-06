//
//  NativeCodeGenerationManager.swift
//  Compiler
//
//  Created by saniul on 13/06/2019.
//

import Foundation

// TODO: Most mentions of "NativeClassXXX" should probably say "NativeTypeXXX"
struct NativeClassToGenerate {
    let annotation: ValdiTypeScriptAnnotation
    let commentedFile: TypeScriptCommentedFile
    let annotatedSymbol: TypeScriptAnnotatedSymbol
    let dumpedSymbol: TS.DumpedSymbolWithComments
    let sourceURL: URL
    let nativeClass: TypeScriptNativeClass
    let compilationItem: CompilationItem
    let linesIndexer: LinesIndexer
    let kind: NativeTypeKind
}

enum NativeClassPurpose {
    case unspecified
    case viewModel
    case context
}

struct NativeTypesToGenerate {
    let iosType: IOSType?
    let androidClass: String?
    let cppType: CPPType?
}

struct NativeFunctionToGenerate {
    let annotation: ValdiTypeScriptAnnotation
    let commentedFile: TypeScriptCommentedFile
    let annotatedSymbol: TypeScriptAnnotatedSymbol
    let dumpedSymbol: TS.DumpedSymbolWithComments
    let sourceURL: URL

    let src: TypeScriptItemSrc
    let tsTypeName: String
    let nativeTypes: NativeTypesToGenerate

    let compilationItem: CompilationItem
    let linesIndexer: LinesIndexer
}

struct NativeModuleToGenerate {
    let annotation: ValdiTypeScriptAnnotation
    let commentedFile: TypeScriptCommentedFile
    let annotatedSymbol: TypeScriptAnnotatedSymbol
    let dumpedSymbol: TS.DumpedSymbolWithComments

    let tsTypeName: String
    let nativeTypes: NativeTypesToGenerate

    let compilationItem: CompilationItem
}

class NativeCodeGenerationManager {
    private let logger: ILogger

    let globalIosImport: String?

    let nativeTypeResolver: TypeScriptNativeTypeResolver

    private var nativeClassesToGenerate = Synchronized<[NativeClassToGenerate]>(data: [])
    private var viewClassesToGenerate = Synchronized<[NativeClassToGenerate]>(data: [])
    private var nativeFunctionsToGenerate = Synchronized<[NativeFunctionToGenerate]>(data: [])
    private var nativeModulesToGenerate = Synchronized<[NativeModuleToGenerate]>(data: [])
    private var viewModelSymbolNameBySourceURL = Synchronized<[URL: String]>(data: [:])
    private var contextSymbolNameBySourceURL = Synchronized<[URL: String]>(data: [:])

    init(logger: ILogger, globalIosImport: String?, rootURL: URL) {
        self.logger = logger
        self.globalIosImport = globalIosImport
        self.nativeTypeResolver = TypeScriptNativeTypeResolver(rootURL: rootURL)
    }

    func addNativeTypeToGenerate(commentedFile: TypeScriptCommentedFile, annotation: ValdiTypeScriptAnnotation, sourceURL: URL, annotatedSymbol: TypeScriptAnnotatedSymbol, compilationItem: CompilationItem, linesIndexer: LinesIndexer, kind: NativeTypeKind, isComponent: Bool) throws {
        let nativeTypeToGenerate = try makeNativeClassToGenerate(
            commentedFile: commentedFile,
            annotation: annotation,
            sourceURL: sourceURL,
            annotatedSymbol: annotatedSymbol,
            compilationItem: compilationItem,
            linesIndexer: linesIndexer,
            kind: kind,
            isComponent: isComponent)

        if isComponent {
            viewClassesToGenerate.data { $0.append(nativeTypeToGenerate) }
        } else {
            nativeClassesToGenerate.data { $0.append(nativeTypeToGenerate) }
        }
    }

    private func makeNativeTypesToGenerate(symbolName: String,
                                           commentedFile: TypeScriptCommentedFile,
                                           annotation: ValdiTypeScriptAnnotation,
                                           compilationItem: CompilationItem) throws  -> NativeTypesToGenerate {
        var iosContainingTypeName = annotation.parameters?["ios"]
        let swiftContainingTypeName = annotation.parameters?["swift"] ?? annotation.parameters?["ios"] ?? symbolName
        var androidContainingClassName = annotation.parameters?["android"]

        if compilationItem.shouldOutputToIOS && iosContainingTypeName == nil {
            iosContainingTypeName = inferIOSClassName(symbolName: symbolName, bundleInfo: compilationItem.bundleInfo)
        }

        if compilationItem.shouldOutputToAndroid && androidContainingClassName == nil {
            androidContainingClassName = inferAndroidClassName(symbolName: symbolName, bundleInfo: compilationItem.bundleInfo)
        }

        var iosContainingType = iosContainingTypeName.map { IOSType(name: $0, swiftName: swiftContainingTypeName, bundleInfo: compilationItem.bundleInfo, kind: .class, iosLanguage: compilationItem.bundleInfo.iosLanguage) }

        if let overridenIosImport = annotation.parameters?["iosImportPrefix"] {
            iosContainingType?.applyImportPrefix(iosImportPrefix: overridenIosImport, isOverridden: true)
        } else {
            iosContainingType?.applyImportPrefix(iosImportPrefix: compilationItem.bundleInfo.iosModuleName, isOverridden: false)
        }

        let cppType = try resolveGeneratedCppType(commentedFile: commentedFile, annotation: annotation, symbolName: symbolName, symbolType: .class, bundleInfo: compilationItem.bundleInfo)

        return NativeTypesToGenerate(
            iosType: compilationItem.bundleInfo.iosCodegenEnabled ? iosContainingType : nil,
            androidClass: compilationItem.bundleInfo.androidCodegenEnabled ? androidContainingClassName : nil,
            cppType: cppType)
    }

    func addNativeModuleToGenerate(commentedFile: TypeScriptCommentedFile, annotation: ValdiTypeScriptAnnotation, sourceURL: URL, annotatedSymbol: TypeScriptAnnotatedSymbol, compilationItem: CompilationItem, linesIndexer: LinesIndexer) throws {



        let fileNameWithoutExtension = commentedFile.src.compilationPath
            .removing(suffixes: FileExtensions.typescriptFileExtensionsDotted)
            .lastPathComponent()
            .replacingOccurrences(of: ".", with: "_")
        let symbolName = "\(fileNameWithoutExtension)_Module".pascalCased

        let nativeTypes = try makeNativeTypesToGenerate(symbolName: symbolName, commentedFile: commentedFile, annotation: annotation, compilationItem: compilationItem)

        let nativeModuleToGenerate = NativeModuleToGenerate(
            annotation: annotation,
            commentedFile: commentedFile,
            annotatedSymbol: annotatedSymbol,
            dumpedSymbol: annotatedSymbol.symbol,
            tsTypeName: symbolName,
            nativeTypes: nativeTypes,
            compilationItem: compilationItem)
        nativeModulesToGenerate.data {
            $0.append(nativeModuleToGenerate)
        }
    }

    func addNativeFuncToGenerate(commentedFile: TypeScriptCommentedFile, annotation: ValdiTypeScriptAnnotation, sourceURL: URL, annotatedSymbol: TypeScriptAnnotatedSymbol, compilationItem: CompilationItem, linesIndexer: LinesIndexer) throws {
        let nativeFuncToGenerate = try makeNativeFuncToGenerate(commentedFile: commentedFile, annotation: annotation, sourceURL: sourceURL, annotatedSymbol: annotatedSymbol, compilationItem: compilationItem, linesIndexer: linesIndexer)
        nativeFunctionsToGenerate.data { $0.append(nativeFuncToGenerate) }
    }

    func hasIOSExports(for bundleInfo: CompilationItem.BundleInfo) -> Bool {
        let hasIOSClasses = nativeClassesToGenerate.data { classes in
            classes.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeClass.iosType != nil }
        }
        let hasIOSViewClasses = viewClassesToGenerate.data { classes in
            classes.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeClass.iosType != nil }
        }
        let hasIOSFunctions = nativeFunctionsToGenerate.data { functions in
            functions.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeTypes.iosType != nil }
        }
        let hasIOSModules = nativeModulesToGenerate.data { modules in
            modules.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeTypes.iosType != nil }
        }
        return hasIOSClasses || hasIOSViewClasses || hasIOSFunctions || hasIOSModules
    }

    func hasAndroidExports(for bundleInfo: CompilationItem.BundleInfo) -> Bool {
        let hasAndroidClasses = nativeClassesToGenerate.data { classes in
            classes.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeClass.androidClass != nil }
        }
        let hasAndroidViewClasses = viewClassesToGenerate.data { classes in
            classes.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeClass.androidClass != nil }
        }
        let hasAndroidFunctions = nativeFunctionsToGenerate.data { functions in
            functions.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeTypes.androidClass != nil }
        }
        let hasAndroidModules = nativeModulesToGenerate.data { modules in
            modules.contains { $0.compilationItem.bundleInfo.name == bundleInfo.name && $0.nativeTypes.androidClass != nil }
        }
        return hasAndroidClasses || hasAndroidViewClasses || hasAndroidFunctions || hasAndroidModules
    }

    func addNativeTypeConverter(commentedFile: TypeScriptCommentedFile, annotation: ValdiTypeScriptAnnotation, sourceURL: URL, annotatedSymbol: TypeScriptAnnotatedSymbol, compilationItem: CompilationItem, linesIndexer: LinesIndexer) throws {
        let symbol = annotatedSymbol.symbol
        guard let function = symbol.function else {
            try throwAnnotationError(annotation, commentedFile, message: "@NativeTypeConverter must be set on a function")
        }

        let kindModifiers = (symbol.kindModifiers ?? "").split(separator: " ")

        guard kindModifiers.contains("export") else {
            try throwAnnotationError(annotation, commentedFile, message: "@NativeTypeConverter must be set on a function that is exported (please add export keyword)")
        }

        guard let typeArguments = function.type.returnValue.typeArguments, typeArguments.count == 2 else {
            try throwAnnotationError(annotation, commentedFile, message: "@NativeTypeConverter must be set on a function that returns a generic type with two parameters")
        }

        let fromType = typeArguments[0].type
        let toType = typeArguments[1].type

        guard let typeReferenceIndexFromType = fromType.typeReferenceIndex, let typeReferenceIndexToType = toType.typeReferenceIndex else {
            try throwAnnotationError(annotation, commentedFile, message: "Expecting type references on the return value generic type")
        }

        guard typeReferenceIndexFromType >= 0 && typeReferenceIndexFromType < commentedFile.references.count else {
            throw CompilerError("Out of bounds type reference \(typeReferenceIndexFromType)")
        }
        guard typeReferenceIndexToType >= 0 && typeReferenceIndexToType < commentedFile.references.count else {
            throw CompilerError("Out of bounds type reference \(typeReferenceIndexFromType)")
        }


        let typeReferenceFromType = commentedFile.references[typeReferenceIndexFromType]
        let typeReferenceToType = commentedFile.references[typeReferenceIndexToType]        

        nativeTypeResolver.registerTypeConverter(src: commentedFile.src, emittingBundleName: compilationItem.bundleInfo.name, tsTypeName: symbol.text, fromTypePath: typeReferenceFromType.fileName, tsFromTypeName: typeReferenceFromType.name, toTypePath: typeReferenceToType.fileName, tsToTypeName: typeReferenceToType.name)
    }

    func addViewModelSymbol(sourceURL: URL, symbol: String) {
        viewModelSymbolNameBySourceURL.data { $0[sourceURL] = symbol }
    }

    func hasViewModelSymbol(for sourceURL: URL) -> Bool {
        return viewModelSymbolNameBySourceURL.data { $0[sourceURL] != nil }
    }

    func addContextSymbol(sourceURL: URL, symbol: String) {
        contextSymbolNameBySourceURL.data { $0[sourceURL] = symbol }
    }

    func hasContextSymbol(for sourceURL: URL) -> Bool {
        return contextSymbolNameBySourceURL.data { $0[sourceURL] != nil }
    }

    func clear() {
        nativeClassesToGenerate.data { $0.removeAll() }
        nativeFunctionsToGenerate.data { $0.removeAll() }
        nativeModulesToGenerate.data { $0.removeAll() }
        viewModelSymbolNameBySourceURL.data { $0.removeAll() }
        contextSymbolNameBySourceURL.data { $0.removeAll() }
        viewClassesToGenerate.data { $0.removeAll() }
    }

    private func inferIOSClassName(symbolName: String, bundleInfo: CompilationItem.BundleInfo) -> String? {
        if let iosClassPrefix = bundleInfo.iosClassPrefix {
            return "\(iosClassPrefix)\(symbolName)"
        }

        return "\(bundleInfo.iosModuleName)\(symbolName)"
    }

    private func resolveGeneratedIOSClassName(commentedFile: TypeScriptCommentedFile,
                                              annotation: ValdiTypeScriptAnnotation,
                                              symbolName: String,
                                              bundleInfo: CompilationItem.BundleInfo) throws -> String? {
        guard bundleInfo.iosCodegenEnabled else { return nil }

        let generatedClassName = inferIOSClassName(symbolName: symbolName, bundleInfo: bundleInfo)
        if let typeName = annotation.parameters?["ios"] {
            return typeName
        }

        guard let generatedClassName = generatedClassName else {
            try throwAnnotationError(annotation, commentedFile, message: "Cannot infer a generated iOS class name for \(symbolName) as the 'ios.class_prefix' value was not set in the valdi_config.yaml or module.yaml.")
        }
        return generatedClassName
    }

    private func resolveGeneratedSwiftClassName(commentedFile: TypeScriptCommentedFile,
                                                annotation: ValdiTypeScriptAnnotation,
                                                symbolName: String,
                                                bundleInfo: CompilationItem.BundleInfo) throws -> String? {
        guard bundleInfo.iosCodegenEnabled else { return nil }
        // Swift names are already in namespaces, so no need to add class_prefix
        return annotation.parameters?["swift"] ?? annotation.parameters?["ios"] ?? symbolName
    }

    private func inferAndroidClassName(symbolName: String, bundleInfo: CompilationItem.BundleInfo) -> String? {
        if let androidClassPath = bundleInfo.androidClassPath {
            return "\(androidClassPath).\(symbolName)"
        }

        if let defaultClassPath = bundleInfo.projectConfig.androidDefaultClassPath {
            return "\(defaultClassPath).\(bundleInfo.name).\(symbolName)"
        }

        return nil
    }

    private func resolveGeneratedAndroidClassName(commentedFile: TypeScriptCommentedFile,
                                                  annotation: ValdiTypeScriptAnnotation,
                                                  symbolName: String,
                                                  bundleInfo: CompilationItem.BundleInfo) throws -> String? {
        guard bundleInfo.androidCodegenEnabled else { return nil }

        let generatedClassName = inferAndroidClassName(symbolName: symbolName, bundleInfo: bundleInfo)

        if let typeName = annotation.parameters?["android"] {

            return typeName
        }

        guard let generatedClassName = generatedClassName else {
            try throwAnnotationError(annotation, commentedFile, message: "Cannot infer the generated Android class path for \(symbolName) as the 'android.class_path' value was not set in the valdi_config.yaml or module.yaml.")
        }

        return generatedClassName
    }

    private func inferCppType(symbolName: String, bundleInfo: CompilationItem.BundleInfo) -> String? {
        if let cppClassPrefix = bundleInfo.cppClassPrefix {
            return "\(cppClassPrefix)\(symbolName.pascalCased)"
        }

        if let defaultClassPrefix = bundleInfo.projectConfig.cppDefaultClassPrefix {
            return "\(defaultClassPrefix)\(bundleInfo.name)::\(symbolName.pascalCased)"
        }

        return nil
    }

    private func resolveGeneratedCppType(commentedFile: TypeScriptCommentedFile,
                                         annotation: ValdiTypeScriptAnnotation,
                                         symbolName: String,
                                         symbolType: CPPTypeDeclaration.SymbolType,
                                         bundleInfo: CompilationItem.BundleInfo) throws -> CPPType? {
        guard bundleInfo.cppCodegenEnabled else { return nil }
        let generatedClassName = inferCppType(symbolName: symbolName, bundleInfo: bundleInfo)

        if let typeName = annotation.parameters?["cpp"] {
            return CPPType(declaration: CPPTypeDeclaration(name: typeName, symbolType: symbolType), module: bundleInfo, includePrefix: bundleInfo.projectConfig.cppImportPathPrefix)
        }

        guard let generatedClassName = generatedClassName else {
            try throwAnnotationError(annotation, commentedFile, message: "Cannot infer the generated C++ class type for \(symbolName) as the 'cpp.default_class_prefrix' value was not set in the valdi_config.yaml or module.yaml.")
        }

        return CPPType(declaration: CPPTypeDeclaration(name: generatedClassName, symbolType: symbolType), module: bundleInfo, includePrefix: bundleInfo.projectConfig.cppImportPathPrefix)
    }

    @discardableResult func registerNativeClass(commentedFile: TypeScriptCommentedFile,
                                                annotation: ValdiTypeScriptAnnotation,
                                                symbol: TS.DumpedSymbolWithComments,
                                                shouldGenerateIOS: Bool,
                                                shouldGenerateAndroid: Bool,
                                                kind: NativeTypeKind,
                                                bundleInfo: CompilationItem.BundleInfo,
                                                isGenerated: Bool) throws -> TypeScriptNativeClass {
        let iosTypeName = try resolveGeneratedIOSClassName(commentedFile: commentedFile, annotation: annotation, symbolName: symbol.text, bundleInfo: bundleInfo)
        let swiftTypeName = try resolveGeneratedSwiftClassName(commentedFile: commentedFile, annotation: annotation, symbolName: symbol.text, bundleInfo: bundleInfo)
        let androidClassName = try resolveGeneratedAndroidClassName(commentedFile: commentedFile, annotation: annotation, symbolName: symbol.text, bundleInfo: bundleInfo)

        let cppSymbolType: CPPTypeDeclaration.SymbolType
        switch kind {
        case .interface:
            cppSymbolType = .class
        case .class:
            cppSymbolType = .class
        case .enum:
            cppSymbolType = .enum
        case .stringEnum:
            cppSymbolType = .enum
        }
        let cppType = try resolveGeneratedCppType(commentedFile: commentedFile, annotation: annotation, symbolName: symbol.text, symbolType: cppSymbolType, bundleInfo: bundleInfo)

        let iosType = iosTypeName.flatMap { iosTypeName in
            var iosType = IOSType(name: iosTypeName, swiftName: swiftTypeName, bundleInfo: bundleInfo, kind: kind, iosLanguage: bundleInfo.iosLanguage)
            if let overridenIosImport = annotation.parameters?["iosImportPrefix"] {
                iosType.applyImportPrefix(iosImportPrefix: overridenIosImport, isOverridden: true)
            } else {
                iosType.applyImportPrefix(iosImportPrefix: bundleInfo.iosModuleName, isOverridden: false)
            }

            return iosType
        }

        let marshallAsUntyped = annotation.parameters?["marshallAsUntyped"] == "true"
        let marshallAsUntypedMap = annotation.parameters?["marshallAsUntypedMap"] == "true"

        if isGenerated && marshallAsUntyped == true {
            try throwAnnotationError(annotation, commentedFile, message: "Generated types are never marshalled as untyped")
        }

        return nativeTypeResolver.register(src: commentedFile.src,
                                           emittingBundleName: bundleInfo.name,
                                           tsTypeName: symbol.text,
                                           iosType: iosType,
                                           androidClass: androidClassName,
                                           cppType: cppType,
                                           kind: kind,
                                           isGenerated: isGenerated,
                                           marshallAsUntyped: marshallAsUntyped,
                                           marshallAsUntypedMap: marshallAsUntypedMap)
    }

    func createCompilationItems(existingItems: DocumentsIndexer) {
        var nativeClassesPromises = [Promise<Void>]()
        let nativeClassesToGenerate = self.nativeClassesToGenerate.data { $0 }

        for nativeClass in nativeClassesToGenerate {
            var isViewModel = false
            if let viewModelSymbolName = viewModelSymbolNameBySourceURL.data({ $0[nativeClass.sourceURL] }), viewModelSymbolName == nativeClass.dumpedSymbol.text {
                isViewModel = true
            }
            var isContext = false
            if let contextSymbolName = contextSymbolNameBySourceURL.data({ $0[nativeClass.sourceURL] }), contextSymbolName == nativeClass.dumpedSymbol.text {
                isContext = true
            }

            if isContext && isViewModel {
                existingItems.injectError(logger: logger, CompilerError("Cannot put @ViewModel and @Context on the same symbol"), relatedItem: nativeClass.compilationItem)
                continue
            }

            var nativeClassPurpose = NativeClassPurpose.unspecified
            if isViewModel {
                nativeClassPurpose = .viewModel
            } else if isContext {
                nativeClassPurpose = .context
            }

            let promise = generateNativeClass(nativeClass, nativeClassPurpose: nativeClassPurpose, items: existingItems).catch { (error) in
                existingItems.injectError(logger: self.logger, error, relatedItem: nativeClass.compilationItem)
            }

            nativeClassesPromises.append(promise)
        }

        var nativeFunctionsPromises = [Promise<Void>]()
        let nativeFunctionsToGenerate = self.nativeFunctionsToGenerate.data { $0 }
        for nativeFunction in nativeFunctionsToGenerate {
            let logger = self.logger
            let promise = generateNativeFunction(nativeFunction, items: existingItems).catch { (error) in
                existingItems.injectError(logger: logger, error, relatedItem: nativeFunction.compilationItem)
            }
            nativeFunctionsPromises.append(promise)
        }

        var nativeModulesPromises = [Promise<Void>]()
        let nativeModulesToGenerate = self.nativeModulesToGenerate.data { $0 }
        for nativeModuleToGenerate in nativeModulesToGenerate {
            let logger = self.logger
            let promise = generateNativeModule(nativeModuleToGenerate, items: existingItems).catch { (error) in
                existingItems.injectError(logger: logger, error, relatedItem: nativeModuleToGenerate.compilationItem)
            }
            nativeModulesPromises.append(promise)
        }

        let allPromises = nativeClassesPromises + nativeFunctionsPromises + nativeModulesPromises
        _ = try? Promise.of(promises: allPromises).waitForData()

        // This needs to happen at the end after we proccessed all the generated types, since some of
        // the types might inject type informations into the documents
        viewClassesToGenerate.data { (nativeClasses) in
            for nativeClass in nativeClasses {
                updateCompilationItemsForGeneratedViewClass(nativeClassToGenerate: nativeClass, items: existingItems)
            }
        }
    }

    private func updateCompilationItemsForGeneratedViewClass(nativeClassToGenerate: NativeClassToGenerate, items: DocumentsIndexer) {
        guard var documentAndIndex = items.findDocument(fromSourceURL: nativeClassToGenerate.sourceURL) else {
            return
        }

        if documentAndIndex.compilationResult.originalDocument.classMapping == nil {
            documentAndIndex.compilationResult.originalDocument.classMapping = ValdiClassMapping()
        }

        let rootNodeType = documentAndIndex.compilationResult.originalDocument.template?.rootNode?.nodeType ?? ""

        // We replace the node mapping as if we specified a class for the root node in the class mapping section.
        documentAndIndex.compilationResult.originalDocument.classMapping?.nodeMappingByClass[rootNodeType] = ValdiNodeClassMapping(
            tsType: nativeClassToGenerate.dumpedSymbol.text,
            iosType: nativeClassToGenerate.nativeClass.iosType,
            androidClassName: nativeClassToGenerate.nativeClass.androidClass,
            cppType: nativeClassToGenerate.nativeClass.cppType,
            kind: .class)

        items.replace(atIndex: documentAndIndex.itemIndex) { item in
            return item.with(newKind: .document(documentAndIndex.compilationResult))
        }
    }

    private func generateNativeModule(_ nativeModuleToGenerate: NativeModuleToGenerate, items: DocumentsIndexer) -> Promise<Void> {
        let nativeTypeExporter = TypeScriptNativeTypeExporter(iosType: nativeModuleToGenerate.nativeTypes.iosType,
                                                              androidClass: nativeModuleToGenerate.nativeTypes.androidClass,
                                                              cppType: nativeModuleToGenerate.nativeTypes.cppType,
                                                              commentedFile: nativeModuleToGenerate.commentedFile,
                                                              annotatedSymbol: nativeModuleToGenerate.annotatedSymbol,
                                                              dumpedSymbol: nativeModuleToGenerate.dumpedSymbol,
                                                              nativeTypeResolver: nativeTypeResolver)

        return nativeTypeExporter.exportModule().then { (exportedModule, classMapping) in
            let resolvedClassMapping = ResolvedClassMapping(localClassMapping: classMapping, projectClassMapping: ProjectClassMapping(allowMappingOverride: false), currentBundle: nativeModuleToGenerate.compilationItem.bundleInfo)

            let item = nativeModuleToGenerate.compilationItem.with(
                newKind: .exportedType(.module(exportedModule), resolvedClassMapping, GeneratedSourceFilename(filename: nativeModuleToGenerate.compilationItem.relativeProjectPath, symbolName: nativeModuleToGenerate.tsTypeName))
            )

            items.append(item: item)

            return
        }
    }

    private func generateNativeClass(_ nativeClassToGenerate: NativeClassToGenerate, nativeClassPurpose: NativeClassPurpose, items: DocumentsIndexer) -> Promise<Void> {
        let nativeTypeExported = TypeScriptNativeTypeExporter(iosType: nativeClassToGenerate.nativeClass.iosType,
                                                              androidClass: nativeClassToGenerate.nativeClass.androidClass,
                                                              cppType: nativeClassToGenerate.nativeClass.cppType,
                                                              commentedFile: nativeClassToGenerate.commentedFile,
                                                              annotatedSymbol: nativeClassToGenerate.annotatedSymbol,
                                                              dumpedSymbol: nativeClassToGenerate.dumpedSymbol,
                                                              nativeTypeResolver: nativeTypeResolver)

        return nativeTypeExported.export().then { (nativeTypeToExport, classMapping) -> Void in
            let resolvedClassMapping = ResolvedClassMapping(localClassMapping: classMapping, projectClassMapping: ProjectClassMapping(allowMappingOverride: false), currentBundle: nativeClassToGenerate.compilationItem.bundleInfo)
            let generatedSourceFilename = GeneratedSourceFilename(filename: nativeClassToGenerate.compilationItem.relativeProjectPath, symbolName: nativeClassToGenerate.nativeClass.tsTypeName)

            switch nativeTypeToExport {
            case .valdiModel(let model):
                var modelToUse = model

                if nativeClassToGenerate.annotation.parameters?["emitConstructors"] == "legacy" {
                    modelToUse.legacyConstructors = true
                } else {
                    for property in modelToUse.properties where property.omitConstructor != nil {
                        throw CompilerError("\(nativeClassToGenerate.nativeClass.tsTypeName).\(property.name) - Using @ConstructorOmitted is deprecated when not using legacy constructors.")
                    }
                }

                if nativeClassToGenerate.kind == .interface {
                    modelToUse.exportAsInterface = true
                    for property in modelToUse.properties where property.omitConstructor != nil {
                        let hint = property.type.isOptional ? "" : " Did you mean to make the property optional with a '?' in the TypeScript interface?"
                        throw CompilerError("\(nativeClassToGenerate.nativeClass.tsTypeName).\(property.name) - Using @ConstructorOmitted on a @GenerateProxy does not do anything.\(hint)")
                    }
                }

                switch nativeClassPurpose {
                case .unspecified:
                    items.append(item: nativeClassToGenerate.compilationItem.with(newKind: .exportedType(.valdiModel(modelToUse), resolvedClassMapping, generatedSourceFilename)))
                case .viewModel:
                    if var foundDocumentAndIndexes = items.findDocument(fromSourceURL: nativeClassToGenerate.sourceURL) {
                        foundDocumentAndIndexes.compilationResult.originalDocument.viewModel = modelToUse
                        items.replace(atIndex: foundDocumentAndIndexes.itemIndex) { item in
                            return item.with(newKind: .document(foundDocumentAndIndexes.compilationResult))
                        }
                    } else {
                        items.append(item: nativeClassToGenerate.compilationItem.with(newKind: .exportedType(.valdiModel(modelToUse), resolvedClassMapping, generatedSourceFilename)))
                    }
                case .context:
                    if var foundDocumentAndIndexes = items.findDocument(fromSourceURL: nativeClassToGenerate.sourceURL) {
                        foundDocumentAndIndexes.compilationResult.originalDocument.componentContext = ValdiNodeClassMapping(
                            tsType: modelToUse.tsType,
                            iosType: modelToUse.iosType,
                            androidClassName: modelToUse.androidClassName,
                            cppType: modelToUse.cppType,
                            kind: .class)

                        items.replace(atIndex: foundDocumentAndIndexes.itemIndex) { item in
                            return item.with(newKind: .document(foundDocumentAndIndexes.compilationResult))
                        }
                    }

                    items.append(item: nativeClassToGenerate.compilationItem.with(newKind: .exportedType(.valdiModel(modelToUse), resolvedClassMapping, generatedSourceFilename)))

                    if !modelToUse.exportAsInterface {
                        items.append(item: nativeClassToGenerate.compilationItem.with(newKind: .exportedDependencyMetadata(modelToUse, resolvedClassMapping, generatedSourceFilename)))
                    }
                }
            case .enum(let exportedEnum):
                let item = nativeClassToGenerate.compilationItem.with(
                    newKind: .exportedType(.enum(exportedEnum), resolvedClassMapping, generatedSourceFilename)
                )

                items.append(item: item)
            case .function, .module:
                return
            }
        }.catch { (error) -> Promise<Void> in
            let exporterError = CompilerError(type: "NativeClassExporter error", message: "Failed to export native class '\(nativeClassToGenerate.dumpedSymbol.text)': \(error.legibleLocalizedDescription)", range: NSRange(location: nativeClassToGenerate.dumpedSymbol.start, length: nativeClassToGenerate.dumpedSymbol.text.count), inDocument: nativeClassToGenerate.commentedFile.fileContent)
            return Promise(error: exporterError)
        }
    }

    private func generateNativeFunction(_ nativeFuncToGenerate: NativeFunctionToGenerate, items: DocumentsIndexer) -> Promise<Void> {
        let nativeTypeExported = TypeScriptNativeTypeExporter(iosType: nativeFuncToGenerate.nativeTypes.iosType,
                                                              androidClass: nativeFuncToGenerate.nativeTypes.androidClass,
                                                              cppType: nativeFuncToGenerate.nativeTypes.cppType,
                                                              commentedFile: nativeFuncToGenerate.commentedFile,
                                                              annotatedSymbol: nativeFuncToGenerate.annotatedSymbol,
                                                              dumpedSymbol: nativeFuncToGenerate.dumpedSymbol,
                                                              nativeTypeResolver: nativeTypeResolver)

        return nativeTypeExported.exportFunction().then { (exportedFunction, classMapping) -> Void in
            let resolvedClassMapping = ResolvedClassMapping(localClassMapping: classMapping, projectClassMapping: ProjectClassMapping(allowMappingOverride: false), currentBundle: nativeFuncToGenerate.compilationItem.bundleInfo)

            let item = nativeFuncToGenerate.compilationItem.with(
                newKind: .exportedType(.function(exportedFunction), resolvedClassMapping, GeneratedSourceFilename(filename: nativeFuncToGenerate.compilationItem.relativeProjectPath, symbolName: nativeFuncToGenerate.tsTypeName))
            )

            items.append(item: item)
            }.catch { (error) -> Promise<Void> in
                let exporterError = CompilerError(type: "NativeFuncExporter error", message: "Failed to export native function '\(nativeFuncToGenerate.dumpedSymbol.text)': \(error.legibleLocalizedDescription)", range: nativeFuncToGenerate.annotation.range, inDocument: nativeFuncToGenerate.commentedFile.fileContent)
                return Promise(error: exporterError)
        }
    }    

    private func makeNativeClassToGenerate(commentedFile: TypeScriptCommentedFile,
                                           annotation: ValdiTypeScriptAnnotation,
                                           sourceURL: URL,
                                           annotatedSymbol: TypeScriptAnnotatedSymbol,
                                           compilationItem: CompilationItem,
                                           linesIndexer: LinesIndexer,
                                           kind: NativeTypeKind,
                                           isComponent: Bool) throws -> NativeClassToGenerate {

        let symbol = annotatedSymbol.symbol
        // TODO: extract validation from this function?
        switch kind {
        case .class, .interface:
            guard symbol.kind == TS.SyntaxKind.interfaceDeclaration || symbol.kind == TS.SyntaxKind.classDeclaration
            else {
                try throwAnnotationError(annotation, commentedFile, message: "@ExportModel or @ExportProxy must be set on an interface or class")
            }

            let ignoreInheritance = annotation.parameters?["ignoreInheritance"] == "true"

            guard ignoreInheritance || isComponent || (symbol.interface?.supertypes?.count ?? 0) == 0 else {
                try throwAnnotationError(annotation, commentedFile, message: "@ExportModel or @ExportProxy can only be used on types without inheritance")
            }

        case .enum, .stringEnum:
            guard symbol.kind == TS.SyntaxKind.enumDeclaration else {
                try throwAnnotationError(annotation, commentedFile, message: "@ExportEnum must be set on an enum")
            }
        }

        let nativeClass = try registerNativeClass(commentedFile: commentedFile, annotation: annotation, symbol: symbol, shouldGenerateIOS: compilationItem.shouldOutputToIOS, shouldGenerateAndroid: compilationItem.shouldOutputToAndroid, kind: kind, bundleInfo: compilationItem.bundleInfo, isGenerated: true)

        return NativeClassToGenerate(annotation: annotation,
                                     commentedFile: commentedFile,
                                     annotatedSymbol: annotatedSymbol,
                                     dumpedSymbol: symbol,
                                     sourceURL: sourceURL,
                                     nativeClass: nativeClass,
                                     compilationItem: compilationItem,
                                     linesIndexer: linesIndexer,
                                     kind: kind)
    }

    private func makeNativeFuncToGenerate(commentedFile: TypeScriptCommentedFile, annotation: ValdiTypeScriptAnnotation, sourceURL: URL, annotatedSymbol: TypeScriptAnnotatedSymbol, compilationItem: CompilationItem, linesIndexer: LinesIndexer) throws -> NativeFunctionToGenerate {

        let symbol = annotatedSymbol.symbol
        guard symbol.function != nil else {
            try throwAnnotationError(annotation, commentedFile, message: "@ExportFunction must be set on a function")
        }

        let kindModifiers = (symbol.kindModifiers ?? "").split(separator: " ")

        guard kindModifiers.contains("export") else {
            try throwAnnotationError(annotation, commentedFile, message: "@ExportFunction must be set on a function that is exported (please add export keyword)")
        }

        let functionName = symbol.text

        let nativeTypes = try makeNativeTypesToGenerate(symbolName: functionName.pascalCased,
                                                        commentedFile: commentedFile,
                                                        annotation: annotation,
                                                        compilationItem: compilationItem)

        let nativeFunc = NativeFunctionToGenerate(annotation: annotation,
                                                  commentedFile: commentedFile,
                                                  annotatedSymbol: annotatedSymbol,
                                                  dumpedSymbol: symbol,
                                                  sourceURL: sourceURL,
                                                  src: commentedFile.src,
                                                  tsTypeName: functionName,
                                                  nativeTypes: nativeTypes,
                                                  compilationItem: compilationItem,
                                                  linesIndexer: linesIndexer)
        return nativeFunc
    }

}