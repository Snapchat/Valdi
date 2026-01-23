//
//  CppModelGenerator.swift
//
//
//  Created by Simon Corsin on 4/11/23.
//

import Foundation

private struct CppPropertyName {
    let fieldName: String
    let getterName: String
    let setterName: String
    let constructorParameterName: String
}

private struct CppProperty {
    let name: CppPropertyName
    let property: ValdiModelProperty
    let typeParser: CppTypeParser
}

private final class CppCodeGeneratorModelNamespaceResolver: CppCodeGeneratorNamespaceResolver {
    private let classNamespace: CPPNamespace
    private let proxyNamespace: CPPNamespace
    private let isInterface: Bool

    init(classNamespace: CPPNamespace, proxyNamespace: CPPNamespace, isInterface: Bool) {
        self.classNamespace = classNamespace
        self.proxyNamespace = proxyNamespace
        self.isInterface = isInterface
    }

    func getNamespace(forTypeAlias typeAlias: CppTypeAlias) -> CPPNamespace {
        if isInterface && typeAlias.isOnMethod {
            return proxyNamespace
        } else {
            return classNamespace
        }
    }
}

final class CppModelGenerator {
    private let cppType: CPPType
    private let bundleInfo: CompilationItem.BundleInfo
    private let typeParameters: [ValdiTypeParameter]?
    private let properties: [ValdiModelProperty]
    private let classMapping: ResolvedClassMapping
    private let sourceFileName: GeneratedSourceFilename
    private let isInterface: Bool
    private let comments: String?

    init(cppType: CPPType,
         bundleInfo: CompilationItem.BundleInfo,
         typeParameters: [ValdiTypeParameter]?,
         properties: [ValdiModelProperty],
         classMapping: ResolvedClassMapping,
         sourceFileName: GeneratedSourceFilename,
         isInterface: Bool,
         comments: String?) {
        self.cppType = cppType
        self.bundleInfo = bundleInfo
        self.typeParameters = typeParameters
        self.properties = properties
        self.classMapping = classMapping
        self.sourceFileName = sourceFileName
        self.isInterface = isInterface
        self.comments = comments
    }

    private func resolveProperties(codeGenerator: CppCodeGenerator, nameAllocator: PropertyNameAllocator) throws -> [CppProperty] {
        return try properties.map { property in
            let typeParser = try codeGenerator.getTypeParser(type: property.type, namePaths: [property.name], nameAllocator: nameAllocator)
            let fieldName = nameAllocator.allocate(property: "_\(property.name)").name
            let getterName = nameAllocator.allocate(property: "get\(property.name.pascalCased)").name
            let setterName = nameAllocator.allocate(property: "set\(property.name.pascalCased)").name
            let constructorParameterName = nameAllocator.allocate(property: property.name).name

            return CppProperty(name:
                                CppPropertyName(fieldName: fieldName, getterName: getterName, setterName: setterName, constructorParameterName: constructorParameterName),
                               property: property,
                               typeParser: typeParser)
        }
    }

    private func generateClassFields(classNamespace: CPPNamespace, cppProperties: [CppProperty]) -> CppCodeWriter {
        let decl = CppCodeWriter()
        for cppProperty in cppProperties {
            let typeName = cppProperty.typeParser.typeNameResolver.resolve(classNamespace)
            if let fieldInitializationString = cppProperty.typeParser.defaultInitializationString {
                decl.appendBody("\(typeName) \(cppProperty.name.fieldName) = \(fieldInitializationString);\n")
            } else {
                decl.appendBody("\(typeName) \(cppProperty.name.fieldName);\n")
            }
        }

        return decl
    }

    private func makeClassWriter() -> CppClassWriter {
        let header = CppCodeWriter()
        let impl = CppCodeWriter()
        let inlineImplementation = typeParameters != nil

        return CppClassWriter(namespace: cppType.declaration.namespace, className: cppType.declaration.name, header: header, impl: impl, inlineImplementation: inlineImplementation)
    }

