// Copyright © 2024 Snap, Inc. All rights reserved.

import Foundation

func indent(spaces count: Int) -> String {
    return "\n" + String(repeating: " ", count: count)
}

private final class SwiftImportSection: CodeWriterContent {

    private(set) var importedClasses = Set<String>()

    func addImport(path: String) {
        importedClasses.insert(path)
    }

    var content: String {
        return importedClasses.sorted().map { "import \($0)\n" }.joined()
    }
}

struct SwiftTypeParser {
    let typeName: String
    let isOptional: Bool
    let isInterface: Bool
    let functionTypeParser: SwiftFunctionTypeParser?
    let innerTypeParser: [SwiftTypeParser]?
    let typeArguments: [SwiftTypeParser]?
    let marshallerName: String?
    var needsEscaping: Bool {
        return functionTypeParser != nil && !isOptional
    }
    var isFunction: Bool {
        return functionTypeParser != nil
    }

    /// - Parameters:
    ///   - typeName: The actual name of that type in Swift (without optional mark)
    ///   - isOptional: Whether the type is optional or not.
    ///   - marshallerName: The name of the marshaller function to use for this type. it. getBool, getFunction, etc...
    ///   - innerTypeParser: The type parser for the inner type of the array, map, or promise.
    ///   - functionTypeParser: The type parser if this is a function type.
    ///   - typeArguments: The type parsers for the type arguments of the generic type.
    init(
        typeName: String,
        isOptional: Bool,
        marshallerName: String?,
        innerTypeParser: SwiftTypeParser? = nil,
        functionTypeParser: SwiftFunctionTypeParser? = nil,
        typeArguments: [SwiftTypeParser]? = nil
    ) {
        self.typeName = SwiftTypeParser.getFullTypeName(typeName: typeName, isOptional: isOptional, isFunction: (functionTypeParser != nil),  typeArguments: typeArguments)
        self.isOptional = isOptional
        self.isInterface = false
        self.functionTypeParser = functionTypeParser
        self.typeArguments = typeArguments
        self.innerTypeParser = innerTypeParser.map { [$0] }
        self.marshallerName = marshallerName.map { (isOptional ? "Optional" : "") + $0 }
    }
    init(
      typeName: String,
      isOptional: Bool,
      marshallerName: String?,
      isInterface: Bool
    ) {
        self.typeName = SwiftTypeParser.getFullTypeName(typeName: typeName, isOptional: isOptional, isFunction: false,  typeArguments: nil)
        self.isOptional = isOptional
        self.isInterface = isInterface
        self.functionTypeParser = nil
        self.typeArguments = nil
        self.innerTypeParser = nil
        self.marshallerName = marshallerName.map { (isOptional ? "Optional" : "") + $0 }
    }

    private static func getFullTypeName(typeName: String, isOptional: Bool, isFunction: Bool, typeArguments: [SwiftTypeParser]?) -> String {
        let optionalMarkIfNeeded = isOptional ? "?" : ""
        if let typeArguments = typeArguments {
            return "\(typeName)<\(typeArguments.map { $0.typeName }.joined(separator: ", "))>\(optionalMarkIfNeeded)"
        } else {
            return "\(typeName)\(optionalMarkIfNeeded)"
        }
    }
    public func typeNameRemoveOptional() -> String {
        if self.typeName.hasSuffix("?") {
            return String(self.typeName.dropLast())
        } else {
            return self.typeName
        }
    }
}

struct SwiftProperty {
    let name: String
    let typeParser: SwiftTypeParser
    let comments: String?
}

struct SwiftSchemaAnnotationResult {
    let typeReferences: CodeWriterContent
    let schema: CodeWriterContent
}

class SwiftFunctionTypeParser {
    let parameterNames: [PropertyName]
    let parameterTypes: [SwiftTypeParser]
    let returnType: SwiftTypeParser
    let functionThrows : Bool = true
    let functionHelperName : String

    init(parameterNames: [PropertyName], parameterTypes: [SwiftTypeParser], returnType: SwiftTypeParser, functionHelperName: String) {
        self.parameterNames = parameterNames
        self.parameterTypes = parameterTypes
        self.returnType = returnType
        self.functionHelperName = functionHelperName
    }

