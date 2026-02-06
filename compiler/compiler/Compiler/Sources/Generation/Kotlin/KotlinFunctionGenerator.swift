//
//  KotlinFunctionGenerator.swift
//  Compiler
//
//  Created by saniul on 12/04/2019.
//

import Foundation

final class KotlinFunctionGenerator {
    private let bundleInfo: CompilationItem.BundleInfo
    private let fullTypeName: String
    private let exportedFunction: ExportedFunction
    private let classMapping: ResolvedClassMapping
    private let sourceFileName: GeneratedSourceFilename
    private let modulePath: String

    init(bundleInfo: CompilationItem.BundleInfo,
         fullTypeName: String,
         exportedFunction: ExportedFunction,
         classMapping: ResolvedClassMapping,
         sourceFileName: GeneratedSourceFilename,
         modulePath: String) {
        self.bundleInfo = bundleInfo
        self.fullTypeName = fullTypeName
        self.exportedFunction = exportedFunction
        self.classMapping = classMapping
        self.sourceFileName = sourceFileName
        self.modulePath = modulePath
    }

    func write() throws -> [NativeSource] {
        let jvmClass = JVMClass(fullClassName: fullTypeName)
        let containingTypeName = jvmClass.name

        let generator = KotlinCodeGenerator(package: jvmClass.package, classMapping: classMapping)
        let jsRuntimeClass = try generator.importClass("com.snap.valdi.jsmodules.ValdiJSRuntime")
        let bridgeFunctionClass = try generator.importClass("com.snap.valdi.callable.ValdiBridgeFunction")

        generator.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: sourceFileName, additionalComments: exportedFunction.comments))
        generator.appendBody("\n")

        let nameAllocator = PropertyNameAllocator.forKotlin()

        let fieldName = nameAllocator.allocate(property: "_invoker").name

        let typeParser = try generator.getTypeParser(type: .function(parameters: exportedFunction.parameters, returnType: exportedFunction.returnType, isSingleCall: false, shouldCallOnWorkerThread: false), isOptional: false, propertyName: nil, nameAllocator: nameAllocator)

        guard let functionTypeParser = typeParser.functionTypeParser else {
            throw CompilerError("Expecting functionTypePArser")
        }

        let descriptorAnnotation = try generator.getDescriptorAnnotation(type: KotlinMarshallableObjectDescriptorAnnotation.ObjectType.valdiFunction, typeParameters: nil, additionalParameters: [])

        let constructorAnnotationContent = try generator.getJavaAnnotationContent(annotationType: "com.snap.valdi.schema.ValdiClassConstructor")

        let invokeParameters = functionTypeParser.parameterNames.map { $0.name }.joined(separator: ", ")

        generator.appendBody("\n")
        generator.appendBody(descriptorAnnotation)
        generator.appendBody("class \(containingTypeName): \(bridgeFunctionClass.name) {\n\n")

        let fieldAnnotation = try descriptorAnnotation.appendField(propertyIndex: 0, propertyName: exportedFunction.functionName, kotlinFieldName: fieldName, expectedKotlinFieldName: "_invoker") { schemaWriter in
            try schemaWriter.appendFunction(returnType: exportedFunction.returnType, parameters: exportedFunction.parameters, isOptional: false, isMethod: false, isSingleCall: false, shouldCallOnWorkerThread: false)
        }

        generator.appendBody(fieldAnnotation)
        generator.appendBody("private var \(fieldName): \(typeParser.fullTypeName)\n\n")

        generator.appendBody(constructorAnnotationContent)
        generator.appendBody("""
        constructor(invoker: \(typeParser.fullTypeName)) {
            \(fieldName) = invoker
        }

        fun invoke\(functionTypeParser.methodTypeName) {
            return \(fieldName)(\(invokeParameters))
        }

        companion object {
            const val MODULE_PATH = "\(bundleInfo.name)/\(modulePath)"
            const val ASYNC_STRICT_MODE: Boolean = \(bundleInfo.asyncStrictMode ? "true" : "false")

            @JvmStatic
            fun create(runtime: \(jsRuntimeClass.name)): \(containingTypeName) {
              return \(bridgeFunctionClass.name).createFromRuntime(runtime, \(containingTypeName)::class.java, MODULE_PATH)
            }
        }
        }

        """)

        let data = try generator.content.indented(indentPattern: "    ").utf8Data()
        return [KotlinCodeGenerator.makeNativeSource(bundleInfo: bundleInfo, jvmClass: jvmClass, file: .data(data))]
    }

}