    private func generateClassConstructors(classNamespace: CPPNamespace, cppProperties: [CppProperty], parentClassName: String) -> (decl: CppCodeWriter, impl: CppCodeWriter) {
        let classWriter = makeClassWriter()

        classWriter.writeConstructor(arguments: [], memberInitializerList: "\(parentClassName)(getRegisteredClass())") { writer in

        }

        if !cppProperties.isEmpty {
            let ctorParameters = cppProperties.map {
                if $0.typeParser.isMovable {
                    return CPPFunctionArgument(typeResolver: .with(specifiers:.constRef, $0.typeParser.typeNameResolver), name: $0.name.constructorParameterName)
                } else {
                    return CPPFunctionArgument(typeResolver: $0.typeParser.typeNameResolver, name: $0.name.constructorParameterName)
                }
            }
            let ctorFieldInitializers = cppProperties.map {
                let initializer = $0.typeParser.isMovable ? "std::move(\($0.name.constructorParameterName))" : $0.name.constructorParameterName
                return "\($0.name.fieldName)(\(initializer))"
            }.joined(separator: ",\n")

            classWriter.writeConstructor(arguments: ctorParameters, memberInitializerList: "\(parentClassName)(getRegisteredClass()),\n\(ctorFieldInitializers)") { writer in

            }
        }

        return (classWriter.header, classWriter.impl)
    }

    private func generateClassMethods(classNamespace: CPPNamespace, cppProperties: [CppProperty]) -> (decl: CppCodeWriter, impl: CppCodeWriter) {
        let classWriter = makeClassWriter()
        for cppProperty in cppProperties {
            if cppProperty.typeParser.isMovable {
                classWriter.writeMethod(name: cppProperty.name.getterName,
                                        arguments: [],
                                        returnType: .with(specifiers:.constRef, cppProperty.typeParser.typeNameResolver),
                                        specifiers: .const) { writer in
                    writer.appendBody("return \(cppProperty.name.fieldName);\n")
                }

                classWriter.writeMethod(name: cppProperty.name.setterName,
                                        arguments: [CPPFunctionArgument(typeResolver: .with(specifiers: .constRef, cppProperty.typeParser.typeNameResolver), name: "value")],
                                        returnType: .with(typeName: "void"),
                                        specifiers: .none) { writer in
                    writer.appendBody("\(cppProperty.name.fieldName) = value;\n")
                }

                classWriter.writeMethod(name: cppProperty.name.setterName,
                                        arguments: [CPPFunctionArgument(typeResolver: .with(specifiers: .rValueRef, cppProperty.typeParser.typeNameResolver), name: "value")],
                                        returnType: .with(typeName: "void"),
                                        specifiers: .none) { writer in
                    writer.appendBody("\(cppProperty.name.fieldName) = std::move(value);\n")
                }
            } else {
                classWriter.writeMethod(name: cppProperty.name.getterName,
                                        arguments: [],
                                        returnType: cppProperty.typeParser.typeNameResolver,
                                        specifiers: .const) { writer in
                    writer.appendBody("return \(cppProperty.name.fieldName);\n")
                }

                classWriter.writeMethod(name: cppProperty.name.setterName,
                                        arguments: [CPPFunctionArgument(typeResolver: cppProperty.typeParser.typeNameResolver, name: "value")],
                                        returnType: .with(typeName: "void"),
                                        specifiers: .none) { writer in
                    writer.appendBody("\(cppProperty.name.fieldName) = value;\n")
                }
            }
        }

        return (classWriter.header, classWriter.impl)
    }

    private func generateRegisterClass(generator: CppCodeGenerator) throws -> (decl: CppCodeWriter, impl: CppCodeWriter) {
        let cppGeneratedClass = CppCodeGenerator.getValdiTypeName(typeName: "CppGeneratedClass").resolve(cppType.declaration.namespace)
        let registeredClassType = CPPTypeNameResolver.with(specifiers: .ptr, CppCodeGenerator.getValdiTypeName(typeName: "RegisteredCppGeneratedClass"))

        let schemaWriter = CppSchemaWriter(typeParameters: typeParameters, generator: generator)
        if isInterface {
            try schemaWriter.appendInterface(cppType.declaration.fullTypeName, properties: self.properties)
        } else {
            try schemaWriter.appendClass(cppType.declaration.fullTypeName, properties: self.properties)
        }

        let classWriter = makeClassWriter()

        var registerSchemaParameters: [String]
        if generator.referencedTypes.isEmpty {
            registerSchemaParameters = ["\"\(schemaWriter.str)\""]
        } else {
            registerSchemaParameters = ["\"\(schemaWriter.str)\"", generator.getTypeReferencesVecExpression(inNamespace: self.cppType.declaration.namespace)]
        }

        if let typeParameters {
            registerSchemaParameters.append("Valdi::CppGeneratedGenericClass::makeTypeArgumentsCallback<\(typeParameters.map { $0.name }.joined(separator: ", "))>()")
            generator.header.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppGeneratedGenericClass.hpp")

            classWriter.writeMethod(name: "getRegisteredClass",
                                    arguments: [],
                                    returnType: registeredClassType,
                                    specifiers: .static_) { writer in
                writer.appendBody("static auto *kRegisteredClass = \(cppGeneratedClass)::registerGenericSchema(\(registerSchemaParameters.joined(separator: ", ")));\n\n")
                writer.appendBody("return kRegisteredClass;\n");
            }
        } else {
            classWriter.writeMethod(name: "getRegisteredClass",
                                    arguments: [],
                                    returnType: registeredClassType,
                                    specifiers: .static_) { writer in
                writer.appendBody("static auto *kRegisteredClass = \(cppGeneratedClass)::registerSchema(\(registerSchemaParameters.joined(separator: ", ")));\n\n")
                writer.appendBody("return kRegisteredClass;\n");
            }
        }

        return (classWriter.header, classWriter.impl)
    }

