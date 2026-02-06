//
//  SwiftFunctionGenerator.swift
//  Compiler
//

import Foundation

final class SwiftFunctionGenerator {
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
        self.typeName = iosType.swiftName
        self.exportedFunction = exportedFunction
        self.classMapping = classMapping
        self.sourceFileName = sourceFileName
        self.bundleName = bundleName
        self.modulePath = modulePath
        self.bundleInfo = bundleInfo
    }

    func write() throws -> [NativeSource] {
        let containingTypeName = iosType.swiftName

        let generator = SwiftSourceFileGenerator(className: containingTypeName, moduleName: iosType.importPrefix ?? "")
        // TODO(3521): Update to valdi_core
        generator.addImport(path: "valdi_core")
        generator.addImport(path: "ValdiCoreSwift")

        generator.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: sourceFileName, additionalComments: exportedFunction.comments))
        generator.appendBody("\n")

        let nameAllocator = PropertyNameAllocator.forSwift()

        let functionType : ValdiModelPropertyType = .function(parameters: exportedFunction.parameters, returnType: exportedFunction.returnType, isSingleCall: false, shouldCallOnWorkerThread: false)
        let typeParser = try generator.getTypeParser(type: functionType, isOptional: false, functionHelperName: "callBlock", nameAllocator: nameAllocator)
        // let swiftProperties = 

        let objectDescriptorGetter = try SwiftSourceFileGenerator.writeObjectDescriptorGetter(
            properties: [SwiftProperty(name: exportedFunction.functionName, typeParser: typeParser, comments: nil)],
            propertyTypes: [functionType],
            typeParameters: nil,
            objectDescriptorType: "ValdiMarshallableObjectType.Function")

        guard let functionTypeParser = typeParser.functionTypeParser else {
            throw CompilerError("Expecting functionTypePArser")
        }

        let functionHelpers = try SwiftSourceFileGenerator.writeFunctionHelpers(sourceGenerator: generator)

        generator.appendBody("""
        public class \(containingTypeName): ValdiBridgeFunction {
            private let callBlock : \(functionTypeParser.methodTypeWithoutNames)
            public static var className = "\(containingTypeName)"
            public static let asyncStrictMode: Bool = \(bundleInfo.asyncStrictMode ? "true" : "false")

            public init(jsRuntime: Any) throws {
                guard let jsRuntime = jsRuntime as? SCValdiJSRuntime else {
                    throw ValdiError.runtimeError("jsRuntime is not of type SCValdiJSRuntime")
                }
                register_\(containingTypeName)()
                self.callBlock = try Self.create_callBlock_from_bridgeFunction(using: Self.createBridgeFunction(jsRuntime: jsRuntime))
            }

            public static func modulePath() -> String {
                return "\(bundleName)/\(modulePath)";
            }
            public func \(exportedFunction.functionName)\(functionTypeParser.methodTypeWithNames) {
                return try self.callBlock(\(functionTypeParser.callableParametersString))
            }
            \(functionHelpers.content)
            \(objectDescriptorGetter.content)
        }
        public func register_\(containingTypeName)() {
            ValdiMarshallableObjectRegistry.shared.register(className: \(containingTypeName).className, objectDescriptor: \(containingTypeName).getDescriptor())
        }
        """)

        let data = try generator.content.indented(indentPattern: "    ").utf8Data()
        return [NativeSource(relativePath: nil,
                             filename: "\(containingTypeName).\(FileExtensions.swift)",
                             file: .data(data),
                             groupingIdentifier: "\(bundleInfo.iosModuleName).\(FileExtensions.swift)", groupingPriority: IOSType.functionTypeGroupingPriority)]
    }

}