    private var parametersStringWithNames: String {
        return parameterNames.enumerated().map { index, name in
            let maybeEscaping = parameterTypes[index].needsEscaping ? "@escaping " : ""
            return "\(name): \(maybeEscaping)\(parameterTypes[index].typeName)"
        }.joined(separator: ", ")
    }

    private var parametersStringWithoutName: String {
        return parameterNames.enumerated().map { index, name in
            let maybeEscaping = parameterTypes[index].needsEscaping ? "@escaping " : ""
            return "\(maybeEscaping)\(parameterTypes[index].typeName)"
        }.joined(separator: ", ")
    }

    var callableParametersString: String {
        return parameterNames.enumerated().map { index, name in
            return "\(name)"
        }.joined(separator: ", ")
    }

    var callableParametersStringWithNames: String {
        return parameterNames.enumerated().map { index, name in
            return "\(name): \(name)"
        }.joined(separator: ", ")
    }

    var methodTypeWithNames: String {
        return "(\(parametersStringWithNames))\(functionThrows ? " throws " : " ")-> \(returnType.typeName)"
    }

    var methodTypeWithoutNames: String {
        return "(\(parametersStringWithoutName))\(functionThrows ? " throws " : " ")-> \(returnType.typeName)"
    }
}

final class SwiftCodeGenerator: CodeWriter {
    static let maxFunctionParametersCount = 16
}

final class SwiftSourceFileGenerator : CodeWriter {
    let nameAllocator = PropertyNameAllocator.forSwift()
    let moduleName: String
    let className: String
    var functionParsers: [SwiftTypeParser] = []
    private let importSection = SwiftImportSection()

    func addImport(path: String) {
        if path == moduleName {
            return
        }
        self.importSection.addImport(path: path)
    }

    init(className: String, moduleName: String = "") {
        self.moduleName = moduleName
        self.className = className
        super.init()
        self.appendHeader(importSection)
        self.appendBody("\n")
    }

    private func getFunctionTypeParser(returnType: ValdiModelPropertyType,
                               parameters: [ValdiModelProperty],
                               nameAllocator: PropertyNameAllocator,
                               functionHelperName: String) throws -> SwiftFunctionTypeParser {
        guard parameters.count <= SwiftCodeGenerator.maxFunctionParametersCount else {
            throw CompilerError("Exported functions to Swift can only support up to \(SwiftCodeGenerator.maxFunctionParametersCount) parameters.")
        }

        let parameterNames = parameters.map { nameAllocator.allocate(property: $0.name) }
        let parameterParsers = try parameters.map { try getTypeParser(type: $0.type.unwrappingOptional, isOptional: $0.type.isOptional, functionHelperName: functionHelperName + $0.name, nameAllocator: nameAllocator) }
        let returnParser = try getTypeParser(type: returnType.unwrappingOptional, isOptional: returnType.isOptional, functionHelperName: functionHelperName + "Return", nameAllocator: nameAllocator)

        return SwiftFunctionTypeParser(parameterNames: parameterNames, parameterTypes: parameterParsers, returnType: returnParser, functionHelperName: functionHelperName)
    }

    private func getGenericTypeParameterTypeParser(name: String, isOptional: Bool, nameAllocator: PropertyNameAllocator) throws -> SwiftTypeParser {
        return SwiftTypeParser(typeName: name, isOptional: isOptional, marshallerName: "GenericTypeParameter")
    }

    private func getGenericObjectTypeParser(typeName: String, mapping: ValdiNodeClassMapping, isOptional: Bool, nameAllocator: PropertyNameAllocator, typeArgumentsTypeParsers: [SwiftTypeParser]) throws -> SwiftTypeParser {
        let typeParser = SwiftTypeParser(
            typeName: typeName,
            isOptional: isOptional,
            marshallerName: "GenericObject",
            typeArguments: typeArgumentsTypeParsers
        )
        return typeParser
    }

