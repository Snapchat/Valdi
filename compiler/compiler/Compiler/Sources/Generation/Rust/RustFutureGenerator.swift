// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct RustFutureGenerator {
    private init() {}

    static func generateDefinition(for future: RustFutureType) -> String {
        let hasValue = future.value.rustOwnedType != "()"
        let completerName = "\(future.rustName)Completer"
        let resolveParameter = hasValue ? "value: \(future.value.rustBorrowedType)" : ""
        let resolvedParameter = hasValue ? "value: \(future.value.rustBorrowedType)" : ""
        let resolvedArgument = hasValue ? "value" : ""
        let pushResolve = hasValue
            ? future.value.pushStatement(
                value: "value", to: "marshaller", owned: false, coreModule: "valdi_core_rust")
            : "marshaller.push_undefined();"
        let deserializeSuccess =
            hasValue
            ? future.value.deserializeExpression(
                from: "value", borrowed: false, coreModule: "valdi_core_rust")
            : "{ let _ = value; Ok(()) }"

        let rust = """
            pub struct \(future.rustName)(valdi_core_rust::Promise);
            pub struct \(future.rustName)Ref<'a>(valdi_core_rust::PromiseRef<'a>);
            pub struct \(completerName)(valdi_core_rust::PromiseCompleter);

            unsafe impl Send for \(completerName) {}

            impl \(future.rustName) {
                pub fn pending() -> Result<(Self, \(completerName)), String> {
                    let (future, completer) = valdi_core_rust::Promise::pending()?;
                    Ok((Self(future), \(completerName)(completer)))
                }

                pub fn resolved(\(resolvedParameter)) -> Result<Self, String> {
                    let (future, completer) = Self::pending()?;
                    completer.resolve(\(resolvedArgument))?;
                    Ok(future)
                }

                pub fn rejected(message: &str) -> Result<Self, String> {
                    let (future, completer) = Self::pending()?;
                    completer.reject(message)?;
                    Ok(future)
                }

                pub fn as_borrowed(&self) -> \(future.rustName)Ref<'_> {
                    \(future.rustName)Ref(self.0.as_borrowed())
                }

                pub fn then<F>(&self, callback: F) -> Result<(), String>
                where
                    F: FnOnce(Result<\(future.value.rustOwnedType), String>) + Send + 'static,
                {
                    self.as_borrowed().then(callback)
                }
            }

            impl Clone for \(future.rustName) {
                fn clone(&self) -> Self { Self(self.0.clone()) }
            }

            impl \(future.rustName)Ref<'_> {
                pub fn to_owned(&self) -> Result<\(future.rustName), String> {
                    Ok(\(future.rustName)(self.0.to_owned()?))
                }

                pub fn then<F>(&self, callback: F) -> Result<(), String>
                where
                    F: FnOnce(Result<\(future.value.rustOwnedType), String>) + Send + 'static,
                {
                    self.0.then_with(move |result| {
                        let result = result.and_then(|value| \(deserializeSuccess));
                        callback(result);
                    })
                }
            }

            impl \(completerName) {
                pub fn resolve(self, \(resolveParameter)) -> Result<(), String> {
                    self.0.resolve_with(|marshaller| {
                        \(pushResolve)
                        Ok(())
                    })
                }

                pub fn reject(self, message: &str) -> Result<(), String> {
                    self.0.reject(message)
                }
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(future.rustName) {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self(valdi_core_rust::ValdiDeserialize::from_valdi(value)?))
                }
            }

            impl<'a> valdi_core_rust::ValdiDeserialize<'a> for \(future.rustName)Ref<'a> {
                fn from_valdi(value: valdi_core_rust::ValueRef<'a>) -> Result<Self, String> {
                    Ok(Self(valdi_core_rust::ValdiDeserialize::from_valdi(value)?))
                }
            }

            impl valdi_core_rust::ValdiSerialize for \(future.rustName) {
                fn to_valdi(&self) -> Result<valdi_core_rust::Value, String> {
                    valdi_core_rust::ValdiSerialize::to_valdi(&self.0)
                }
            }

            impl valdi_core_rust::ValdiPush for \(future.rustName) {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    valdi_core_rust::ValdiPush::push_to(&self.0, marshaller)
                }
            }

            impl valdi_core_rust::ValdiPush for \(future.rustName)Ref<'_> {
                fn push_to(&self, marshaller: &mut valdi_core_rust::Marshaller<'_>) -> Result<(), String> {
                    valdi_core_rust::ValdiPush::push_to(&self.0, marshaller)
                }
            }

            impl valdi_core_rust::ValdiToOwned for \(future.rustName)Ref<'_> {
                type Owned = \(future.rustName);

                fn to_owned_value(&self) -> Result<Self::Owned, String> {
                    self.to_owned()
                }
            }

            """
        return rust
    }
}
