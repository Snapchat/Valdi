//
//  ObjCDependencyRepresentationGenerator.swift
//
//
//  Created by John Corbett on 5/16/23.
//

import Foundation

final class ObjCDependencyRepresentationGenerator {
    private struct ContextFactoryProperty: Encodable {
        let qualifiedType: String
        let name: String
        let propertyAttribute: String
        let injectable: Bool
    }

    private struct DependencyDataOutput: Encodable {
        let single_file_codegen: Bool
        let contexts: [String: [ContextFactoryProperty]]
    }

    private let items: [IntermediateDependencyMetadata]
    private let singleFileCodegen: Bool

    init(items: [IntermediateDependencyMetadata], singleFileCodegen: Bool) {
        self.items = items
        self.singleFileCodegen = singleFileCodegen
    }

    func generate() throws -> File {
        let contexts = try items.reduce(into: [String: [ContextFactoryProperty]]()) { initialItems, dependencyMetadata in
            let model = dependencyMetadata.model

            guard let iosType = model.iosType else {
                return
            }

            let className = iosType.name
            let objcGenerator = ObjCClassGenerator(className: className)

            let nameAllocator = PropertyNameAllocator.forObjC()

            let properties = try model.properties.map { property in
                let typeParser = try objcGenerator.writeTypeParser(type: property.type.unwrappingOptional,
                                                                   isOptional: property.type.isOptional,
                                                                   namePaths: [property.name],
                                                                   allowValueType: true,
                                                                   isInterface: model.exportAsInterface,
                                                                   nameAllocator: nameAllocator.scoped())

                return ContextFactoryProperty(qualifiedType: typeParser.typeName,
                                              name: property.name,
                                              propertyAttribute: typeParser.propertyAttribute,
                                              injectable: property.injectableParams.compatibility.contains(.ios))
            }

            initialItems[className] = properties
        }

        let output = DependencyDataOutput(single_file_codegen: singleFileCodegen, contexts: contexts)

        let encoder = JSONEncoder()
        encoder.outputFormatting = .sortedKeys

        let data: Data
        do {
            data = try encoder.encode(output)
        } catch {
            throw CompilerError("Failed to encode dependency metadata")
        }

        return File.data(data)
    }
}
