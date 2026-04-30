//
//  File.swift
//  
//
//  Created by Simon Corsin on 6/5/20.
//

import Foundation

enum CType {
    case void
    case pointer
    case double
    case bool
    case int
    case long
    case nsobject

    var typeEncoding: String {
        switch self {
        case .void:
            return "v"
        case .pointer:
            return "o"
        case .double:
            return "d"
        case .bool:
            return "b"
        case .int:
            return "i"
        case .long:
            return "l"
        case .nsobject:
            return "o"
        }
    }

    var cName: String {
        switch self {
        case .void:
            return "void"
        case .pointer:
            return "const void *"
        case .double:
            return "double"
        case .bool:
            return "BOOL"
        case .int:
            return "int32_t"
        case .long:
            return "int64_t"
        case .nsobject:
            return "id"
        }
    }

    var makeFieldValueFunctionName: String {
        switch self {
        case .void:
            return "SCValdiFieldValueMakeNull"
        case .pointer:
            return "SCValdiFieldValueMakePtr"
        case .double:
            return "SCValdiFieldValueMakeDouble"
        case .bool:
            return "SCValdiFieldValueMakeBool"
        case .int:
            return "SCValdiFieldValueMakeInt"
        case .long:
            return "SCValdiFieldValueMakeLong"
        case .nsobject:
            return "SCValdiFieldValueMakeUnretainedObject"
        }
    }

    var getFieldValueFunctionName: String {
        switch self {
        case .void:
            return "void"
        case .pointer:
            return "SCValdiFieldValueGetPtr"
        case .double:
            return "SCValdiFieldValueGetDouble"
        case .bool:
            return "SCValdiFieldValueGetBool"
        case .int:
            return "SCValdiFieldValueGetInt"
        case .long:
            return "SCValdiFieldValueGetLong"
        case .nsobject:
            return "SCValdiFieldValueGetObject"
        }
    }

    var defaultValue: String {
        switch self {
            case .void:
                return "void"
            case .pointer:
                return "nil"
            case .double:
                return "0.0"
            case .bool:
                return "FALSE"
            case .int:
                return "0"
            case .long:
                return "0"
            case .nsobject:
                return "nil"
        }
    }

    var convertingPointerToNSObject: CType {
        if case .pointer = self {
            return .nsobject
        }
        return self
    }
}

enum CFunctionType {
    case block
    case objcMethod
}

struct ObjCTrampolineCallArgument {
    let name: String
    let type: CType
}

class ObjCEmittedTrampolineFunctions {

    /**
     Type signature that the runtime already supports.
     For those we don't need to emit trampoline funcitons
     */
    static let builtinTypeSignatures: Set<String> = [
        "o:v", "oo:v", "ooo:v", "oooo:v", "ooooo:v", "oooooo:v",
        "o:o", "oo:o", "ooo:o", "oooo:o",
        "o:b", "oo:b", "ooo:b",
        "o:d", "oo:d", "ooo:d"
    ]

    let emittedFunctionsSection = ObjCCodeGenerator()
    let blocksSection = ObjCCodeGenerator()

    var needBlockSupport: Bool {
        return !emittedBlockSupportSignatures.isEmpty
    }

    private var emittedBlockSupportSignatures: Set<String> = []

    func appendBlockSupportIfNeeded(parameters: [CType], returnType: CType, functionType: CFunctionType) {
        let resolvedParameters: [CType]
        switch functionType {
        case .block:
            // The first parameter of the block in the Objective-C block ABI is always the block itself
            resolvedParameters = [CType.pointer] + parameters
        case .objcMethod:
            // Objective-C methods take the receiver and the selector as the first two parameters
            resolvedParameters = [CType.pointer, CType.pointer] + parameters
        }

        let parametersTypeEncoding = resolvedParameters.map { $0.typeEncoding }.joined()

        let functionTypeSignature = "\(parametersTypeEncoding):\(returnType.typeEncoding)"

        if ObjCEmittedTrampolineFunctions.builtinTypeSignatures.contains(functionTypeSignature) {
            // The runtime already supports this type signature
            return
        }

        if emittedBlockSupportSignatures.contains(functionTypeSignature) {
            // We already emitted a trampoline function for this type signature
            return
        }
        emittedBlockSupportSignatures.insert(functionTypeSignature)

        appendTrampoline(typeEncoding: functionTypeSignature, parametersTypeEncoding: parametersTypeEncoding, parameters: resolvedParameters, returnType: returnType)
    }