    /// Returns the type parser for the given type.
    /// - Parameters:
    ///   - type: The type to parse.
    ///   - isOptional: Whether the type is optional or not.
    ///   - functionHelperName: Name for the helper function to generate for the function type.
    ///   - nameAllocator: The name allocator to use for naming the function type.
    func getTypeParser(type: ValdiModelPropertyType, isOptional: Bool, functionHelperName: String?, nameAllocator: PropertyNameAllocator) throws -> SwiftTypeParser {
        switch type {
        case .string:
            return SwiftTypeParser(typeName: "String", isOptional: isOptional, marshallerName: "String")
        case .double:
            return SwiftTypeParser(typeName: "Double", isOptional: isOptional, marshallerName: "Double")
        case .bool:
            return SwiftTypeParser(typeName: "Bool", isOptional: isOptional, marshallerName: "Bool")
        case .long:
            return SwiftTypeParser(typeName: "Int64", isOptional: isOptional, marshallerName: "Long")
        case .array(let elementType):
            let childParser = try getTypeParser(type: elementType.unwrappingOptional, isOptional: elementType.isOptional, functionHelperName: functionHelperName, nameAllocator: nameAllocator)
            return SwiftTypeParser(typeName: "[\(childParser.typeName)]", isOptional: isOptional, marshallerName: "Array", innerTypeParser: childParser)
        case .bytes:
            self.addImport(path: "Foundation")
            return SwiftTypeParser(typeName: "Data", isOptional: isOptional, marshallerName: "Data")
        case .map(let keyType, let valueType):
            let valueTypeParser = try getTypeParser(type: valueType.unwrappingOptional, isOptional: valueType.isOptional, functionHelperName: functionHelperName, nameAllocator: nameAllocator)
            let keyTypeParser = try getTypeParser(type: keyType.unwrappingOptional, isOptional: keyType.isOptional, functionHelperName: functionHelperName, nameAllocator: nameAllocator)
            guard keyTypeParser.typeName == "String" else {
                throw CompilerError("Maps with non-string keys are not supported in Swift")
            }
            return SwiftTypeParser(typeName: "[\(keyTypeParser.typeName): \(valueTypeParser.typeName)]", isOptional: isOptional, marshallerName: "Map", innerTypeParser: valueTypeParser)
        case .any:
            return SwiftTypeParser(typeName: "Any", isOptional: true, marshallerName: "Untyped")
        case .void:
            return SwiftTypeParser(typeName: "Void", isOptional: false, marshallerName: nil)
        case .function(let parameters, let returnType, _, _):
            guard let functionHelperName = functionHelperName else {
                throw CompilerError("Function type must have a name")
            }
            let functionTypeParser = try getFunctionTypeParser(returnType: returnType, parameters: parameters, nameAllocator: nameAllocator, functionHelperName: functionHelperName)
            let swiftTypeParser = SwiftTypeParser(typeName: "(\(functionTypeParser.methodTypeWithoutNames))", isOptional: isOptional, marshallerName: "Function", functionTypeParser: functionTypeParser)
            self.functionParsers.append(swiftTypeParser)
            return swiftTypeParser
        case .enum(let mapping):
            guard let resolvedType = mapping.iosType?.swiftName,
                  let moduleImport = mapping.iosType?.importPrefix else {
                throw CompilerError("No swift type declared")
            }
            _ = importModule(moduleImport, mapping)
            return SwiftTypeParser(typeName: resolvedType, isOptional: isOptional, marshallerName: "Enum")
        case .object(let mapping):
            guard let resolvedType = mapping.iosType?.swiftName,
                  let moduleImport = mapping.iosType?.importPrefix else {
                throw CompilerError("No swift class declared")
            }
            _ = importModule(moduleImport, mapping)

            let marshallerName: String
            switch mapping.iosType?.iosLanguage {
            case .swift, .both:
                marshallerName = "Object"
            case .objc:
                marshallerName = "ObjCObject"
            default:
                let language = mapping.iosType?.iosLanguage.rawValue ?? "Unknown"
                throw CompilerError("Unknown language for object \(resolvedType): \(language)")
            }
            switch mapping.kind {
            case .interface:
                return SwiftTypeParser(typeName: resolvedType, isOptional: isOptional, marshallerName: marshallerName, isInterface: marshallerName == "Object")
            default:
                return SwiftTypeParser(typeName: resolvedType, isOptional: isOptional, marshallerName: marshallerName)
            }
        case .genericTypeParameter(let name):
            return try getGenericTypeParameterTypeParser(name: name, isOptional: isOptional, nameAllocator: nameAllocator)
        case .genericObject(let mapping, let typeArguments):
            guard let resolvedType = mapping.iosType?.swiftName,
                  let moduleImport = mapping.iosType?.importPrefix else {
                throw CompilerError("No swift class declared")
            }

            let typeArgumentsTypeParsers = try typeArguments.map {
                try getTypeParser(type: $0, isOptional: false, functionHelperName: functionHelperName, nameAllocator: nameAllocator)
            }

            _ = importModule(moduleImport, mapping)
            return try getGenericObjectTypeParser(typeName: resolvedType, mapping: mapping, isOptional: isOptional, nameAllocator: nameAllocator, typeArgumentsTypeParsers: typeArgumentsTypeParsers)
        case .promise(let typeArgument):
            let promiseTypeName = "ValdiPromise"
            let childParser = try getTypeParser(type: typeArgument.unwrappingOptional, isOptional: typeArgument.isOptional, functionHelperName: functionHelperName, nameAllocator: nameAllocator)
            let typeName = "\(promiseTypeName)<\(childParser.typeName)>"

            return SwiftTypeParser(typeName: typeName, isOptional: isOptional, marshallerName: "Promise")
        case .nullable(let type):
            return try getTypeParser(type: type, isOptional: true, functionHelperName: functionHelperName, nameAllocator: nameAllocator)
        }
    }

