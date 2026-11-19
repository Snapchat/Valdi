// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

final class RustProxyGenerator {
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

    func write() throws -> [NativeSource] {
        precondition(model.exportAsInterface)
        precondition(model.typeParameters?.isEmpty != false)

        let methods = try RustMethodResolver.resolve(
            properties: model.properties,
            bundleInfo: bundleInfo,
            declaration: cppType)
        let rustName = cppType.declaration.name.rustTypeIdentifier
        let traitName = "\(rustName)Proxy"

        let traitMethods = methods.map { method in
            let parameters = method.parameters.map { "\($0.name): \($0.type.rustBorrowedType)" }.joined(
                separator: ", ")
            return
                "    fn \(method.property.rustName)(&self\(parameters.isEmpty ? "" : ", \(parameters)")) -> Result<\(method.returnType.rustOwnedType), String>;"
        }.joined(separator: "\n")

        let ownedMethods = methods.map { method in
            let parameters = method.parameters.map { "\($0.name): \($0.type.rustBorrowedType)" }.joined(
                separator: ", ")
            let arguments = method.parameters.map(\.name).joined(separator: ", ")
            return """
                    pub fn \(method.property.rustName)(&self\(parameters.isEmpty ? "" : ", \(parameters)")) -> Result<\(method.returnType.rustOwnedType), String> {
                        self.as_borrowed().\(method.property.rustName)(\(arguments))
                    }
                """
        }.joined(separator: "\n")

        let borrowedMethods = methods.map { method in
            let parameters = method.parameters.map { "\($0.name): \($0.type.rustBorrowedType)" }.joined(
                separator: ", ")
            let callMarshaller = method.parameters.isEmpty ? "_marshaller" : "marshaller"
            let values = method.parameters.map { parameter in
                "                \(parameter.type.pushStatement(value: parameter.name, to: callMarshaller, asReturnValue: false, coreModule: "valdi_core_rust"))"
            }.joined(separator: "\n")
            let resultReader =
                method.returnType.rustOwnedType == "()"
                ? "|_| Ok(())"
                : "|value| \(method.returnType.deserializeExpression(from: "value", borrowed: false, coreModule: "valdi_core_rust"))"
            return """
                    pub fn \(method.property.rustName)(&self\(parameters.isEmpty ? "" : ", \(parameters)")) -> Result<\(method.returnType.rustOwnedType), String> {
                        let function_value = self.0.property("\(method.property.property.name.escapedForRustStringLiteral)")?;
                        let function = <valdi_core_rust::FunctionRef<'_> as valdi_core_rust::ValdiDeserialize>::from_valdi(function_value)?;
                        function.call_with(
                            |\(callMarshaller)| {
                \(values)
                                Ok(())
                            },
                            \(resultReader),
                        )
                    }
                """
        }.joined(separator: "\n")

        let invokeCases = methods.enumerated().map { index, method in
            let arguments = method.parameters.enumerated().map { argumentIndex, parameter in
                let value = "arguments.get(\(argumentIndex))?"
                return parameter.type.deserializeBinding(
                    named: parameter.name,
                    from: value,
                    borrowed: true,
                    coreModule: "valdi_core_rust"
                )
            }.joined(separator: "\n")
            let callArguments = method.parameters.map(\.name).joined(separator: ", ")
            let call = "proxy.\(method.property.rustName)(\(callArguments))?"
            let pushResult = method.returnType.pushStatement(
                value: "result",
                to: "result_marshaller",
                asReturnValue: true,
                coreModule: "valdi_core_rust")
            var resultStatements = [String]()
            if !method.parameters.isEmpty {
                resultStatements.append("let arguments = unsafe { call.arguments() };")
                resultStatements.append(arguments)
            }
            resultStatements.append(call)
            let resultBody = RustCodeFormatting.indentLines(
                resultStatements.joined(separator: "\n"), spaces: 8)
            let invokeCase = """
                \(index) => {
                    let result = {
                \(resultBody)
                    };
                    let mut result_storage = unsafe { call.result() };
                    let result_marshaller = &mut result_storage;
                \(RustCodeFormatting.indentLines(pushResult, spaces: 4))
                }
                """
            return RustCodeFormatting.indentLines(invokeCase, spaces: 20)
        }.joined(separator: ",\n")

        let rust = """
            pub trait \(traitName) {
            \(traitMethods)
            }

            pub struct \(rustName)(valdi_core_rust::Value);
            #[derive(Clone, Copy)]
            pub struct \(rustName)Ref<'a>(valdi_core_rust::ValueRef<'a>);

            impl \(rustName) {
                pub fn new<T: \(traitName) + 'static>(proxy: T) -> Result<Self, String> {
                    unsafe extern "C" fn invoke<T: \(traitName) + 'static>(
                        instance: *mut std::ffi::c_void,
                        method: usize,
                        mut call: valdi_core_rust::ValdiCall,
                        ) -> valdi_core_rust::ValdiStatus {
                        valdi_core_rust::ffi_boundary(|| {
                            let proxy = unsafe { &*(instance as *const T) };
                            match method {
            \(invokeCases),
                                _ => return Err(format!("invalid Rust proxy method index: {method}")),
                            };
                            Ok(())
                        })
                    }

                    unsafe extern "C" fn drop_instance<T>(instance: *mut std::ffi::c_void) {
                        let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                            unsafe { drop(Box::from_raw(instance as *mut T)) };
                        }));
                    }

                    let instance = Box::into_raw(Box::new(proxy)).cast();
                    let value = valdi_core_rust::new_proxy(
                        \(RustSchemaGenerator.symbol(bundleName: bundleInfo.name, cppType: cppType))(),
                        \(methods.count),
                        instance,
                        invoke::<T>,
                        drop_instance::<T>,
                    )?;
                    Ok(Self(value))
                }

                pub fn as_borrowed(&self) -> \(rustName)Ref<'_> {
                    \(rustName)Ref(self.0.as_borrowed())
                }

            \(ownedMethods)
            }

            impl Clone for \(rustName) {
                fn clone(&self) -> Self { Self(self.0.clone()) }
            }

            impl \(rustName)Ref<'_> {
                pub fn to_owned(&self) -> Result<\(rustName), String> {
                    Ok(\(rustName)(self.0.to_owned()?))
                }

            \(borrowedMethods)
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(rustName) {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self(value.to_owned()?))
                }
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(rustName)Ref<'a> {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self(value))
                }
            }

            impl valdi_core_rust::ValdiSerialize for \(rustName) {
                fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> { Ok(self.0.clone()) }
            }

            impl valdi_core_rust::ValdiSerialize for \(rustName)Ref<'_> {
                fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> { self.0.to_owned() }
            }

            impl valdi_core_rust::ValdiPush for \(rustName) {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    valdi_core_rust::ValdiPush::push_to(&self.0, marshaller)
                }
            }

            impl valdi_core_rust::ValdiPush for \(rustName)Ref<'_> {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    valdi_core_rust::ValdiPush::push_to(&self.0, marshaller)
                }
            }

            impl valdi_core_rust::ValdiToOwned for \(rustName)Ref<'_> {
                type Owned = \(rustName);

                fn to_owned_value(&self) -> Result<Self::Owned, String> {
                    self.to_owned()
                }
            }

            """

        let schemaWriter = RustSchemaWriter(typeParameters: nil)
        try schemaWriter.appendInterface(
            cppType.declaration.fullTypeName,
            properties: model.properties,
            asyncStrictMode: bundleInfo.asyncStrictMode)
        let schema = RustSchemaGenerator.classRegistration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: schemaWriter.str,
            references: RustSchemaGenerator.references(in: model.properties))

        let referencedTypes = methods.flatMap { $0.parameters.map(\.type) + [$0.returnType] }
        return RustNativeSourceGenerator.generate(
            bundleInfo: bundleInfo,
            cppType: cppType,
            sourceFileName: sourceFileName,
            files: RustReferencedTypeGenerator.addDefinitions(
                requiredBy: referencedTypes,
                to: RustGeneratedFiles(rust: rust + schema, cHeader: "", registrationSymbols: [])))
    }
}
