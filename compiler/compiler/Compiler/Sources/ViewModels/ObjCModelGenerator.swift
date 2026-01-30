//
//  ObjCViewModelGenerator.swift
//  Compiler
//
//  Created by Simon Corsin on 5/17/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

final class ObjCModelGenerator {
    private let iosType: IOSType
    private let bundleInfo: CompilationItem.BundleInfo
    private let className: String
    private let typeParameters: [ValdiTypeParameter]?
    private let properties: [ValdiModelProperty]
    private let classMapping: ResolvedClassMapping
    private let sourceFileName: GeneratedSourceFilename
    private let isInterface: Bool
    private let emitLegacyConstructors: Bool
    private let comments: String?

    init(iosType: IOSType, bundleInfo: CompilationItem.BundleInfo, typeParameters: [ValdiTypeParameter]?, properties: [ValdiModelProperty], classMapping: ResolvedClassMapping, sourceFileName: GeneratedSourceFilename, isInterface: Bool, emitLegacyConstructors: Bool, comments: String?) {
        self.iosType = iosType
        self.bundleInfo = bundleInfo
        self.className = iosType.name.replacingOccurrences(of: "-", with: "_")
        self.typeParameters = typeParameters
        self.properties = properties
        self.classMapping = classMapping
        self.sourceFileName = sourceFileName
        self.isInterface = isInterface
        self.emitLegacyConstructors = emitLegacyConstructors
        self.comments = comments
    }

    private struct ConstructorParameter {
        let param: ObjcMessageParameter
        let isOptional: Bool
        let isOmitted: Bool
    }

    private func sanitizePropertyName(_ name: String) -> String {
        if name.hasPrefix("new") {
            return "_\(name)"
        }
        return name
    }

    private func resolveProperties(valdiProperties: [ValdiModelProperty], nameAllocator: PropertyNameAllocator) -> [ObjCProperty] {
        return valdiProperties.map {
            let sanitizedName = sanitizePropertyName($0.name)
            let propertyName = nameAllocator.allocate(property: sanitizedName).name

            return ObjCProperty(propertyName: propertyName, modelProperty: $0)
        }
    }