    private func generateFieldsReferenceArgument(cppProperties: [CppProperty]) -> String {
        if cppProperties.isEmpty {
            return ""
        } else {
            let fieldsReferenceString = cppProperties.map { "self.\($0.name.fieldName)" }.joined(separator: ", ")
            return ", \(fieldsReferenceString)"
        }
    }

    private func generateMethodsReferenceArgument(receiverArgument: String, cppProperties: [CppProperty]) -> String {
        if cppProperties.isEmpty {
            return ""
        } else {
            let methodsReferenceString = cppProperties.map { cppProperty in
                if cppProperty.typeParser.method != nil {
                    return "Valdi::CppMarshaller::methodToFunction<\(cppType.declaration.name), &\(cppType.declaration.name)::\(cppProperty.name.constructorParameterName)>(\(receiverArgument))"
                } else {
                    return "\(receiverArgument)->\(cppProperty.name.constructorParameterName)()"
                }
            }.joined(separator: ", ")
            return ", \(methodsReferenceString)"
        }
    }

    private func writeClassPrologue(parentClassName: String, header: CodeWriter) {
        header.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: self.sourceFileName, additionalComments: self.comments))
        header.appendBody("\n")

        if let typeParameters {
            let typeNames = typeParameters.map { "typename \($0.name)" }.joined(separator: ", ")
            header.appendBody("template<\(typeNames)>\n")
        }
        header.appendBody("class \(cppType.declaration.name): public \(parentClassName) {\n")
    }

    private func getSelfTypename() -> String {
        if let typeParameters {
            let typeNames = typeParameters.map { $0.name }.joined(separator: ", ")
            return "\(cppType.declaration.name)<\(typeNames)>"
        } else {
            return cppType.declaration.name
        }
    }

    private func writeClass(classNamespace: CPPNamespace, generator: CppCodeGenerator, cppProperties: [CppProperty]) throws {
        let parentClassName = CppFileGenerator.getValdiTypeDeclaration(typeName: "CppGeneratedModel").resolveTypeName(inNamespace: classNamespace)
        let exceptionTrackerTypeDeclaration = CppFileGenerator.getValdiTypeDeclaration(typeName: "ExceptionTracker")
        let valueTypeDeclaration = CppFileGenerator.getValdiTypeDeclaration(typeName: "Value")

        generator.header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: exceptionTrackerTypeDeclaration, typeArguments: nil))
        generator.header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: valueTypeDeclaration, typeArguments: nil))

        let exceptionTrackerType = CPPTypeNameResolver.with(specifiers: .ref, .with(typeDeclaration: exceptionTrackerTypeDeclaration))
        let valueType = CPPTypeNameResolver.with(typeDeclaration: valueTypeDeclaration)

        let (constructorsDecl, constructorsImpl) = generateClassConstructors(classNamespace: classNamespace, cppProperties: cppProperties, parentClassName: parentClassName)
        let (methodsDecl, methodsImpl) = generateClassMethods(classNamespace: classNamespace, cppProperties: cppProperties)
        let fieldsDeclaration = generateClassFields(classNamespace: classNamespace, cppProperties: cppProperties)
        let (registerClassHeader, registerClassBody) = try generateRegisterClass(generator: generator)

        let selfTypeName = getSelfTypename()

        /* Header generation */
        writeClassPrologue(parentClassName: parentClassName, header: generator.header.body)
        generator.header.body.appendBody("public:\n")
        if !generator.typealiases.isEmpty {
            for cppTypealias in generator.typealiases {
                generator.header.body.appendBody(cppTypealias.statement.resolve(classNamespace))
            }
            generator.header.body.appendBody("\n")
        }

        generator.header.body.appendBody(constructorsDecl)
        generator.header.body.appendBody("\n")
        generator.header.body.appendBody(methodsDecl)
        generator.header.body.appendBody(registerClassHeader)

        let marshallWriter = makeClassWriter()
        let fieldsReferenceArguments = generateFieldsReferenceArgument(cppProperties: cppProperties)

        marshallWriter.writeMethod(name: "unmarshall",
                                   arguments: [
                                        CPPFunctionArgument(typeResolver: exceptionTrackerType, name: "exceptionTracker"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .constRef, valueType), name: "value"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .ref, .with(typeName: selfTypeName)), name: "out")
                                   ],
                                   returnType: .with(typeName: "void"),
                                   specifiers: .static_) { writer in
            if !cppProperties.isEmpty {
                writer.appendBody("auto &self = out;\n")
            }

            writer.appendBody("Valdi::CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value\(fieldsReferenceArguments));\n")
        }

        marshallWriter.writeMethod(name: "marshall",
                                   arguments: [
                                        CPPFunctionArgument(typeResolver: exceptionTrackerType, name: "exceptionTracker"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .constRef, .with(typeName: selfTypeName)), name: "value"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .ref, valueType), name: "out")
                                   ],
                                   returnType: .with(typeName: "void"),
                                   specifiers: .static_) { writer in
            if !cppProperties.isEmpty {
                writer.appendBody("const auto &self = value;\n")
            }
            writer.appendBody("Valdi::CppMarshaller::marshallTypedObject(exceptionTracker, *getRegisteredClass(), out\(fieldsReferenceArguments));\n")
        }

        generator.header.body.appendBody(marshallWriter.header)

        generator.header.body.appendBody("\nprivate:\n")
        generator.header.body.appendBody(fieldsDeclaration)
        generator.header.body.appendBody("};\n")

        /* Impl generation */

        generator.impl.body.appendBody(constructorsImpl)
        generator.impl.body.appendBody(methodsImpl)
        generator.impl.body.appendBody(marshallWriter.impl)
        generator.impl.body.appendBody(registerClassBody)
    }

    private func generateInterfaceMethods(classNamespace: CPPNamespace,
                                          proxyNamespace: CPPNamespace,
                                          generator: CppCodeGenerator,
                                          nameAllocator: PropertyNameAllocator,
                                          cppProperties: [CppProperty]) -> (decl: CppCodeWriter, impl: CppCodeWriter) {
        let decl = CppCodeWriter()
        let impl = CppCodeWriter()

        for cppProperty in cppProperties {
            let scopedNameAllocator = nameAllocator.scoped()

            if let cppMethod = cppProperty.typeParser.method {
                let returnTypeName = cppMethod.returnTypeNameResolver.resolve(classNamespace)
                let returnPrefix = returnTypeName == "void" ? "" : "return "

                let parameterNames = cppMethod.parameters.map { scopedNameAllocator.allocate(property: $0.name).name }
                let parameterWithTypes = cppMethod.parameters.indices.map {
                    let parameterName = parameterNames[$0]
                    let parameterType = cppMethod.parameters[$0].typeNameResolver.resolve(classNamespace)
                    return "\(parameterType) \(parameterName)"
                }.joined(separator: ", ")

                decl.appendBody("virtual \(returnTypeName) \(cppProperty.name.constructorParameterName)(\(parameterWithTypes)) = 0;\n\n")

                if cppProperty.property.type.isOptional {
                    impl.appendBody("""
                    \(returnTypeName) \(cppProperty.name.constructorParameterName)(\(parameterWithTypes)) final {
                      if (!\(cppProperty.name.fieldName)) {
                        Valdi::CppMarshaller::throwUnimplementedMethod();
                      }
                      \(returnPrefix)\(cppProperty.name.fieldName).value()(\(parameterNames.joined(separator: ", ") ));
                    }


                    """)
                } else {
                    impl.appendBody("""
                    \(returnTypeName) \(cppProperty.name.constructorParameterName)(\(parameterWithTypes)) final {
                      \(returnPrefix)\(cppProperty.name.fieldName)(\(parameterNames.joined(separator: ", ") ));
                    }


                    """)
                }
            } else {
                decl.appendBody("virtual \(cppProperty.typeParser.typeNameResolver.resolve(classNamespace)) \(cppProperty.name.constructorParameterName)() = 0;\n\n")

                impl.appendBody("""
                \(cppProperty.typeParser.typeNameResolver.resolve(proxyNamespace)) \(cppProperty.name.constructorParameterName)() final {
                  return \(cppProperty.name.fieldName);
                }


                """)
            }
        }

        return (decl, impl)
    }

    private func writeInterface(classNamespace: CPPNamespace,
                                generator: CppCodeGenerator,
                                proxyClassName: String,
                                proxyNamespace: CPPNamespace,
                                nameAllocator: PropertyNameAllocator,
                                cppProperties: [CppProperty]) throws {
        let parentClassName = CppFileGenerator.getValdiTypeDeclaration(typeName: "CppGeneratedInterface").resolveTypeName(inNamespace: classNamespace)
        let exceptionTrackerTypeDeclaration = CppFileGenerator.getValdiTypeDeclaration(typeName: "ExceptionTracker")
        let valueTypeDeclaration = CppFileGenerator.getValdiTypeDeclaration(typeName: "Value")

        generator.header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: exceptionTrackerTypeDeclaration, typeArguments: nil))
        generator.header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: valueTypeDeclaration, typeArguments: nil))

        let exceptionTrackerType = CPPTypeNameResolver.with(specifiers: .ref, .with(typeDeclaration: exceptionTrackerTypeDeclaration))
        let valueType = CPPTypeNameResolver.with(typeDeclaration: valueTypeDeclaration)

        let cppTypeRefType = CppCodeGenerator.makeRefType(typeName: .with(typeName: getSelfTypename()))
        let (registerClassHeader, registerClassBody) = try generateRegisterClass(generator: generator)

        let (methodsDecl, methodsImpl) = generateInterfaceMethods(classNamespace: classNamespace,
                                                                  proxyNamespace: proxyNamespace,
                                                                  generator: generator,
                                                                  nameAllocator: nameAllocator,
                                                                  cppProperties: cppProperties)
        let fieldsDeclaration = generateClassFields(classNamespace: proxyNamespace, cppProperties: cppProperties)

        /* Header generation */
        writeClassPrologue(parentClassName: parentClassName, header: generator.header.body)
        generator.header.body.appendBody("public:\n")
        let nonMethodsTypealiases = generator.typealiases.filter { !$0.isOnMethod }
        if !nonMethodsTypealiases.isEmpty {
            for cppTypealias in nonMethodsTypealiases {
                generator.header.body.appendBody(cppTypealias.statement.resolve(classNamespace))
            }
            generator.header.body.appendBody("\n")
        }

        let ctor = makeClassWriter()

        generator.header.body.appendBody(ctor.header)
        generator.header.body.appendBody(methodsDecl)
        generator.header.body.appendBody(registerClassHeader)

        let marshallWriter = makeClassWriter()

        generator.header.body.appendBody(marshallWriter.header)
        generator.header.body.appendBody("};\n")

        ctor.writeConstructor(arguments: [], memberInitializerList: "\(parentClassName)(getRegisteredClass())") { writer in
        }

        /* Impl generation */

        generator.impl.body.appendBody("class \(proxyClassName): public \(cppType.declaration.name) {\n")
        generator.impl.body.appendBody("public:\n")

        let methodsTypealiases = generator.typealiases.filter { $0.isOnMethod }
        if !methodsTypealiases.isEmpty {
            for cppTypealias in methodsTypealiases {
                generator.impl.body.appendBody(cppTypealias.statement.resolve(classNamespace))
            }
            generator.impl.body.appendBody("\n")
        }

        generator.impl.body.appendBody("\(proxyClassName)() = default;\n")
        generator.impl.body.appendBody("~\(proxyClassName)() override = default;\n\n")
        generator.impl.body.appendBody(methodsImpl)

        let proxyUnmarshallWriter = CppClassWriter(namespace: cppType.declaration.namespace, className: proxyClassName, header: generator.impl.body, impl: generator.impl.body, inlineImplementation: true)
        proxyUnmarshallWriter.writeMethod(name: "unmarshall",
                                          arguments: [
                                            CPPFunctionArgument(typeResolver: exceptionTrackerType, name: "exceptionTracker"),
                                            CPPFunctionArgument(typeResolver: .with(specifiers: .constRef, valueType), name: "value"),
                                            CPPFunctionArgument(typeResolver: .with(specifiers: .ref, CppCodeGenerator.makeRefType(typeName: .with(typeName: proxyClassName))), name: "out")
                                          ],
                                          returnType: .with(typeName: "void"),
                                          specifiers: .static_) { writer in
            let fieldsReferenceArguments = generateFieldsReferenceArgument(cppProperties: cppProperties)
            writer.appendBody("out = Valdi::makeShared<\(proxyClassName)>();\n")
            if !cppProperties.isEmpty {
                writer.appendBody("auto &self = *out;\n")
            }
            writer.appendBody("Valdi::CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value\(fieldsReferenceArguments));\n")
        }

        marshallWriter.writeMethod(name: "unmarshall",
                                   arguments: [
                                        CPPFunctionArgument(typeResolver: exceptionTrackerType, name: "exceptionTracker"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .constRef, valueType), name: "value"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .ref, cppTypeRefType), name: "out")
                                   ],
                                   returnType: .with(typeName: "void"),
                                   specifiers: .static_) { writer in
            writer.appendBody("Valdi::CppMarshaller::unmarshallProxyObject<\(proxyClassName)>(exceptionTracker, *getRegisteredClass(), value, out);\n")
        }

        marshallWriter.writeMethod(name: "marshall",
                                   arguments: [
                                        CPPFunctionArgument(typeResolver: exceptionTrackerType, name: "exceptionTracker"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .constRef, cppTypeRefType), name: "value"),
                                        CPPFunctionArgument(typeResolver: .with(specifiers: .ref, valueType), name: "out")
                                   ],
                                   returnType: .with(typeName: "void"),
                                   specifiers: .static_) { writer in
            let methodsReferenceArguments = generateMethodsReferenceArgument(receiverArgument: "value", cppProperties: cppProperties)
            writer.appendBody("""
                Valdi::CppMarshaller::marshallProxyObject(exceptionTracker, *getRegisteredClass(), *value, out, [&]() {
                   Valdi::CppMarshaller::marshallTypedObject(exceptionTracker, *getRegisteredClass(), out\(methodsReferenceArguments));
                });

                """)
        }

        generator.impl.body.appendBody("private:\n")
        generator.impl.body.appendBody(fieldsDeclaration)
        generator.impl.body.appendBody("};\n\n")

        generator.impl.body.appendBody(ctor.impl)

        generator.impl.body.appendBody(marshallWriter.impl)

        generator.impl.body.appendBody(registerClassBody)
    }

    func write() throws -> [NativeSource] {
        let namespace = cppType.declaration.namespace
        let classNamespace = namespace.appending(component: self.cppType.declaration.name)

        let nameAllocator = PropertyNameAllocator.forCpp()
        let proxyClassName = nameAllocator.allocate(property: "\(cppType.declaration.name)Proxy")

        let proxyNamespace = namespace.appending(component: proxyClassName.name)

        let generator = CppCodeGenerator(namespace: namespace,
                                         selfIncludePath: cppType.includePath,
                                         namespaceResolver: CppCodeGeneratorModelNamespaceResolver(classNamespace: classNamespace,
                                                                                                   proxyNamespace: proxyNamespace,
                                                                                                   isInterface: isInterface))
        ["getRegisteredClass", "registeredClass"].forEach {
            _ = nameAllocator.allocate(property: $0)
        }

        generator.header.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp")

        generator.impl.includeSection.addInclude(path: cppType.includePath)
        if typeParameters != nil {
            generator.header.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppMarshaller.hpp")
        } else {
            generator.impl.includeSection.addInclude(path: "valdi_core/cpp/Marshalling/CppMarshaller.hpp")
        }

        let cppProperties = try resolveProperties(codeGenerator: generator, nameAllocator: nameAllocator)

        if isInterface {
            try writeInterface(classNamespace: classNamespace,
                               generator: generator,
                               proxyClassName: proxyClassName.name,
                               proxyNamespace: proxyNamespace,
                               nameAllocator: nameAllocator,
                               cppProperties: cppProperties)
        } else {
            try writeClass(classNamespace: classNamespace,
                           generator: generator,
                           cppProperties: cppProperties)
        }

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
