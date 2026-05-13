//
//  File.swift
//  
//
//  Created by Simon Corsin on 4/12/23.
//

import Foundation

class CppSchemaWriterListener: SchemaWriterListener {
    private let generator: CppCodeGenerator?
    private let referencedTypes: [String]
    private let indexByTypeName: [String: Int]

    init(generator: CppCodeGenerator?) {
        self.generator = generator
        self.referencedTypes = generator?.referenceTypeKeys ?? []

        self.indexByTypeName = referencedTypes.enumerated().associate {
            ($0.element, $0.offset)
        }
    }

    func getClassName(nodeMapping: ValdiNodeClassMapping, typeArguments: [ValdiModelPropertyType]?) throws -> String? {
        if !nodeMapping.isGenerated {
            return nil
        }

        let fullTypeName: String
        if let typeArguments {
            fullTypeName = CppCodeGenerator.makeCanonicalTypeKey(type: .genericObject(nodeMapping, typeArguments: typeArguments))
        } else {
            fullTypeName = CppCodeGenerator.makeCanonicalTypeKey(type: .object(nodeMapping))
        }

        guard let index = self.indexByTypeName[fullTypeName] else {
            throw CompilerError("Could not resolve dependent declaration for type \(fullTypeName)")
        }

        return "[\(index)]"
    }
}

class CppSchemaWriter: SchemaWriter {
    private let cppWriterListener: CppSchemaWriterListener

    init(typeParameters: [ValdiTypeParameter]?, generator: CppCodeGenerator?) {
        self.cppWriterListener = CppSchemaWriterListener(generator: generator)
        super.init(typeParameters: typeParameters,
                   listener: cppWriterListener,
                   alwaysBoxFunctionParametersAndReturnValue: false,
                   boxIntEnums: false)
    }
}