    private func importModule(_ moduleName: String, _ mapping: ValdiNodeClassMapping) -> String {
        if moduleName == self.moduleName {
            return moduleName
        }
        let languageSuffix = (mapping.iosType?.iosLanguage == .both) ? "_Swift" : ""
        let importName = moduleName + languageSuffix
        self.addImport(path: importName)
        return importName
    }

    private static func marshallerCanThrow(type: SwiftTypeParser) -> Bool {
        let baseMarshallerNames = ["Array", "Map", "Object", "GenericObject", "Untyped", "Promise", "ObjCObject", "GenericTypeParameter"
        ]
        let marshallerNames = baseMarshallerNames.flatMap { [$0, "Optional\($0)"] }
        return marshallerNames.contains(type.marshallerName ?? "")
    }

    /// Returns the marshaller function for a given typeParser
    /// Parameters:
    ///   - typeParser: The typeParser for the type
    ///   - parameter: The parameter to pass to the marshaller function
    private static func getMarshallerFunction(for typeParser: SwiftTypeParser, withParameter parameter: String) -> String {
        guard let marshallingName = typeParser.marshallerName else {
            return ""
        }

        let maybeTry = marshallerCanThrow(type: typeParser) ? "try " : ""
        let innerMarshaller = typeParser.innerTypeParser.map { innerTypeParser in
            " { \(getMarshallerFunction(for: innerTypeParser[0], withParameter: "$0")) }"
        } ?? ""

        var parameter = parameter
        if let functionHelperName = typeParser.functionTypeParser?.functionHelperName {
            parameter = "Self.create_\(functionHelperName)_bridgeFunction(using: \(parameter))"
        }
        return "\(maybeTry)marshaller.push" + marshallingName + "(\(parameter))" + innerMarshaller
    }


