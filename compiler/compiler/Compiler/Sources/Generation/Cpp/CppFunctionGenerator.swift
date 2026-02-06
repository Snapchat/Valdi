//
//  CppFunctionGenerator.swift
//  Compiler
//
//  Created by Simon Corsin on 12/1/25.
//

final class CppFunctionGenerator {
    private let cppType: CPPType
    private let exportedFunction: ExportedFunction
    private let classMapping: ResolvedClassMapping
    private let sourceFileName: GeneratedSourceFilename
    private let modulePath: String
    private let bundleInfo: CompilationItem.BundleInfo

    init(cppType: CPPType,
         exportedFunction: ExportedFunction,
         classMapping: ResolvedClassMapping,
         sourceFileName: GeneratedSourceFilename,
         modulePath: String,
         bundleInfo: CompilationItem.BundleInfo) {
        self.cppType = cppType
        self.exportedFunction = exportedFunction
        self.classMapping = classMapping
        self.sourceFileName = sourceFileName
        self.modulePath = modulePath
        self.bundleInfo = bundleInfo
    }

    func write() throws -> [NativeSource] {
        let typealiasNamespace = cppType.declaration.namespace.appending(component: "\(cppType.declaration.name)TypeAliases")

        let generator = CppCodeGenerator(namespace: self.cppType.declaration.namespace,
                                         selfIncludePath: cppType.includePath,
                                         namespaceResolver: CppCodeGeneratorSingleNamespaceResolver(classNamespace: typealiasNamespace))

        generator.header.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppGeneratedExportedFunction.hpp")
        generator.header.includeSection.addInclude(path: "valdi_core/cpp/Utils/Result.hpp")
        generator.impl.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppGeneratedExportedFunctionUtils.hpp")
        generator.impl.includeSection.addInclude(path: cppType.includePath)


        generator.header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration:
                                                                                                    CPPTypeDeclaration(namespace: CPPNamespace(components: ["snap", "valdi_core"]), name: "JSRuntime", symbolType: .class),
                                                                                                   typeArguments: nil))
        generator.header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration:
                                                                                                    CPPTypeDeclaration(namespace: CPPNamespace(components: ["snap", "valdi_core"]), name: "JSRuntimeNativeObjectsManager", symbolType: .class),
                                                                                                   typeArguments: nil))

        let property = ValdiModelProperty(name: exportedFunction.functionName,
                                          type: .function(parameters: exportedFunction.parameters, returnType: exportedFunction.returnType, isSingleCall: false, shouldCallOnWorkerThread: false),
                                          comments: exportedFunction.comments,
                                          omitConstructor: nil,
                                          injectableParams: .empty)

        let nameAllocator = PropertyNameAllocator.forCpp()

        let returnTypeParser = try generator.getTypeParser(type: exportedFunction.returnType, namePaths: [], nameAllocator: nameAllocator)
        let parameterTypeParsers = try exportedFunction.parameters.map { try generator.getTypeParser(type: $0.type, namePaths: [], nameAllocator: nameAllocator) }

        let schemaWriter = CppSchemaWriter(typeParameters: nil, generator: generator)

        try schemaWriter.appendClass(cppType.declaration.name, properties: [property])

        let allTypeArguments = ([returnTypeParser] + parameterTypeParsers).map { $0.typeNameResolver.resolve(self.cppType.declaration.namespace) }.joined(separator: ", ")

        var registerSchemaParameters: [String]
        if generator.referencedTypes.isEmpty {
            registerSchemaParameters = ["\"\(schemaWriter.str)\""]
        } else {
            registerSchemaParameters = ["\"\(schemaWriter.str)\"", generator.getTypeReferencesVecExpression(inNamespace: self.cppType.declaration.namespace)]
        }

        if !generator.typealiases.isEmpty {
            generator.header.body.appendBody("namespace \(typealiasNamespace.components.last!) {\n")
            for cppTypealias in generator.typealiases {
                generator.header.body.appendBody(cppTypealias.statement.resolve(typealiasNamespace))
            }
            generator.header.body.appendBody("}\n\n")
        }

        generator.header.body.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: sourceFileName, additionalComments: exportedFunction.comments))

        let className = cppType.declaration.name
        generator.header.body.appendBody("""

            class \(className): public Valdi::CppGeneratedExportedFunction<\(allTypeArguments)> {
            public:
                using CppGeneratedExportedFunction<\(allTypeArguments)>::CppGeneratedExportedFunction;
            
                /**
                * Resolve the function from the given JS runtime. If the native objects manager is provided,
                * emitted native objects will be associated with the given manager, otherwise they will be 
                * associated with the global native objects manager and only be cleaned up on JS GC.
                */
                static Valdi::Result<\(className)> resolve(snap::valdi_core::JSRuntime &jsRuntime, const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager);
            };

            """)

        generator.impl.body.appendBody("""
            
            Valdi::Result<\(className)> \(className)::resolve(snap::valdi_core::JSRuntime &jsRuntime, const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager) {
                static auto *kSchema = Valdi::CppGeneratedExportedFunctionUtils::registerFunctionSchema(\(registerSchemaParameters.joined(separator: ", ")));
                return Valdi::CppGeneratedExportedFunctionUtils::resolve<\(className)>(jsRuntime, nativeObjectsManager, "\(bundleInfo.name)/\(modulePath)", *kSchema);
            }
            """)

        return [
            NativeSource(relativePath: cppType.includeDir,
                         filename: "\(cppType.declaration.name).hpp",
                         file: .data(try generator.header.content.indented.utf8Data()),
                         groupingIdentifier: "\(bundleInfo.name).hpp", groupingPriority: 0, localFilenameDependencies: generator.dependenciesInSameHeaderFile),
            NativeSource(relativePath: cppType.includeDir,
                         filename: "\(cppType.declaration.name).cpp",
                         file: .data(try generator.impl.content.indented.utf8Data()),
                         groupingIdentifier: "\(bundleInfo.name).cpp", groupingPriority: 0)

        ]
    }
}