    private func appendTrampoline(typeEncoding: String, parametersTypeEncoding: String, parameters: [CType], returnType: CType) {
        let functionNameSuffix = "\(parametersTypeEncoding.uppercased())_\(returnType.typeEncoding)"
        // If additional function names are added, make sure to update the logic in CombineNativeSourcesProcessor.swift
        // that looks for trampoline function definitions in order to deduplicate them (see trampolineFunctionName)
        let invokeFunctionName = "SCValdiFunctionInvoke\(functionNameSuffix)"
        let createBlockFunctionName = "SCValdiBlockCreate\(functionNameSuffix)"

        appendInvokeFunction(functionName: invokeFunctionName, parameters: parameters, returnType: returnType)
        appendCreateBlockFunction(functionName: createBlockFunctionName, parameters: parameters, returnType: returnType)
        appendBlockSupportDeclaration(typeEncoding: typeEncoding, invokeFunctionName: invokeFunctionName, createBlockFunctionName: createBlockFunctionName)
    }

    private func appendBlockSupportDeclaration(typeEncoding: String, invokeFunctionName: String, createBlockFunctionName: String) {
        blocksSection.appendBody("{ .typeEncoding = \"\(typeEncoding)\", .invoker = &\(invokeFunctionName), .factory = &\(createBlockFunctionName) },\n")
    }

    private func appendInvokeFunction(functionName: String, parameters: [CType], returnType: CType) {
        let functionBody = CodeWriter()

        emittedFunctionsSection.appendBody("static SCValdiFieldValue \(functionName)(const void *function, SCValdiFieldValue* fields) {\n")
        emittedFunctionsSection.appendBody(functionBody)
        emittedFunctionsSection.appendBody("}\n\n")

        let callArguments = parameters.enumerated().map { "\($0.element.getFieldValueFunctionName)(fields[\($0.offset)])" }.joined(separator: ", ")

        let cFunctionPointerType = "(\(returnType.cName) (*) (\(parameters.map { $0.cName }.joined(separator: ", ")) ))"

        let callExpression = "(\(cFunctionPointerType)function)(\(callArguments))"

        if case .void = returnType {
            functionBody.appendBody("\(callExpression);\n")
            functionBody.appendBody("return \(returnType.makeFieldValueFunctionName)();\n")
        } else {
            functionBody.appendBody("\(returnType.cName) result = \(callExpression);\n")
            functionBody.appendBody("return \(returnType.makeFieldValueFunctionName)(result);\n")
        }
    }

    private func appendCreateBlockFunction(functionName: String, parameters: [CType], returnType: CType) {
        let functionBody = CodeWriter()

        emittedFunctionsSection.appendBody("static id \(functionName)(SCValdiBlockTrampoline trampoline) {\n")
        emittedFunctionsSection.appendBody(functionBody)
        emittedFunctionsSection.appendBody("}\n\n")

        // The first parameter is already present in the block
        let resolvedParameters = parameters[1...]

        let callArguments = resolvedParameters.enumerated().map { ObjCTrampolineCallArgument(name: "p\($0.offset)", type: $0.element) }

        let blockArguments = callArguments.map { "\($0.type.cName) \($0.name)" }.joined(separator: ", ")

        functionBody.appendBody("return ^\(returnType.cName)(\(blockArguments)) {\n")

        ObjCEmittedTrampolineFunctions.appendTrampolineCall(output: functionBody, trampolineVar: "trampoline", receiver: "nil", arguments: callArguments, returnType: returnType)
        functionBody.appendBody("};\n")
    }

    static func appendTrampolineCall(output: CodeWriter, trampolineVar: String, receiver: String, arguments: [ObjCTrampolineCallArgument], returnType: CType) {
        if case .void = returnType {
            appendTrampolineCallBody(output: output, trampolineVar: trampolineVar, receiver: receiver, arguments: arguments)
            output.appendBody(";\n")
        } else {
            output.appendBody("return \(returnType.getFieldValueFunctionName)(")
            appendTrampolineCallBody(output: output, trampolineVar: trampolineVar, receiver: receiver, arguments: arguments)
            output.appendBody(");\n")
        }
    }

    private static func appendTrampolineCallBody(output: CodeWriter, trampolineVar: String, receiver: String, arguments: [ObjCTrampolineCallArgument]) {
        output.appendBody("\(trampolineVar)(\(receiver)")

        if !arguments.isEmpty {
            output.appendBody(", ")
            let argumentsString = arguments.map { "\($0.type.makeFieldValueFunctionName)(\($0.name))" }.joined(separator: ", ")
            output.appendBody(argumentsString)
        }

        output.appendBody(")")
    }

}
