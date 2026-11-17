// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

final class RustValueModelGenerator {
    private let model: ValdiModel
    private let cppType: CPPType
    private let bundleInfo: CompilationItem.BundleInfo
    private let sourceFileName: GeneratedSourceFilename

    init(
        model: ValdiModel,
        cppType: CPPType,
        bundleInfo: CompilationItem.BundleInfo,
        sourceFileName: GeneratedSourceFilename
    ) {
        self.model = model
        self.cppType = cppType
        self.bundleInfo = bundleInfo
        self.sourceFileName = sourceFileName
    }

    private struct Property {
        let source: ValdiModelProperty
        let name: String
        let type: RustType
    }

    private func resolveProperties() throws -> [Property] {
        let names = RustIdentifierScope()
        let resolver = RustTypeResolver(
            bundleInfo: bundleInfo,
            moduleIncludeDir: cppType.includeDir,
            ownerName: cppType.declaration.name)
        return try model.properties.map { property in
            Property(
                source: property,
                name: try names.allocate(property.name, resolved: property.name.snakeCased.rustIdentifier),
                type: try resolver.resolve(property.type, namePath: [property.name]))
        }
    }

    private func schemaRegistration() throws -> String {
        let writer = RustSchemaWriter(typeParameters: model.typeParameters)
        try writer.appendClass(
            cppType.declaration.fullTypeName,
            properties: model.properties,
            asyncStrictMode: bundleInfo.asyncStrictMode)
        if model.typeParameters?.isEmpty == false {
            return RustSchemaGenerator.genericRegistration(
                bundleInfo: bundleInfo,
                cppType: cppType,
                schema: writer.str,
                references: RustSchemaGenerator.references(in: model.properties))
        }
        return RustSchemaGenerator.classRegistration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: writer.str,
            references: RustSchemaGenerator.references(in: model.properties))
    }

    private func deserializeFields(_ properties: [Property]) -> String {
        return properties.map { property in
            let value = "value.property(\"\(property.source.name.escapedForRustStringLiteral)\")?"
            let expression = property.type.deserializeExpression(
                from: value,
                borrowed: false,
                coreModule: "valdi_core_rust")
            return "            \(property.name): \(expression)?,"
        }.joined(separator: "\n")
    }

    private func borrowedPropertyType(_ type: RustType, lifetime: String) -> String {
        switch type {
        case .string:
            return "std::borrow::Cow<'\(lifetime), str>"
        case .nullable(.string):
            return "Option<std::borrow::Cow<'\(lifetime), str>>"
        default:
            return type.rustBorrowedType.replacingOccurrences(of: "'_", with: "'\(lifetime)")
        }
    }

    private func borrowedAccessors(_ properties: [Property], lifetime: String) -> String {
        properties.map { property in
            let value = "self.value.property(\"\(property.source.name.escapedForRustStringLiteral)\")?"
            let expression = property.type.deserializeExpression(
                from: value,
                borrowed: true,
                coreModule: "valdi_core_rust")
            return """
                    pub fn \(property.name)(&self) -> Result<\(borrowedPropertyType(property.type, lifetime: lifetime)), String> {
                        \(expression)
                    }
                """
        }.joined(separator: "\n")
    }

    private func pushFields(_ properties: [Property]) -> String {
        properties.map { property in
            return """
                        valdi_core_rust::ValdiPush::push_to(&self.\(property.name), marshaller)?;
                        marshaller.put_map_property(object, "\(property.source.name.escapedForRustStringLiteral)")?;
                """
        }.joined(separator: "\n")
    }

    private func serializeBody(_ properties: [Property]) -> String {
        let locals = properties.map { property in
            let expression = property.type.serializeExpression(
                from: "&self.\(property.name)",
                coreModule: "valdi_core_rust")
            return "        let \(property.name) = \(expression)?;"
        }.joined(separator: "\n")
        let entries = properties.map { property in
            "            (\"\(property.source.name.escapedForRustStringLiteral)\", \(property.name).as_borrowed()),"
        }.joined(separator: "\n")
        return """
            \(locals)
                    valdi_core_rust::Value::object(&[
            \(entries)
                    ])
            """
    }

    private func writeGeneric(properties: [Property], name: String) throws -> String {
        guard let parameters = model.typeParameters else {
            throw CompilerError("Missing generic parameters for Rust model '\(model.tsType)'")
        }
        let genericParameters = parameters.map { $0.name.rustTypeIdentifier }
        let generics = genericParameters.joined(separator: ", ")
        let fields = properties.map { "    pub \($0.name): \($0.type.rustOwnedType)," }.joined(
            separator: "\n")
        let initializerParameters = properties.map { "\($0.name): \($0.type.rustOwnedType)" }.joined(
            separator: ", ")
        let initializerFields = properties.map(\.name).joined(separator: ", ")
        let deserializeBounds = genericParameters.map { "\($0): valdi_core_rust::ValdiDeserialize<'a>" }
            .joined(separator: ", ")
        let serializeBounds = genericParameters.map { "\($0): valdi_core_rust::ValdiSerialize" }.joined(
            separator: ", ")
        let pushBounds = genericParameters.map { "\($0): valdi_core_rust::ValdiPush" }.joined(
            separator: ", ")
        let borrowedBounds = genericParameters.map {
            "\($0): valdi_core_rust::ValdiDeserialize<'a> + valdi_core_rust::ValdiToOwned"
        }.joined(separator: ", ")
        let ownedGenerics = genericParameters.map { "\($0)::Owned" }.joined(separator: ", ")
        let ownedFields = properties.map {
            "            \($0.name): valdi_core_rust::ValdiToOwned::to_owned_value(&self.\($0.name)()?)?,"
        }.joined(separator: "\n")

        return """
            pub struct \(name)<\(generics)> {
            \(fields)
            }

            #[derive(Clone, Copy)]
            pub struct \(name)Ref<'a, \(generics)> {
                value: valdi_core_rust::ValueRef<'a>,
                marker: std::marker::PhantomData<(\(generics),)>,
            }

            impl<\(generics)> \(name)<\(generics)> {
                pub fn new(\(initializerParameters)) -> Self {
                    Self { \(initializerFields) }
                }
            }

            impl<'a, \(borrowedBounds)> \(name)Ref<'a, \(generics)> {
            \(borrowedAccessors(properties, lifetime: "a"))

                pub fn to_owned(&self) -> Result<\(name)<\(ownedGenerics)>, String> {
                    Ok(\(name) {
            \(ownedFields)
                    })
                }
            }

            impl<'a, \(deserializeBounds)> valdi_core_rust::ValdiDeserialize<'a> for \(name)<\(generics)> {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self {
            \(deserializeFields(properties))
                    })
                }
            }

            impl<'a, \(generics)> valdi_core_rust::ValdiDeserialize<'a> for \(name)Ref<'a, \(generics)> {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self { value, marker: std::marker::PhantomData })
                }
            }

            impl<\(serializeBounds)> valdi_core_rust::ValdiSerialize for \(name)<\(generics)> {
                fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> {
            \(serializeBody(properties))
                }
            }

            impl<\(pushBounds)> valdi_core_rust::ValdiPush for \(name)<\(generics)> {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    let object = marshaller.push_map(\(properties.count));
            \(pushFields(properties))
                    Ok(())
                }
            }

            impl<\(generics)> valdi_core_rust::ValdiPush for \(name)Ref<'_, \(generics)> {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    marshaller.push_value(self.value);
                    Ok(())
                }
            }

            impl<'a, \(borrowedBounds)> valdi_core_rust::ValdiToOwned for \(name)Ref<'a, \(generics)> {
                type Owned = \(name)<\(ownedGenerics)>;

                fn to_owned_value(&self) -> Result<Self::Owned, String> {
                    self.to_owned()
                }
            }

            """
    }

    private func writeConcrete(properties: [Property], name: String) -> String {
        let ownedFields = properties.map { "    pub \($0.name): \($0.type.rustOwnedType)," }.joined(
            separator: "\n")
        let initializerParameters = properties.map { "\($0.name): \($0.type.rustOwnedType)" }.joined(
            separator: ", ")
        let initializerFields = properties.map(\.name).joined(separator: ", ")

        return """
            pub struct \(name) {
            \(ownedFields)
            }

            #[derive(Clone, Copy)]
            pub struct \(name)Ref<'a> {
                value: valdi_core_rust::ValueRef<'a>,
            }

            impl \(name) {
                pub fn new(\(initializerParameters)) -> Self {
                    Self { \(initializerFields) }
                }
            }

            impl<'a> \(name)Ref<'a> {
            \(borrowedAccessors(properties, lifetime: "a"))

                pub fn to_owned(&self) -> Result<\(name), String> {
                    valdi_core_rust::ValdiDeserialize::from_valdi(self.value)
                }
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(name) {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self {
            \(deserializeFields(properties))
                    })
                }
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(name)Ref<'a> {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self { value })
                }
            }

            impl valdi_core_rust::ValdiSerialize for \(name) {
                fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> {
            \(serializeBody(properties))
                }
            }

            impl valdi_core_rust::ValdiPush for \(name) {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    let object = marshaller.push_map(\(properties.count));
            \(pushFields(properties))
                    Ok(())
                }
            }

            impl valdi_core_rust::ValdiPush for \(name)Ref<'_> {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    marshaller.push_value(self.value);
                    Ok(())
                }
            }

            impl valdi_core_rust::ValdiToOwned for \(name)Ref<'_> {
                type Owned = \(name);

                fn to_owned_value(&self) -> Result<Self::Owned, String> {
                    self.to_owned()
                }
            }

            """
    }

    func write() throws -> [NativeSource] {
        precondition(!model.exportAsInterface)
        let properties = try resolveProperties()
        let name = cppType.declaration.name.rustTypeIdentifier
        let modelRust =
            if model.typeParameters?.isEmpty == false {
                try writeGeneric(properties: properties, name: name)
            } else {
                writeConcrete(properties: properties, name: name)
            }
        let rust = modelRust + (try schemaRegistration())
        return RustNativeSourceGenerator.generate(
            bundleInfo: bundleInfo,
            cppType: cppType,
            sourceFileName: sourceFileName,
            files: RustGeneratedFiles(rust: rust, cHeader: "", registrationSymbols: []))
    }
}