    /// Returns the unmarshaller function for a given typeParser
    /// Parameters:
    ///   - typeParser: The typeParser for the type
    ///   - parameter: The parameter to pass to the unmarshaller function
    private static func getUnmarshallerFunction(for typeParser: SwiftTypeParser, withParameter parameter: String) -> String {
        guard let marshallingName = typeParser.marshallerName else {
            return ""
        }

        if typeParser.isInterface {
            if typeParser.isOptional {
                let proxyUnmarshaller = "try \(typeParser.typeNameRemoveOptional())_Proxy(from:marshaller, at:$0)"
                return "try marshaller.getOptional(\(parameter)) { \(proxyUnmarshaller) }"
            } else {
                return "try \(typeParser.typeNameRemoveOptional())_Proxy(from:marshaller, at:\(parameter))"
            }
        }

        let innerMarshaller = typeParser.innerTypeParser.map { innerTypeParser in
            " { \(getUnmarshallerFunction(for: innerTypeParser[0], withParameter: "$0")) }"
        } ?? ""
        let unmarshaller = "try marshaller.get" + marshallingName + "(\(parameter))" + innerMarshaller
        if let functionHelperName = typeParser.functionTypeParser?.functionHelperName {
            return "Self.create_\(functionHelperName)_from_bridgeFunction(using: \(unmarshaller))"
        }
        return unmarshaller
    }

    // Creates a function that writes calls bridgeFunction with a ValdiMarshaller using the given parameterTypes and returnTypes
    // bridgeFunction is a ValdiFunction that will be called with a marshaller
    // isLambda is true will generate a lambda function, false will generate a regular function
    public static func writeFunctionMarshaller(functionParser: SwiftFunctionTypeParser, isOptional: Bool) -> String  {
        let optionalMark = isOptional ? "?" : ""
        let optionalPredicate = isOptional ? "guard let functionImpl = functionImpl else { return nil }" : ""

        // Body of the function that places each parameter onto the marshaller stack
        let parameterMarshallers = functionParser.parameterTypes.enumerated().map { index, parameterType in
            let paramName = functionParser.parameterNames[index].name
            return "_ = \(getMarshallerFunction(for: parameterType, withParameter: paramName))"
        }.joined(separator: indent(spaces: 12))

        let returnValue = getUnmarshallerFunction(for: functionParser.returnType, withParameter: "-1")
        let shouldSync = returnValue.isEmpty ? "false" : "true"
        return  """
                    static func create_\(functionParser.functionHelperName)_from_bridgeFunction(using functionImpl: ValdiFunction\(optionalMark)) -> (\(functionParser.methodTypeWithoutNames))\(optionalMark) {
                        \(optionalPredicate)
                        return { \(functionParser.methodTypeWithNames) in
                            let marshaller = ValdiMarshaller()
                            \(parameterMarshallers)
                            if !functionImpl.perform(with: marshaller, sync: \(shouldSync)) {
                                try marshaller.checkError()
                                throw ValdiError.functionCallFailed(\"\(functionParser.functionHelperName)\")
                            }
                            return \(returnValue)
                        }
                    }\n
                """
    }

