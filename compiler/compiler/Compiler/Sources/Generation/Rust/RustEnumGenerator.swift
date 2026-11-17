// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

final class RustEnumGenerator {
    private let exportedEnum: ExportedEnum
    private let cppType: CPPType
    private let bundleInfo: CompilationItem.BundleInfo
    private let sourceFileName: GeneratedSourceFilename

    init(
        exportedEnum: ExportedEnum, cppType: CPPType, bundleInfo: CompilationItem.BundleInfo,
        sourceFileName: GeneratedSourceFilename
    ) {
        self.exportedEnum = exportedEnum
        self.cppType = cppType
        self.bundleInfo = bundleInfo
        self.sourceFileName = sourceFileName
    }

    func write() throws -> [NativeSource] {
        let name = cppType.declaration.name.rustTypeIdentifier
        let names = RustIdentifierScope()
        let schemaWriter = RustSchemaWriter(typeParameters: nil)
        let rust: String
        switch exportedEnum.cases {
        case .enum(let values):
            try schemaWriter.appendIntEnum(cppType.declaration.fullTypeName, enumCases: values)
            let cases = try values.map { value in
                (try names.allocate(value.name, resolved: value.name.rustTypeIdentifier), value.value)
            }
            let declarations = cases.map { "    \($0.0) = \($0.1)," }.joined(separator: "\n")
            let conversions = cases.map { "            \($0.1) => Ok(Self::\($0.0))," }.joined(
                separator: "\n")
            rust = """
                #[repr(i32)]
                #[derive(Clone, Copy, Debug, Eq, PartialEq)]
                pub enum \(name) {
                \(declarations)
                }

                impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(name) {
                    fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                        match value.as_number()? as i32 {
                \(conversions)
                            value => Err(format!("invalid \(name) value: {value}")),
                        }
                    }
                }

                impl valdi_core_rust::ValdiSerialize for \(name) {
                    fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> {
                        valdi_core_rust::Value::from_number(*self as i32 as f64)
                    }
                }

                impl valdi_core_rust::ValdiPush for \(name) {
                    fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                        marshaller.push_number(*self as i32 as f64);
                        Ok(())
                    }
                }

                impl valdi_core_rust::ValdiToOwned for \(name) {
                    type Owned = Self;

                    fn to_owned_value(&self) -> Result<Self::Owned, String> { Ok(*self) }
                }

                """
        case .stringEnum(let values):
            try schemaWriter.appendStringEnum(cppType.declaration.fullTypeName, enumCases: values)
            let cases = try values.map { value in
                (try names.allocate(value.name, resolved: value.name.rustTypeIdentifier), value.value)
            }
            let declarations = cases.map { "    \($0.0)," }.joined(separator: "\n")
            let deserializeCases = cases.map {
                "            \"\($0.1.escapedForRustStringLiteral)\" => Ok(Self::\($0.0)),"
            }.joined(separator: "\n")
            let serializeCases = cases.map {
                "            Self::\($0.0) => \"\($0.1.escapedForRustStringLiteral)\","
            }.joined(separator: "\n")
            rust = """
                #[derive(Clone, Copy, Debug, Eq, PartialEq)]
                pub enum \(name) {
                \(declarations)
                }

                impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(name) {
                    fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                        match value.as_str()?.as_ref() {
                \(deserializeCases)
                            value => Err(format!("invalid \(name) value: {value}")),
                        }
                    }
                }

                impl valdi_core_rust::ValdiSerialize for \(name) {
                    fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> {
                        let value = match self {
                \(serializeCases)
                        };
                        valdi_core_rust::Value::from_string(value)
                    }
                }

                impl valdi_core_rust::ValdiPush for \(name) {
                    fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                        let value = match self {
                \(serializeCases)
                        };
                        marshaller.push_string(value);
                        Ok(())
                    }
                }

                impl valdi_core_rust::ValdiToOwned for \(name) {
                    type Owned = Self;

                    fn to_owned_value(&self) -> Result<Self::Owned, String> { Ok(*self) }
                }

                """
        }

        let schema = RustSchemaGenerator.enumRegistration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: schemaWriter.str)
        return RustNativeSourceGenerator.generate(
            bundleInfo: bundleInfo,
            cppType: cppType,
            sourceFileName: sourceFileName,
            files: RustGeneratedFiles(rust: rust + schema, cHeader: "", registrationSymbols: []))
    }
}
