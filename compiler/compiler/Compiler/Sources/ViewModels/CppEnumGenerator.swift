// Copyright © 2025 Snap, Inc. All rights reserved.

class CppEnumGenerator {
    private let exportedEnum: ExportedEnum
    private let cppType: CPPType
    private let bundleInfo: CompilationItem.BundleInfo
    private let sourceFileName: GeneratedSourceFilename

    init(exportedEnum: ExportedEnum,
         cppType: CPPType,
         bundleInfo: CompilationItem.BundleInfo,
         sourceFileName: GeneratedSourceFilename) {
        self.exportedEnum = exportedEnum
        self.cppType = cppType
        self.bundleInfo = bundleInfo
        self.sourceFileName = sourceFileName
    }

    func write() throws -> [NativeSource] {
        let header = CppFileGenerator(namespace: cppType.declaration.namespace, isHeader: true)
        let impl = CppFileGenerator(namespace: cppType.declaration.namespace, isHeader: false)
        
        impl.includeSection.addInclude(path: cppType.includePath)

        let exceptionTrackerTypeDecl = CppFileGenerator.getValdiTypeDeclaration(typeName: "ExceptionTracker")
        let valueTypeDecl = CppFileGenerator.getValdiTypeDeclaration(typeName: "Value")
        let registeredClassType = CppFileGenerator.getValdiTypeDeclaration(typeName: "RegisteredCppGeneratedClass")
        let generatedEnumClass = CppFileGenerator.getValdiTypeDeclaration(typeName: "CppGeneratedEnum")

        header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: exceptionTrackerTypeDecl, typeArguments: nil))
        header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: valueTypeDecl, typeArguments: nil))
        header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: registeredClassType, typeArguments: nil))

        header.body.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: self.sourceFileName, additionalComments: exportedEnum.comments))
        header.body.appendBody("\n")

        let enumBody = CppCodeWriter()

        header.body.appendBody("enum class \(cppType.declaration.name) {\n")
        header.body.appendBody(enumBody)
        header.body.appendBody("};\n\n")

        header.body.appendBody("void unmarshallEnum(\(exceptionTrackerTypeDecl.fullTypeName) &exceptionTracker, const \(valueTypeDecl.fullTypeName) &value, \(cppType.declaration.name) &out);\n\n")
        header.body.appendBody("void marshallEnum(\(exceptionTrackerTypeDecl.fullTypeName) &exceptionTracker, \(cppType.declaration.name) value, \(valueTypeDecl.fullTypeName) &out);\n\n")

        header.body.appendBody("\(registeredClassType.fullTypeName) *getRegisteredEnumClass(const \(cppType.declaration.name) *);\n")

        impl.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppGeneratedEnum.hpp")

        let fqMarshallerTypeName: String
        let allEnumValuesLiteral: String
        let schemaWriter = CppSchemaWriter(typeParameters: nil, generator: nil)
        let nameAllocator = PropertyNameAllocator.forCpp()

        let allCases: [(String, String?)]
        switch exportedEnum.cases {
        case .enum(let intCases):
            try schemaWriter.appendIntEnum(cppType.declaration.fullTypeName, enumCases: intCases)
            let intEnumMarshaller = CppFileGenerator.getValdiTypeDeclaration(typeName: "CppIntEnumMarshaller")
            fqMarshallerTypeName = "\(intEnumMarshaller.fullTypeName)<\(cppType.declaration.name), \(intCases.count)>"
            allEnumValuesLiteral = intCases.map { "\($0.value)" }.joined(separator: ", ")

            allCases = intCases.map { (nameAllocator.allocate(property: $0.name).name, $0.comments) }
            break
        case .stringEnum(let stringCases):
            try schemaWriter.appendStringEnum(cppType.declaration.fullTypeName, enumCases: stringCases)

            let stringEnumMarshaller = CppFileGenerator.getValdiTypeDeclaration(typeName: "CppStringEnumMarshaller")
            fqMarshallerTypeName = "\(stringEnumMarshaller.fullTypeName)<\(cppType.declaration.name), \(stringCases.count)>"
            allEnumValuesLiteral = stringCases.map { "\"\($0.value)\"" }.joined(separator: ", ")

            allCases = stringCases.map { (nameAllocator.allocate(property: $0.name).name, $0.comments) }
            break
        }

        let lastIndex = allCases.count - 1
        for (idx, (caseName, comments)) in allCases.enumerated() {
            if let comments {
                enumBody.appendBody(FileHeaderCommentGenerator.generateMultilineComment(comment: comments))
                enumBody.appendBody("\n")
            }

            enumBody.appendBody("\(caseName) = \(idx)")
            if idx != lastIndex {
                enumBody.appendBody(",\n")
            } else {
                enumBody.appendBody("\n")
            }
        }

        let getEnumMarshallerName = "get\(cppType.declaration.name.pascalCased)EnumMarshaller"

        impl.body.appendBody("""
            namespace {
              const \(fqMarshallerTypeName) &\(getEnumMarshallerName)() {
                static auto kEnumMarshaller = \(fqMarshallerTypeName)({\(allEnumValuesLiteral)});
                return kEnumMarshaller;
              }
            }
            
            void unmarshallEnum(\(exceptionTrackerTypeDecl.fullTypeName) &exceptionTracker, const \(valueTypeDecl.fullTypeName) &value, \(cppType.declaration.name) &out) {
              \(getEnumMarshallerName)().unmarshall(exceptionTracker, value, out);
            }
            
            void marshallEnum(\(exceptionTrackerTypeDecl.fullTypeName) &exceptionTracker, \(cppType.declaration.name) value, \(valueTypeDecl.fullTypeName) &out) {
              \(getEnumMarshallerName)().marshall(exceptionTracker, value, out);
            }
            
            \(registeredClassType.fullTypeName) *getRegisteredEnumClass(const \(cppType.declaration.name) *) {
              static auto kEnumClass = \(generatedEnumClass.fullTypeName)::registerEnumSchema("\(schemaWriter.str)");
              return kEnumClass;
            }
            """)

        return [
            NativeSource(relativePath: cppType.includeDir,
                         filename: "\(cppType.declaration.name).hpp",
                         file: .string(header.content.indented),
                         groupingIdentifier: "\(bundleInfo.name).hpp", groupingPriority: 0),
            NativeSource(relativePath: cppType.includeDir,
                         filename: "\(cppType.declaration.name).cpp",
                         file: .string(impl.content.indented),
                         groupingIdentifier: "\(bundleInfo.name).cpp", groupingPriority: 0)
        ]
    }
}
