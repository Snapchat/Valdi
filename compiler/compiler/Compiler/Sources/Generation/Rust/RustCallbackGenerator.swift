// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct RustCallbackGenerator {
    private init() {}

    static func generateDefinition(for callback: RustCallbackType) -> String {
        let rustParameters = callback.parameters.map {
            "\($0.name): \($0.type.rustBorrowedType)"
        }.joined(separator: ", ")
        let parameterSuffix = rustParameters.isEmpty ? "" : ", \(rustParameters)"
        let argumentNames = callback.parameters.map(\.name).joined(separator: ", ")

        let createArguments = callback.parameters.enumerated().map { index, parameter in
            return parameter.type.deserializeBinding(
                named: parameter.name,
                from: "arguments.get(\(index))?",
                borrowed: true,
                coreModule: "valdi_core_rust")
        }.joined(separator: "\n")
        let createCall = "callback.call(\(argumentNames))?"
        let pushResult = callback.returnType.pushStatement(
            value: "result",
            to: "result_marshaller",
            asReturnValue: true,
            coreModule: "valdi_core_rust")
        var resultStatements = [String]()
        if !callback.parameters.isEmpty {
            resultStatements.append("let arguments = unsafe { call.arguments() };")
            resultStatements.append(createArguments)
        }
        resultStatements.append(createCall)
        let resultBody = RustCodeFormatting.indentLines(
            resultStatements.joined(separator: "\n"), spaces: 16)

        let callMarshaller = callback.parameters.isEmpty ? "_marshaller" : "marshaller"
        let callValues = callback.parameters.map { parameter in
            "                \(parameter.type.pushStatement(value: parameter.name, to: callMarshaller, asReturnValue: false, coreModule: "valdi_core_rust"))"
        }.joined(separator: "\n")
        let resultReader =
            callback.returnType.rustOwnedType == "()"
            ? "|_| Ok(())"
            : "|value| \(callback.returnType.deserializeExpression(from: "value", borrowed: false, coreModule: "valdi_core_rust"))"

        let rust = """
            pub trait \(callback.rustName)Fn {
                fn call(&self\(parameterSuffix)) -> Result<\(callback.returnType.rustOwnedType), String>;
            }

            pub struct \(callback.rustName)(valdi_core_rust::Function);
            pub struct \(callback.rustName)Ref<'a>(valdi_core_rust::FunctionRef<'a>);

            impl \(callback.rustName) {
                pub fn new<T: \(callback.rustName)Fn + 'static>(callback: T) -> Result<Self, String> {
                    Ok(Self(valdi_core_rust::Function::new(move |mut call| {
                        let result = {
            \(resultBody)
                        };
                        let mut result_storage = unsafe { call.result() };
                        let result_marshaller = &mut result_storage;
                        \(pushResult)
                        Ok(())
                    })?))
                }

                pub fn as_borrowed(&self) -> \(callback.rustName)Ref<'_> {
                    \(callback.rustName)Ref(self.0.as_borrowed())
                }

                pub fn call(&self\(parameterSuffix)) -> Result<\(callback.returnType.rustOwnedType), String> {
                    self.as_borrowed().call(\(argumentNames))
                }
            }

            impl Clone for \(callback.rustName) {
                fn clone(&self) -> Self { Self(self.0.clone()) }
            }

            impl \(callback.rustName)Ref<'_> {
                pub fn to_owned(&self) -> Result<\(callback.rustName), String> {
                    Ok(\(callback.rustName)(self.0.to_owned()?))
                }

                pub fn call(&self\(parameterSuffix)) -> Result<\(callback.returnType.rustOwnedType), String> {
                    self.0.call_with(
                        |\(callMarshaller)| {
            \(callValues)
                            Ok(())
                        },
                        \(resultReader),
                    )
                }
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(callback.rustName) {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self(valdi_core_rust::ValdiDeserialize::from_valdi(value)?))
                }
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(callback.rustName)Ref<'a> {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self(valdi_core_rust::ValdiDeserialize::from_valdi(value)?))
                }
            }

            impl valdi_core_rust::ValdiSerialize for \(callback.rustName) {
                fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> {
                    valdi_core_rust::ValdiSerialize::to_valdi(&self.0)
                }
            }

            impl valdi_core_rust::ValdiPush for \(callback.rustName) {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    valdi_core_rust::ValdiPush::push_to(&self.0, marshaller)
                }
            }

            impl valdi_core_rust::ValdiPush for \(callback.rustName)Ref<'_> {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    valdi_core_rust::ValdiPush::push_to(&self.0, marshaller)
                }
            }

            impl valdi_core_rust::ValdiToOwned for \(callback.rustName)Ref<'_> {
                type Owned = \(callback.rustName);

                fn to_owned_value(&self) -> Result<Self::Owned, String> {
                    self.to_owned()
                }
            }

            """
        return rust
    }
}
