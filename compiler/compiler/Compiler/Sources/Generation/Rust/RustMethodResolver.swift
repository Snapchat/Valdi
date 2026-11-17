// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct RustMethodResolver {
    private init() {}

    static func resolve(
        properties: [ValdiModelProperty],
        bundleInfo: CompilationItem.BundleInfo,
        declaration: CPPType
    ) throws -> [RustResolvedMethod] {
        let propertyNames = RustIdentifierScope()
        let resolver = RustTypeResolver(
            bundleInfo: bundleInfo,
            moduleIncludeDir: declaration.includeDir,
            ownerName: declaration.declaration.name)
        return try properties.map { property in
            let resolvedProperty = RustResolvedProperty(
                property: property,
                rustName: try propertyNames.allocate(
                    property.name, resolved: property.name.snakeCased.rustIdentifier))
            let parameters: [ValdiModelProperty]
            let returnType: ValdiModelPropertyType
            switch property.type {
            case .function(let values, let value, _, _, _):
                parameters = values
                returnType = value
            default:
                parameters = []
                returnType = property.type
            }

            let parameterNames = RustIdentifierScope()
            let resolvedParameters = try parameters.map { parameter in
                (
                    try parameterNames.allocate(
                        parameter.name, resolved: parameter.name.snakeCased.rustIdentifier),
                    try resolver.resolve(parameter.type, namePath: [property.name, parameter.name])
                )
            }
            return RustResolvedMethod(
                property: resolvedProperty,
                parameters: resolvedParameters,
                returnType: try resolver.resolve(returnType, namePath: [property.name, "Return"]))
        }
    }
}
