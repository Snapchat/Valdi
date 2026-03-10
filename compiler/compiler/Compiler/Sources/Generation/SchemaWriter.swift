//
//  File.swift
//
//
//  Created by Simon Corsin on 1/23/23.
//

import Foundation

protocol SchemaWriterListener: AnyObject {
    func getClassName(nodeMapping: ValdiNodeClassMapping, typeArguments: [ValdiModelPropertyType]?) throws -> String?
}

class SchemaWriter {

    private(set) var str = ""
    private let listener: SchemaWriterListener
    private let typeParameters: [ValdiTypeParameter]?
    private let alwaysBoxFunctionParametersAndReturnValue: Bool
    private let boxIntEnums: Bool

    init(typeParameters: [ValdiTypeParameter]?,
         listener: SchemaWriterListener,
         alwaysBoxFunctionParametersAndReturnValue: Bool,
         boxIntEnums: Bool) {
        self.typeParameters = typeParameters
        self.listener = listener
        self.alwaysBoxFunctionParametersAndReturnValue = alwaysBoxFunctionParametersAndReturnValue
        self.boxIntEnums = boxIntEnums
    }

    func appendComma() {
        str.append(",")
    }

    func appendClass(_ clsName: String, properties: [ValdiModelProperty]) throws {
        str.append("c '\(clsName)'")
        try appendProperties(properties: properties, isMethod: false)
    }

    func appendInterface(_ clsName: String, properties: [ValdiModelProperty]) throws {
        str.append("c+ '\(clsName)'")
        try appendProperties(properties: properties, isMethod: true)
    }

    func appendStringEnum(_ enumName: String, enumCases: [EnumCase<String>]) throws {
        try appendEnum(enumName, enumTypename: "s", enumCases: enumCases) { value in
            return "'\(value)'"
        }
    }

    func appendIntEnum(_ enumName: String, enumCases: [EnumCase<Int>]) throws {
        try appendEnum(enumName, enumTypename: "i", enumCases: enumCases) { value in
            return "\(value)"
        }
    }

    private func appendEnum<T>(_ enumName: String, enumTypename: String, enumCases: [EnumCase<T>], caseToString: (T) -> String) throws {
        str.append("e<\(enumTypename)> '\(enumName)'")
        try appendList(list: enumCases, startDelimiter: "{", endDelimiter: "}", handle: { enumCase in
            appendPropertyName(enumCase.name)
            str.append(caseToString(enumCase.value))
        })
    }

    func appendFunction(returnType: ValdiModelPropertyType, parameters: [ValdiModelProperty], isOptional: Bool, isMethod: Bool, isSingleCall: Bool, shouldCallOnWorkerThread: Bool) throws {
        doAppendTypeName("f", boxed: false, isOptional: isOptional)

        if isMethod || isSingleCall || shouldCallOnWorkerThread {
            var modifiers = [String]()
            if isMethod {
                modifiers.append("m")
            }
            if isSingleCall {
                modifiers.append("s")
            }
            if shouldCallOnWorkerThread {
                modifiers.append("w")
            }

            try appendList(list: modifiers, startDelimiter: "|", endDelimiter: "|", handle: { modifier in
                str.append(modifier)
            })
        }

        let shouldBoxParametersAndReturnValue = !isMethod && self.alwaysBoxFunctionParametersAndReturnValue
        try appendList(list: parameters, startDelimiter: "(", endDelimiter: ")") { parameter in
            try appendType(parameter.type, asBoxed: shouldBoxParametersAndReturnValue, isMethod: false)
        }

        if !returnType.isVoid {
            str.append(": ")
            try appendType(returnType, asBoxed: shouldBoxParametersAndReturnValue, isMethod: false)
        }
    }

    func appendTypeRef(nodeMapping: ValdiNodeClassMapping, boxed: Bool, isOptional: Bool, hasConverter: Bool) throws {
        guard let className = try listener.getClassName(nodeMapping: nodeMapping, typeArguments: nil) else {
            doAppendTypeName("u", boxed: false, isOptional: isOptional)
            return
        }

        doAppendTypeName("r", boxed: boxed, isOptional: isOptional)
        if nodeMapping.kind == .enum {
            str.append("<e>")
        } else if hasConverter {
            str.append("<c>")
        }

        str.append(":'")
        str.append(className)
        str.append("'")
    }

    func appendGenTypeRef(nodeMapping: ValdiNodeClassMapping,
                          isOptional: Bool,
                          hasConverter: Bool,
                          typeArguments: [ValdiModelPropertyType]) throws {
        guard let className = try listener.getClassName(nodeMapping: nodeMapping, typeArguments: typeArguments) else {
            doAppendTypeName("u", boxed: false, isOptional: isOptional)
            return
        }

        doAppendTypeName("g", boxed: false, isOptional: isOptional)
        if hasConverter {
            str.append("<c>")
        }
        str.append(":'")
        str.append(className)
        str.append("'")
        try appendList(list: typeArguments, startDelimiter: "<", endDelimiter: ">") { item in
            try appendType(item, asBoxed: true, isMethod: false)
        }
    }

