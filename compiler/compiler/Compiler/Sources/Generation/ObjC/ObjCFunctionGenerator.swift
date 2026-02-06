//
//  ObjCFunctionGenerator.swift
//  Compiler
//
//  Created by saniul on 12/04/2019.
//

import Foundation

final class ObjCFunctionGenerator {
    private let iosType: IOSType
    private let typeName: String
    private let exportedFunction: ExportedFunction
    private let classMapping: ResolvedClassMapping
    private let sourceFileName: GeneratedSourceFilename
    private let bundleName: String
    private let modulePath: String
    private let bundleInfo: CompilationItem.BundleInfo

    init(iosType: IOSType, exportedFunction: ExportedFunction, classMapping: ResolvedClassMapping, sourceFileName: GeneratedSourceFilename, bundleName: String, modulePath: String, bundleInfo: CompilationItem.BundleInfo) {
        self.iosType = iosType
        self.typeName = iosType.name
        self.exportedFunction = exportedFunction
        self.classMapping = classMapping
        self.sourceFileName = sourceFileName
        self.bundleName = bundleName
        self.modulePath = modulePath
        self.bundleInfo = bundleInfo
    }

    private func generateCode() throws -> GeneratedCode {
        let classGenerator = ObjCClassGenerator(className: typeName)

        // TODO(3521): Update to valdi_core
        classGenerator.header.addImport(path: "<valdi_core/SCValdiBridgeFunction.h>")
        classGenerator.impl.addImport(path: "<valdi_core/SCValdiMarshallableObjectRegistry.h>")

        let callBlockVariable = classGenerator.nameAllocator.allocate(property: "callBlock")

        let functionParser = try classGenerator.writeFunctionTypeParser(returnType: exportedFunction.returnType, parameters: exportedFunction.parameters, namePaths: [], isInterface: false, includesGenericType: false, nameAllocator: classGenerator.nameAllocator)

        let objcSelector = ObjCSelector(returnType: functionParser.returnParser.typeName,
                                        methodName: exportedFunction.functionName,
                                        parameters: functionParser.parameters.map { ObjcMessageParameter(name: $0.name.name, type: $0.parser.typeName) })

        let comments = FileHeaderCommentGenerator.generateComment(sourceFilename: sourceFileName, additionalComments: exportedFunction.comments)

        var messageDeclaration = ""
        if let comments = exportedFunction.comments, !comments.isEmpty {
            messageDeclaration = FileHeaderCommentGenerator.generateMultilineComment(comment: comments)
            messageDeclaration.append("\n")
        }
        messageDeclaration.append(objcSelector.messageDeclaration)
        messageDeclaration.append(";")

        classGenerator.header.appendBody("""
            \(comments)
            @interface \(typeName): SCValdiBridgeFunction

            \(messageDeclaration);

            @end

        """)

        let objcProperty = ObjCProperty(propertyName: exportedFunction.functionName, modelProperty:
                                            ValdiModelProperty(name: exportedFunction.functionName,
                                                                  type: .function(parameters: exportedFunction.parameters, returnType: exportedFunction.returnType, isSingleCall: false, shouldCallOnWorkerThread: false),
                                                                  comments: nil,
                                                                  omitConstructor: nil,
                                                                  injectableParams: .empty))
        let objectDescriptor = try classGenerator.writeObjectDescriptorGetter(resolvedProperties: [objcProperty],
                                                                              objcSelectors: [nil],
                                                                              typeParameters: nil,
                                                                              objectDescriptorType: "SCValdiMarshallableObjectTypeFunction")

        let messageForwarder = ObjCCodeGenerator()
        messageForwarder.appendBody(objcSelector.messageDeclaration)
        messageForwarder.appendBody("{\n")
        messageForwarder.appendBody("\(functionParser.typeName) \(callBlockVariable.name) = (\(functionParser.typeName))self.\(callBlockVariable);\n")

        let callBody = "\(callBlockVariable)(\(objcSelector.parameters.map { $0.name }.joined(separator: ", ") ))"
        if functionParser.returnParser.cType == .void {
            messageForwarder.appendBody(callBody)
        } else {
            messageForwarder.appendBody("return ")
            messageForwarder.appendBody(callBody)
        }
        messageForwarder.appendBody(";\n")
        messageForwarder.appendBody("}\n")

        classGenerator.impl.appendBody(classGenerator.emittedFunctions.emittedFunctionsSection)
        classGenerator.impl.appendBody("""
        @implementation \(typeName)

        + (NSString *)modulePath
        {
          return @"\(bundleName)/\(modulePath)";
        }

        + (BOOL)asyncStrictMode
        {
          return \(bundleInfo.asyncStrictMode ? "YES" : "NO");
        }


        """)

        classGenerator.impl.appendBody(messageForwarder)
        classGenerator.impl.appendBody("\n")
        classGenerator.impl.appendBody(objectDescriptor)
        classGenerator.impl.appendBody("\n")
        classGenerator.impl.appendBody("@end\n")

        return GeneratedCode(apiHeader: classGenerator.apiHeader, apiImpl: classGenerator.apiImpl, header: classGenerator.header, impl: classGenerator.impl)
    }

    func write() throws -> [NativeSource] {
        let generatedCode = try generateCode()
        let nativeSources = try NativeSource.iosNativeSourcesFromGeneratedCode(generatedCode,
                                                                               iosType: iosType,
                                                                               bundleInfo: bundleInfo)

        return nativeSources
    }

}