    // Writes the unmarshalling code for an object that initializes the object with the values from a marshaller
    // Calls getTypedObjectProperty for each property of the object which calls the unmarshaller specific for the type of the property
    public static func writeObjectUnmarshaller(properties: [SwiftProperty], isInterface: Bool) -> CodeWriter {
        let propertyUnmarshallers = properties.enumerated().map { index, property in
            var propertyNameOrFunctionImpl = property.name
            let unmarshallPropertyType = getUnmarshallerFunction(for: property.typeParser, withParameter: "$0")
            if isInterface && property.typeParser.isFunction {
                propertyNameOrFunctionImpl += "_implementation"
            }

            // shortcut for common types
            switch property.typeParser.typeName {
            case "Bool": fallthrough
            case "Int": fallthrough
            case "Long": fallthrough
            case "Double": fallthrough
            case "String": fallthrough
            case "Data": 
                return "self.\(propertyNameOrFunctionImpl) = try marshaller.getTypedObjectProperty\(property.typeParser.typeName)(objectIndex, \(index))"
            case "Bool?": fallthrough
            case "Int?": fallthrough
            case "Long?": fallthrough
            case "Double?": fallthrough
            case "String?": fallthrough
            case "Data?": 
                return "self.\(propertyNameOrFunctionImpl) = try marshaller.getTypedObjectPropertyOptional\(property.typeParser.typeName.dropLast())(objectIndex, \(index))"
            default:
                let enumsAndObjectMarshallers = ["Enum", "OptionalEnum", "Object", "OptionalObject"]
                let functionMarshallers = ["Function", "OptionalFunction"]
                if !property.typeParser.isInterface,
                   let marshallerName = property.typeParser.marshallerName, enumsAndObjectMarshallers.contains(marshallerName) {
                    return "self.\(propertyNameOrFunctionImpl) = try marshaller.getTypedObjectProperty\(marshallerName)(objectIndex, \(index))"
                } else if let marshallerName = property.typeParser.marshallerName,
                          let functionHelperName = property.typeParser.functionTypeParser?.functionHelperName,
                          functionMarshallers.contains(marshallerName) {
                    return "self.\(propertyNameOrFunctionImpl) = Self.create_\(functionHelperName)_from_bridgeFunction(using: try marshaller.getTypedObjectProperty\(marshallerName)(objectIndex, \(index)))"
                } else {
                    return "self.\(propertyNameOrFunctionImpl) = try marshaller.getTypedObjectProperty(objectIndex, \(index)) { \(unmarshallPropertyType) }"
                }
            }
        }.joined(separator: indent(spaces: 8))


        // If we're initializing an interface, we need to unwrap given proxy object to place the ValueTyped object on the marshaller stack
        // and then pop the proxy object from the stack
        let maybeUnwrapProxy = isInterface ? "let objectIndex = try marshaller.unwrapProxy(objectIndex)" : ""
        let maybePop = isInterface ? "marshaller.pop()" : ""

        let unmarshaller = CodeWriter()
        unmarshaller.appendBody(
        """
            public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
                \(maybeUnwrapProxy)
                \(propertyUnmarshallers)
                \(maybePop)
            }\n
        """)
        return unmarshaller
    }

    /// Write marshalling code that pushes an object onto the marshaller stack
    /// Then assigns each property of the object in the marshaller
    /// Parameters:
    ///   - properties: The properties of the object
    ///   - className: The name of the class
    ///   - isInterface: True we are generating an interface. Pushes a proxy object onto the marshaller stack.
    public static func writeObjectMarshaller(properties: [SwiftProperty], className: String, isInterface: Bool) -> CodeWriter {
        let propertyMarshallers = properties.enumerated().map() { index, property in

            switch property.typeParser.typeName {
            case "Bool": fallthrough
            case "Int": fallthrough
            case "Long": fallthrough
            case "Double": fallthrough
            case "String": fallthrough
            case "Data": 
                return "try marshaller.setTypedObjectProperty\(property.typeParser.typeName)(objectIndex, \(index), self.\(property.name))"
            case "Bool?": fallthrough
            case "Int?": fallthrough
            case "Long?": fallthrough
            case "Double?": fallthrough
            case "String?": fallthrough
            case "Data?": 
                return "try marshaller.setTypedObjectPropertyOptional\(property.typeParser.typeName.dropLast())(objectIndex, \(index), self.\(property.name))"
            default:
                let enumsAndObjectMarshallers = ["Enum", "OptionalEnum", "Object", "OptionalObject"]
                let functionMarshallers = ["Function", "OptionalFunction"]
                if let marshallerName = property.typeParser.marshallerName, enumsAndObjectMarshallers.contains(marshallerName) {
                    return "try marshaller.setTypedObjectProperty\(marshallerName)(objectIndex, \(index), self.\(property.name))"
                } else if let marshallerName = property.typeParser.marshallerName,
                          let functionHelperName = property.typeParser.functionTypeParser?.functionHelperName,
                          functionMarshallers.contains(marshallerName) {
                    let parameter = "Self.create_\(functionHelperName)_bridgeFunction(using: self.\(property.name))"
                    return "try marshaller.setTypedObjectProperty\(marshallerName)(objectIndex, \(index), \(parameter))"
                }
                else {
                    let marshallerFunction = getMarshallerFunction(for: property.typeParser, withParameter: "self.\(property.name)")
                    return "try marshaller.setTypedObjectProperty(objectIndex, \(index)) { _ = \(marshallerFunction) }"
                }
            }
        }.joined(separator: indent(spaces: 8))

        // Push a proxy object onto the marshaller stack if we're generating an interface. The return value
        let maybePushProxyStart = isInterface ? "let objectIndex = try marshaller.pushProxy(object: self) {" : ""
        let maybePushProxyEnd = isInterface ? "}" : ""

        let marshaller = CodeWriter()
        marshaller.appendBody(
        """
            public func push(to marshaller: ValdiMarshaller) throws -> Int {
                \(maybePushProxyStart)
                register_\(className)()
                let objectIndex = try marshaller.pushObject(className: "\(className)")
                \(propertyMarshallers)
                \(maybePushProxyEnd)
                return objectIndex
            }\n
        """)
        return marshaller
    }