    func appendPromise(isOptional: Bool, typeArgument: ValdiModelPropertyType) throws {
        doAppendTypeName("p", boxed: false, isOptional: isOptional)
        str.append("<")
        try appendType(typeArgument, asBoxed: true, isMethod: false)
        str.append(">")
    }

    private func doAppendTypeName(_ name: String, boxed: Bool, isOptional: Bool) {
        str.append(name)
        if boxed {
            str.append("@")
        }
        if isOptional {
            str.append("?")
        }
    }

    func appendType(_ type: ValdiModelPropertyType, asBoxed: Bool, isMethod: Bool) throws {
        let innerType = type.unwrappingOptional
        let isOptional = type.isOptional

        switch innerType {
        case .string:
            doAppendTypeName("s", boxed: false, isOptional: isOptional)
        case .double:
            doAppendTypeName("d", boxed: asBoxed || isOptional, isOptional: isOptional)
        case .bool:
            doAppendTypeName("b", boxed: asBoxed || isOptional, isOptional: isOptional)
        case .long:
            doAppendTypeName("l", boxed: asBoxed || isOptional, isOptional: isOptional)
        case .array(elementType: let elementType):
            doAppendTypeName("a", boxed: false, isOptional: isOptional)
            str.append("<")
            try appendType(elementType, asBoxed: true, isMethod: false)
            str.append(">")
        case .bytes:
            doAppendTypeName("t", boxed: false, isOptional: isOptional)
        case .map(keyType: let keyType, valueType: let valueType):
            doAppendTypeName("m", boxed: false, isOptional: isOptional)
            str.append("<")
            try appendType(keyType, asBoxed: true, isMethod: false)
            str.append(",")
            try appendType(valueType, asBoxed: true, isMethod: false)
            str.append(">")
        case .any:
            doAppendTypeName("u", boxed: false, isOptional: isOptional)
        case .void:
            doAppendTypeName("v", boxed: false, isOptional: isOptional)
        case .function(parameters: let parameters, returnType: let returnType, isSingleCall: let isSingleCall, shouldCallOnWorkerThread: let shouldCallOnWorkerThread):
            try appendFunction(returnType: returnType, parameters: parameters, isOptional: isOptional, isMethod: isMethod, isSingleCall: isSingleCall, shouldCallOnWorkerThread: shouldCallOnWorkerThread)
        case .object(let nodeMapping):
            try appendTypeRef(nodeMapping: nodeMapping, boxed: false, isOptional: isOptional, hasConverter: nodeMapping.converter != nil)
        case .genericTypeParameter(name: let name):
            guard let index = typeParameters?.firstIndex(where: { item in
                item.name == name
            }) else {
                throw CompilerError("Could not match generic type parameter \(name) to given type parameters \(self.typeParameters ?? [])")
            }

            doAppendTypeName("r", boxed: false, isOptional: isOptional)
            str.append(":\(index)")
        case .genericObject(let nodeMapping, let typeArguments):
            try appendGenTypeRef(nodeMapping: nodeMapping, isOptional: isOptional, hasConverter: nodeMapping.converter != nil, typeArguments: typeArguments)
        case .promise(typeArgument: let typeArgument):
            try appendPromise(isOptional: isOptional, typeArgument: typeArgument)
        case .enum(let e):
            let shouldBoxEnum = e.kind == .enum && self.boxIntEnums && (asBoxed || isOptional)
            try appendTypeRef(nodeMapping: e, boxed: shouldBoxEnum, isOptional: isOptional, hasConverter: false)
        case .nullable:
            fatalError()
        }
    }

    func appendProperties(properties: [ValdiModelProperty], isMethod: Bool) throws {
        try appendList(list: properties, startDelimiter: "{", endDelimiter: "}") { property in
            try appendProperty(property: property, isMethod: isMethod)
        }
    }

    func appendPropertyName(_ propertyName: String) {
        str.append("'\(propertyName)':")
    }

    func appendProperty(property: ValdiModelProperty, isMethod: Bool) throws {
        appendPropertyName(property.name)
        try appendType(property.type, asBoxed: false, isMethod: isMethod)
    }

    func appendEnumValue(_ value: String) {
        str.append(value)
    }

    private func appendList<T>(list: [T],
                               startDelimiter: String,
                               endDelimiter: String,
                               handle: (T) throws -> Void) throws {
        str.append(startDelimiter)
        var first = true
        for item in list {
            if !first {
                str.append(", ")
            }
            first = false
            try handle(item)
        }
        str.append(endDelimiter)
    }
}
