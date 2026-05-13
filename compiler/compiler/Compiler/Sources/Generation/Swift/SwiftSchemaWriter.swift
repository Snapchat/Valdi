//
//  SwiftSchemaWriter.swift
//
//
//  Created by Ben Dodson on 2/2/24.
//

import Foundation

class SwiftSchemaWriterListener: SchemaWriterListener {

    private let emittedIdentifiers: SwiftEmittedIdentifiers

    init(emittedIdentifiers: SwiftEmittedIdentifiers) {
        self.emittedIdentifiers = emittedIdentifiers
    }

    func getClassName(nodeMapping: ValdiNodeClassMapping, typeArguments: [ValdiModelPropertyType]?) throws -> String? {
        guard let iosType = nodeMapping.iosType else {
            throw CompilerError("No iOS type in given mapping")
        }

        let identifierIndex = emittedIdentifiers.getIdentifierIndex(iosType.swiftName)
        if (nodeMapping.iosType?.iosLanguage == .swift || nodeMapping.iosType?.iosLanguage == .both) {
            emittedIdentifiers.setRegisterFunction(forIdentifier: iosType.swiftName, registerFunction: "register_\(iosType.swiftName)")
        } else if nodeMapping.iosType?.iosLanguage == .objc {

            if nodeMapping.marshallAsUntyped {
                emittedIdentifiers.setRegisterFunction(forIdentifier: iosType.swiftName, registerFunction: "{ ValdiMarshallableObjectRegistry.shared.registerUntypedObjCClass(className: \"\(iosType.swiftName)\") }")
            } else {
                emittedIdentifiers.setRegisterFunction(forIdentifier: iosType.swiftName, registerFunction: "{ ValdiMarshallableObjectRegistry.shared.registerObjCClass(className: \"\(iosType.swiftName)\") }")
            }
        } else {
            let language = nodeMapping.iosType?.iosLanguage.rawValue ?? "Unknown"
            throw CompilerError("Unsupported language for type \(iosType.swiftName): \(language)")
        }
        return "[\(identifierIndex)]"
    }
}

class SwiftSchemaWriter: SchemaWriter {
    init(typeParameters: [ValdiTypeParameter]?, emittedIdentifiers: SwiftEmittedIdentifiers) {
        super.init(typeParameters: typeParameters,
                   listener: SwiftSchemaWriterListener(emittedIdentifiers: emittedIdentifiers),
                   alwaysBoxFunctionParametersAndReturnValue: false,
                   boxIntEnums: true)
    }
}