    // Write the constructor that initializes the object with all the non-optional properties
    /// Parameters:
    ///   - properties: The properties of the object
    public static func writeConstructor(properties: [SwiftProperty]) -> CodeWriter {
        var propertyAssignments = [String]()
        var callParameters = [String]()

        // Iterate over all properties non-optional properties and build the function signature and body
        for property in properties {
            if !property.typeParser.isOptional {
                let escaping = property.typeParser.needsEscaping ? "@escaping " : ""
                callParameters.append("\(property.name):\(escaping)\(property.typeParser.typeName)")
                propertyAssignments.append("self.\(property.name) = \(property.name)")
            }
        }

        let callParameterString = callParameters.joined(separator: ", ")
        let constructorBody = propertyAssignments.joined(separator: indent(spaces: 8))
        let constructor = CodeWriter()
        constructor.appendBody(
        """
            public init(\(callParameterString)) {
                \(constructorBody)
            }\n
        """)
        return constructor
    }

    /// Writes out a function that returns a ValdiMarshallableObjectDescriptor for the object. This contains the schema of the object.
    /// Parameters:
    ///   - properties: The properties of the object
    ///   - typeParameters: The type parameters of the object
    ///   - objectDescriptorType: The type of the object descriptor
    public static func writeObjectDescriptorGetter(properties: [SwiftProperty],
                                                   propertyTypes: [ValdiModelPropertyType],
                                                   typeParameters: [ValdiTypeParameter]?,
                                                   objectDescriptorType: String) throws -> CodeWriter {
        let emittedIdentifiers = SwiftEmittedIdentifiers()
        // Write a ValdiMarshallableObjectFieldDescriptor for each property of the object. This contains its name and schema.
        let fieldDescriptors = try properties.enumerated().map { index, property throws in
            let schemaWriter = SwiftSchemaWriter(typeParameters: typeParameters, emittedIdentifiers: emittedIdentifiers)
            try schemaWriter.appendType(propertyTypes[index], asBoxed: false, isMethod: false)
            return "ValdiMarshallableObjectFieldDescriptor(name: \"\(property.name)\", type: \"\(schemaWriter.str)\"),"
        }.joined(separator: indent(spaces: 12))

        // Create identifiers list for other referenced objects
        let identifiersParam = !emittedIdentifiers.isEmpty ? "identifiers" : "nil"
        let identifiersDefinition = !emittedIdentifiers.isEmpty ?
            "let \(identifiersParam): [ValdiMarshallableIdentifier] =\n\(emittedIdentifiers.initializationString.content)" :
            "// No identifiers needed for this object"

        let descGetter = CodeWriter()
        descGetter.appendBody(
            """
                static func getDescriptor() -> ValdiMarshallableObjectDescriptor {
                    let fields: [ValdiMarshallableObjectFieldDescriptor] =
                    [
                        \(fieldDescriptors)
                    ]
                    \(identifiersDefinition)
                    return ValdiMarshallableObjectDescriptor(fields, identifiers: \(identifiersParam), type: \(objectDescriptorType))
                }\n
            """)
        return descGetter
    }

