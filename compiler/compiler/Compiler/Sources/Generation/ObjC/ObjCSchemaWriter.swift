//
//  ObjCSchemaWriter.swift
//  
//
//  Created by Simon Corsin on 1/26/23.
//

import Foundation

class ObjCSchemaWriterListener: SchemaWriterListener {

    private let emittedIdentifiers: ObjCEmittedIdentifiers

    init(emittedIdentifiers: ObjCEmittedIdentifiers) {
        self.emittedIdentifiers = emittedIdentifiers
    }

    func getClassName(nodeMapping: ValdiNodeClassMapping, typeArguments: [ValdiModelPropertyType]?) throws -> String? {
        guard let iosType = nodeMapping.iosType else {
            throw CompilerError("No iOS type in given mapping")
        }

        let identifierIndex = emittedIdentifiers.getIdentifierIndex(iosType.name)

        return "[\(identifierIndex)]"
    }
}

class ObjCSchemaWriter: SchemaWriter {
    init(typeParameters: [ValdiTypeParameter]?, emittedIdentifiers: ObjCEmittedIdentifiers) {
        super.init(typeParameters: typeParameters,
                   listener: ObjCSchemaWriterListener(emittedIdentifiers: emittedIdentifiers),
                   alwaysBoxFunctionParametersAndReturnValue: false,
                   boxIntEnums: true)
    }
}