    private func generateCode(for properties: [ValdiModelProperty], className: String) throws -> GeneratedCode {
        let classGenerator = ObjCClassGenerator(className: className)

        let nameAllocator = classGenerator.nameAllocator

        _ = nameAllocator.allocate(property: className)

        // TODO(3521): Update to valdi_core
        classGenerator.apiHeader.addImport(path: "<valdi_core/SCValdiMarshallable.h>")
        classGenerator.apiHeader.addImport(path: "<valdi_core/SCValdiMarshallableObject.h>")

        // FIXME: import apiHeader into header
        classGenerator.apiImpl.addImport(path: "<valdi_core/SCValdiMarshallableObjectDescriptor.h>")

        classGenerator.apiHeader.appendBody("\n")
        classGenerator.apiHeader.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: sourceFileName, additionalComments: self.comments))
        classGenerator.apiHeader.appendBody("\n")

        classGenerator.impl.appendBody("\n")

        let resolvedProperties = resolveProperties(valdiProperties: properties, nameAllocator: nameAllocator)

        let classHeaderBody = ObjCCodeGenerator()
        let classImplBody = ObjCCodeGenerator()

        let isGenericType = self.typeParameters != nil

        let instanceType: String

        if isInterface {
            classGenerator.impl.appendBody(classGenerator.emittedFunctions.emittedFunctionsSection)
            classGenerator.apiHeader.appendBody("@protocol \(className) <NSObject, SCValdiMarshallable>\n\n")
            classGenerator.apiHeader.appendBody(classHeaderBody)
            classGenerator.apiHeader.appendBody("@end\n\n")

            classGenerator.impl.appendBody("@interface \(className): SCValdiProxyMarshallableObject\n\n")
            classGenerator.impl.appendBody("@end\n\n")

            instanceType =  "id<\(className)> _Nonnull "

            // We also forward declare the protocol or class here in case if the typedef uses the class/protocol itself
            classGenerator.apiHeader.addForwardDeclaration(type: "@protocol \(className);")
        } else {
            instanceType = "\(className) * _Nonnull "

            classGenerator.apiImpl.appendBody(classGenerator.emittedFunctions.emittedFunctionsSection)

            let typeParametersSuffix: String
            if let typeParameters = self.typeParameters {
                let typeParametersList = typeParameters.map { "__covariant \($0.name)" }.joined(separator: ", ")
                typeParametersSuffix = "<\(typeParametersList)>"
            } else {
                typeParametersSuffix = ""
            }
            classGenerator.apiHeader.addForwardDeclaration(type: "@class \(className)\(typeParametersSuffix);")

            for property in resolvedProperties {
                classImplBody.appendBody("@dynamic \(property.propertyName);\n")
            }
            classImplBody.appendBody("\n")

            let supertype = isGenericType ? "SCValdiMarshallableGenericObject" : "SCValdiMarshallableObject"
            classGenerator.apiHeader.appendBody("@interface \(className)\(typeParametersSuffix): \(supertype)\n\n")
            classGenerator.apiHeader.appendBody(classHeaderBody)
            classGenerator.apiHeader.appendBody("@end\n\n")
        }

        if let typeParameters = self.typeParameters {
            classGenerator.apiImpl.appendBody("// Obj-C lightweight generics do not exist in class implementations\n")
            for typeParam in typeParameters {
                classGenerator.apiImpl.appendBody("typedef id \(typeParam.name);\n")
            }
        }

        if isInterface {
            classGenerator.impl.appendBody("@implementation \(className)\n\n")
            classGenerator.impl.appendBody(classImplBody)
            classGenerator.impl.appendBody("@end\n\n")
        } else {
            classGenerator.apiImpl.appendBody("@implementation \(className)\n\n")
            classGenerator.apiImpl.appendBody(classImplBody)
            classGenerator.apiImpl.appendBody("@end\n\n")
        }

        let marshallFunctionName = "\(className)Marshall"
        let unmarshallFunctionName = "\(className)Unmarshall"

        _ = nameAllocator.allocate(property: unmarshallFunctionName)
        _ = nameAllocator.allocate(property: marshallFunctionName)

        var resolvedParameters = [ConstructorParameter]()
        var initMethods: [ObjCSelector] = []

        var allTypeParsers = [ObjCTypeParser]()

        for objcProperty in resolvedProperties {
            let property = objcProperty.modelProperty
            do {
                let typeParser = try classGenerator.writeTypeParser(type: property.type.unwrappingOptional,
                                                                    isOptional: property.type.isOptional,
                                                                    namePaths: [property.name],
                                                                    allowValueType: true,
                                                                    isInterface: isInterface,
                                                                    nameAllocator: nameAllocator.scoped())

                allTypeParsers.append(typeParser)

                let constructorOmitted = property.omitConstructor?.ios == true
                resolvedParameters.append(ConstructorParameter(
                    param: ObjcMessageParameter(name: objcProperty.propertyName, type: typeParser.typeName),
                    isOptional: property.type.isOptional,
                    isOmitted: constructorOmitted
                ))

                if typeParser.objcSelector != nil {
                    let disclaimerComment = "NOTE: This method can be called from any thread."
                    if let comments = property.comments {
                        classHeaderBody.appendBody(FileHeaderCommentGenerator.generateMultilineComment(comment: "\(comments)\n\(disclaimerComment)"))
                    } else {
                        classHeaderBody.appendBody(FileHeaderCommentGenerator.generateMultilineComment(comment: disclaimerComment))
                    }
                    classHeaderBody.appendBody("\n")
                    if property.type.isOptional {
                        classHeaderBody.appendBody("@optional\n")
                    } else {
                        classHeaderBody.appendBody("@required\n")
                    }
                    classHeaderBody.appendBody("\(typeParser.typeName);\n\n")
                } else {
                    if let comments = property.comments {
                        classHeaderBody.appendBody(FileHeaderCommentGenerator.generateMultilineComment(comment: comments))
                        classHeaderBody.appendBody("\n")
                    }
                    if let propertyAdditionalHeaderDeclaration = typeParser.propertyAdditionalHeaderDeclaration {
                        classHeaderBody.appendBody("\(propertyAdditionalHeaderDeclaration)\n")
                    }
                    if isInterface {
                        if property.type.isOptional {
                            classHeaderBody.appendBody("@optional\n")
                        } else {
                            classHeaderBody.appendBody("@required\n")
                        }
                    }
                    classHeaderBody.appendBody("@property (\(typeParser.propertyAttribute), nonatomic) \(typeParser.typeName) \(objcProperty.propertyName);\n\n")
                }
            } catch let error {
                throw CompilerError("Failed to process property '\(property.name)': \(error.legibleLocalizedDescription)")
            }
        }

        if isInterface {

        } else {

            if emitLegacyConstructors {
                // Generate constructors using the legacy technique
                let allParameters = resolvedParameters.map { $0.param }
                let initSelector = ObjCSelector(returnType: "instancetype _Nonnull", methodName: "init", parameters: allParameters)
                initMethods.append(initSelector)
                // Add an initializer for every @ConstructorOmitted, by going backward and removing them one by one
                var parametersWithoutOmitted = resolvedParameters
                while let lastOmittedIndex = parametersWithoutOmitted.lastIndex(where: { $0.isOmitted }) {
                    parametersWithoutOmitted.remove(at: lastOmittedIndex)
                    initMethods.append(ObjCSelector(returnType: "instancetype _Nonnull", methodName: "init", parameters: parametersWithoutOmitted.map { $0.param }))
                }
            } else {
                // Generate constructors using the new recommended minimal constructors technique
                var parametersWithoutOptionals = resolvedParameters
                while let lastOptionalIndex = parametersWithoutOptionals.lastIndex(where: { $0.isOptional }) {
                    parametersWithoutOptionals.remove(at: lastOptionalIndex)
                }
                initMethods.append(ObjCSelector(returnType: "instancetype _Nonnull", methodName: "init", parameters: parametersWithoutOptionals.map { $0.param }))
            }

            for initMethod in initMethods {
                if !initMethod.parameters.isEmpty {
                    let parametersSet = Set(initMethod.parameters.map { $0.name })

                    var passParameters = [String]()

                    for (index, property) in resolvedProperties.enumerated() {
                        if parametersSet.contains(property.propertyName) {
                            passParameters.append(property.propertyName)
                        } else {
                            passParameters.append(allTypeParsers[index].cType.defaultValue)
                        }
                    }

                    let initMethodContents: String = "return [super initWithFieldValues:nil, \(passParameters.joined(separator: ", "))];"

                    classImplBody.appendBody("""
                    \(initMethod.messageDeclaration)
                    {
                        \(initMethodContents)
                    }
                    \n
                    """)
                } else {
                    classImplBody.appendBody("""
                        \(initMethod.messageDeclaration)
                        {
                        return [super init];
                        }
                        \n
                        """)
                }

                classHeaderBody.appendBody("\(initMethod.messageDeclaration);\n\n")
            }
        }

        let objectDescriptor = try classGenerator.writeObjectDescriptorGetter(resolvedProperties: resolvedProperties,
                                                                              objcSelectors: allTypeParsers.map { $0.objcSelector },
                                                                              typeParameters: self.typeParameters,
                                                                              objectDescriptorType: isInterface ? "SCValdiMarshallableObjectTypeProtocol" : "SCValdiMarshallableObjectTypeClass")

        classImplBody.appendBody("\n")
        classImplBody.appendBody(objectDescriptor)

        if !isGenericType && isInterface {
            let marshallFunctionSignature = "NSInteger \(marshallFunctionName)(SCValdiMarshallerRef  _Nonnull marshaller, \(instanceType)instance)"

            classGenerator.header.appendBody("""
            /**
            * Marshall the instance into the given SCValdiMarshaller.
            */
            FOUNDATION_EXPORT \(marshallFunctionSignature);
            \n
            """)

            classGenerator.impl.appendBody("""
                \(marshallFunctionSignature) {
                    return SCValdiMarshallableObjectMarshallInterface(marshaller, instance, [\(className) class]);
                }
                \n
                """)
        }

        return GeneratedCode(
            apiHeader: classGenerator.apiHeader,
            apiImpl: classGenerator.apiImpl,
            header: classGenerator.header,
            impl: classGenerator.impl
        )
    }

    func write() throws -> [NativeSource] {
        let generatedCode = try generateCode(for: properties, className: className)
        let nativeSources = try NativeSource.iosNativeSourcesFromGeneratedCode(generatedCode,
                                                                               iosType: iosType,
                                                                               bundleInfo: bundleInfo)
        return nativeSources
    }

}