    public static func writeProxyFunctionImplementation(propertyName: String, functionParser: SwiftFunctionTypeParser, isOptional: Bool) -> String {
        let functionImpl = "\(propertyName)_implementation"
        let optionalMark = isOptional ? "?" : ""
        let optionalPredicate =  isOptional ? "guard let functionImpl = self.\(functionImpl) else { throw ValdiError.functionCallFailed(\"\(propertyName)\") }" : ""
        let callStatement = isOptional ?
            "return try functionImpl(\(functionParser.callableParametersString))" :
            "return try self.\(functionImpl)(\(functionParser.callableParametersString))"
        return
            """
                public func \(propertyName) \(functionParser.methodTypeWithNames) {
                    \(optionalPredicate)
                    \(callStatement)
                }
                private var \(functionImpl): (\(functionParser.methodTypeWithoutNames))\(optionalMark)\n
            """
    }

    /// Writes the variable declarations for the object
    /// Function properties in Proxy objects (interfaces) are implemented as a call to a bridge function
    /// Parameters:
    ///   - properties: The properties of the object
    ///   - isInterface: True we are generating an interface. Affects how function properties are generated.
    public static func writeVariableDeclaration(properties: [SwiftProperty], isInterface: Bool) -> CodeWriter {
        let variableDeclarations = CodeWriter()
        for property in properties {
            if let functionParser = property.typeParser.functionTypeParser, isInterface {
                let functionImplementation = writeProxyFunctionImplementation(propertyName: property.name, functionParser: functionParser, isOptional: property.typeParser.isOptional)
                variableDeclarations.appendBody(functionImplementation)
                continue
            }

            if !isInterface {
                if let comments = property.comments {
                    variableDeclarations.appendBody(FileHeaderCommentGenerator.generateMultilineComment(comment: comments))
                    variableDeclarations.appendBody("\n")
                }
            }
            variableDeclarations.appendBody("    public var \(property.name): \(property.typeParser.typeName)\n")
        }
        return variableDeclarations
    }

    // Writes the function that unmarshalls the parameters of a function from a marshaller and calls the function
    // The return value is then pushed onto the marshaller stack
    public static func writeFunctionUnmarshaller(functionParser: SwiftFunctionTypeParser, isOptional: Bool) -> CodeWriter {
        let escaping = isOptional ? "" : "@escaping "
        let optionalMark = isOptional ? "?" : ""
        let optionalPredicate = isOptional ? "guard let functionImpl = functionImpl else { return nil }" : ""

        // Unmarshall each parameter and build the call arguments for the function
        let parameterUnmarshallers = functionParser.parameterTypes.enumerated().map { index, parameterType in
            let paramName = functionParser.parameterNames[index].name
            return "let \(paramName) : \(parameterType.typeName) = \(getUnmarshallerFunction(for:parameterType, withParameter: String(index)))"
        }.joined(separator: indent(spaces: 16))

        let returnMarshaller = getMarshallerFunction(for: functionParser.returnType, withParameter: "result")
        let callStatement = "\(returnMarshaller.isEmpty ? "" : "let result = ")try functionImpl(\(functionParser.callableParametersString))"
        let marshallReturnValue = returnMarshaller.isEmpty ? "// No return value for this call" : "_ = \(returnMarshaller)"

        let functionHelpers = CodeWriter()
        functionHelpers.appendBody(
        """
            static func create_\(functionParser.functionHelperName)_bridgeFunction(using functionImpl: \(escaping)(\(functionParser.methodTypeWithoutNames))\(optionalMark)) -> ((ValdiMarshaller) -> Bool)\(optionalMark) {
                \(optionalPredicate)
                return createBridgeFunctionWrapper({ marshaller in
                    \(parameterUnmarshallers)
                    \(callStatement)
                    \(marshallReturnValue)
                    })
            }\n
        """)
        return functionHelpers
    }

    public static func writeFunctionHelpers(sourceGenerator: SwiftSourceFileGenerator) throws -> CodeWriter {
        let funcionHelpers = CodeWriter()
        for typeParser in sourceGenerator.functionParsers {
            if let functionTypeParser = typeParser.functionTypeParser {
                funcionHelpers.appendBody(writeFunctionUnmarshaller(functionParser: functionTypeParser, isOptional: typeParser.isOptional))
                funcionHelpers.appendBody(writeFunctionMarshaller(functionParser: functionTypeParser, isOptional: typeParser.isOptional))
            }
        }
        return funcionHelpers
    }
}
