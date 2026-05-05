//
//  CppModuleGenerator.swift
//  Compiler
//
//  Created by Simon Corsin on 11/26/25.
//


final class CppModuleGenerator {
    private let bundleInfo: CompilationItem.BundleInfo
    private let cppType: CPPType
    private let exportedModule: ExportedModule
    private let classMapping: ResolvedClassMapping
    private let sourceFileName: GeneratedSourceFilename

    init(bundleInfo: CompilationItem.BundleInfo,
         cppType: CPPType,
         exportedModule: ExportedModule,
         classMapping: ResolvedClassMapping,
         sourceFileName: GeneratedSourceFilename) {
        self.bundleInfo = bundleInfo
        self.cppType = cppType
        self.exportedModule = exportedModule
        self.classMapping = classMapping
        self.sourceFileName = sourceFileName
    }

    func write() throws -> [NativeSource] {
        let moduleFactoryClassName = "\(cppType.declaration.name)Factory"
        let moduleFactoryCppType = CPPType(declaration: CPPTypeDeclaration(namespace: self.cppType.declaration.namespace,
                                                                         name: moduleFactoryClassName,
                                                                         symbolType: .class),
                                         module: bundleInfo,
                                         includePrefix: self.cppType.includePrefix)

        let classNamespace = cppType.declaration.namespace.appending(component: moduleFactoryCppType.declaration.name)

        var output: [NativeSource] = []

        let modelGenerator = CppModelGenerator(cppType: cppType,
                                               bundleInfo: bundleInfo,
                                               typeParameters: nil,
                                               properties: exportedModule.model.properties,
                                               classMapping: classMapping,
                                               sourceFileName: sourceFileName,
                                               isInterface: true,
                                               comments: exportedModule.model.comments)

        output += try modelGenerator.write()

        let generator = CppCodeGenerator(namespace: self.cppType.declaration.namespace,
                                         selfIncludePath: moduleFactoryCppType.includePath,
                                         namespaceResolver: CppCodeGeneratorSingleNamespaceResolver(classNamespace: classNamespace))

        generator.impl.includeSection.addInclude(path: moduleFactoryCppType.includePath)

        generator.header.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppGeneratedModuleFactory.hpp")
        generator.header.includeSection.addInclude(path: "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp")
        if cppType.includePath != moduleFactoryCppType.includePath {
            generator.header.includeSection.addInclude(path: cppType.includePath)
        }

        let additionalComments = """
        This type should be inherited from, and the onLoadModule() should be overriden to return
        an instance of type \(cppType.declaration.fullTypeName).
        The module should then be registered by creating a Valdi::RegisterModuleFactory static instance.
            
        """

        generator.header.body.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: sourceFileName, additionalComments: additionalComments))
        generator.header.body.appendBody("""
            
            class \(moduleFactoryClassName): public Valdi::CppGeneratedModuleFactory<\(cppType.declaration.name)> {
            public:
                \(moduleFactoryClassName)();
                ~\(moduleFactoryClassName)() override;
            
                Valdi::StringBox getModulePath() final;            
            };

            """)

        generator.impl.body.appendBody("""
            \(moduleFactoryClassName)::\(moduleFactoryClassName)() = default;
            \(moduleFactoryClassName)::~\(moduleFactoryClassName)() = default;
            
            Valdi::StringBox \(moduleFactoryClassName)::getModulePath() {
                return Valdi::StringBox::fromCString("\(self.exportedModule.modulePath)");
            }
            """)

        output.append(NativeSource(relativePath: moduleFactoryCppType.includeDir,
                                   filename: "\(moduleFactoryCppType.declaration.name).hpp",
                                   file: .data(try generator.header.content.indented.utf8Data()),
                                   groupingIdentifier: "\(bundleInfo.name).hpp", groupingPriority: 0))

        output.append(NativeSource(relativePath: moduleFactoryCppType.includeDir,
                                   filename: "\(moduleFactoryCppType.declaration.name).cpp",
                                   file: .data(try generator.impl.content.indented.utf8Data()),
                                   groupingIdentifier: "\(bundleInfo.name).cpp", groupingPriority: 0))

        return output
    }
}
