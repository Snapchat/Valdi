// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

final class RustModuleGenerator {
    private let exportedModule: ExportedModule
    private let cppType: CPPType
    private let bundleInfo: CompilationItem.BundleInfo
    private let sourceFileName: GeneratedSourceFilename

    init(
        exportedModule: ExportedModule,
        cppType: CPPType,
        bundleInfo: CompilationItem.BundleInfo,
        classMapping: ResolvedClassMapping,
        sourceFileName: GeneratedSourceFilename
    ) {
        self.exportedModule = exportedModule
        self.cppType = cppType
        self.bundleInfo = bundleInfo
        self.sourceFileName = sourceFileName
        _ = classMapping
    }

    func write() throws -> [NativeSource] {
        let methods = try RustMethodResolver.resolve(
            properties: exportedModule.model.properties,
            bundleInfo: bundleInfo,
            declaration: cppType)
        let cppName = cppType.declaration.name
        let moduleName =
            cppName.hasSuffix("Module") ? String(cppName.dropLast("Module".count)) : cppName
        let rustName = moduleName.rustTypeIdentifier
        let traitName = "\(rustName)Module"
        let macroName =
            "register_\(rustName.snakeCased.rustIdentifier.replacingOccurrences(of: "r#", with: ""))"
        let prefix = "valdi_\(bundleInfo.name)_\(cppType.declaration.fullTypeName)_module".rustCSymbol
            .lowercased()
        let schemaSymbol = RustSchemaGenerator.symbol(bundleName: bundleInfo.name, cppType: cppType)
        let registrationFunction = "\(prefix)_register"
        let registrationStatic = "\(prefix)_initializer".uppercased()

        let traitMethods = methods.map { method in
            let parameters = method.parameters.map { "\($0.name): \($0.type.rustBorrowedType)" }.joined(
                separator: ", ")
            return
                "    fn \(method.property.rustName)(&self\(parameters.isEmpty ? "" : ", \(parameters)")) -> Result<\(method.returnType.rustOwnedType), String>;"
        }.joined(separator: "\n")

        let invokeCases = methods.enumerated().map { index, method in
            let arguments = method.parameters.enumerated().map { argumentIndex, parameter in
                let value = "arguments.get(\(argumentIndex))?"
                return parameter.type.deserializeBinding(
                    named: parameter.name,
                    from: value,
                    borrowed: true,
                    coreModule: "$crate::__valdi_core"
                )
            }.joined(separator: "\n")
            let callArguments = method.parameters.map(\.name).joined(separator: ", ")
            let call = "module.\(method.property.rustName)(\(callArguments))?"
            let pushResult = method.returnType.pushStatement(
                value: "result",
                to: "result_marshaller",
                owned: true,
                coreModule: "$crate::__valdi_core")
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
        }.joined(separator: ",\n").replacingOccurrences(
            of: "valdi_core_rust::",
            with: "$crate::__valdi_core::")

        let rust = """
            pub trait \(traitName) {
            \(traitMethods)
            }

            #[macro_export]
            macro_rules! \(macroName) {
                ($factory:expr) => {
                    #[unsafe(no_mangle)]
                    pub extern "C" fn \(prefix)_create() -> *mut std::ffi::c_void {
                        match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                            let module: Box<dyn $crate::\(traitName)> = Box::new(($factory)());
                            Box::into_raw(Box::new(module)) as *mut std::ffi::c_void
                        })) {
                            Ok(instance) => instance,
                            Err(_) => std::ptr::null_mut(),
                        }
                    }

                    #[unsafe(no_mangle)]
                    pub unsafe extern "C" fn \(prefix)_drop(instance: *mut std::ffi::c_void) {
                        let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                            unsafe { drop(Box::from_raw(instance as *mut Box<dyn $crate::\(traitName)>)) };
                        }));
                    }

                    #[unsafe(no_mangle)]
                    pub unsafe extern "C" fn \(prefix)_invoke(
                        instance: *mut std::ffi::c_void,
                        method: usize,
                        mut call: $crate::__valdi_core::ValdiCall,
                    ) -> $crate::__valdi_core::ValdiStatus {
                        $crate::__valdi_core::ffi_boundary(|| {
                            use $crate::*;
                            let module = unsafe { &**(instance as *mut Box<dyn $crate::\(traitName)>) };
                            match method {
            \(invokeCases),
                                _ => return Err(format!("invalid Rust module method index: {method}")),
                            };
                            Ok(())
                        })
                    }

                    extern "C" fn \(registrationFunction)() {
                        $crate::__valdi_core::run_static_initializer(|| {
                            $crate::__valdi_core::register_module(
                                "\(exportedModule.modulePath.escapedForRustStringLiteral)",
                                $crate::\(schemaSymbol)(),
                                \(methods.count),
                                \(prefix)_create,
                                \(prefix)_drop,
                                \(prefix)_invoke,
                            )
                        });
                    }

                    #[used]
                    #[unsafe(no_mangle)]
                    #[cfg_attr(target_vendor = "apple", unsafe(link_section = "__DATA,__mod_init_func"))]
                    #[cfg_attr(any(target_os = "linux", target_os = "android"), unsafe(link_section = ".init_array"))]
                    pub static \(registrationStatic): extern "C" fn() = \(registrationFunction);
                };
            }

            """

        let methodConstants = methods.enumerated().map { index, method in
            let methodName = method.property.rustName.replacingOccurrences(of: "r#", with: "")
            return "    \(prefix)_method_\(methodName) = \(index),"
        }.joined(separator: "\n")

        let cHeader = """
            #include "valdi_core/cpp/Marshalling/RustValdi.h"
            #ifdef __cplusplus
            extern "C" {
            #endif
            enum {
            \(methodConstants)
            };
            void* \(prefix)_create(void);
            void \(prefix)_drop(void* instance);
            ValdiRustStatus \(prefix)_invoke(
                void* instance,
                size_t method,
                ValdiRustCall call);
            #ifdef __cplusplus
            }
            #endif

            """

        let schemaWriter = RustSchemaWriter(typeParameters: nil)
        try schemaWriter.appendInterface(
            cppType.declaration.fullTypeName,
            properties: exportedModule.model.properties,
            asyncStrictMode: bundleInfo.asyncStrictMode)
        let schema = RustSchemaGenerator.classRegistration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: schemaWriter.str,
            references: RustSchemaGenerator.references(in: exportedModule.model.properties))

        let referencedTypes = methods.flatMap { $0.parameters.map(\.type) + [$0.returnType] }
        let files = RustReferencedTypeGenerator.addDefinitions(
            requiredBy: referencedTypes,
            to: RustGeneratedFiles(
                rust: rust + schema,
                cHeader: cHeader,
                registrationSymbols: [registrationStatic]))
        return RustNativeSourceGenerator.generate(
            bundleInfo: bundleInfo,
            cppType: cppType,
            sourceFileName: sourceFileName,
            files: files)
    }
}
