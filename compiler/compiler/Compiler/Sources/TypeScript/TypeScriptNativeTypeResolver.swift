//
//  TypeScriptNativeTypeResolver.swift
//  Compiler
//
//  Created by Simon Corsin on 12/10/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct TypeScriptNativeClass: Codable {
    let tsTypeName: String
    let iosType: IOSType?
    let androidClass: String?
    let cppType: CPPType?
    let kind: NativeTypeKind
    let isGenerated: Bool
    let marshallAsUntyped: Bool
    let marshallAsUntypedMap: Bool
    let convertedFromFunctionPath: String?
}

struct TypeScriptNativeConvertedClass: Codable {
    let convertFunctionPath: String
    let toCompilationPath: String
    let toTypeName: String
}

enum TypeScriptNativeTypeKind {
    case cls(TypeScriptNativeClass)
    case convertedClass(TypeScriptNativeConvertedClass)
}

struct TypeScriptNativeType {
    let emittingBundleName: String
    let kind: TypeScriptNativeTypeKind
}

struct SerializedTypeScriptNativeType: Codable {
    let compilationPath: String
    let tsTypeName: String
    let emittingBundleName: String
    let cls: TypeScriptNativeClass?
    let convertedClass: TypeScriptNativeConvertedClass?
}

struct SerializedTypeScriptNativeTypeResolver: Codable {
    let entries: [SerializedTypeScriptNativeType]
}

final class TypeScriptNativeTypeResolver {
    private let rootURL: URL

    private let lock = DispatchSemaphore.newLock()

    private var typesByPath = [String: [String: TypeScriptNativeType]]()
    init(rootURL: URL) {
        self.rootURL = rootURL
    }

    func restore(fromSerialized serialized: SerializedTypeScriptNativeTypeResolver) throws {
        try lock.lock {
            for entry in serialized.entries {
                if let cls = entry.cls {
                    typesByPath[entry.compilationPath, default: [:]][entry.tsTypeName] = TypeScriptNativeType(emittingBundleName: entry.emittingBundleName, kind: .cls(cls))
                } else if let convertedClass = entry.convertedClass {
                    typesByPath[entry.compilationPath, default: [:]][entry.tsTypeName] = TypeScriptNativeType(emittingBundleName: entry.emittingBundleName, kind: .convertedClass(convertedClass))
                } else {
                    throw CompilerError("Could not resolve entry type")
                }
            }
        }
    }

    func serialize() -> SerializedTypeScriptNativeTypeResolver {
        var entries = [SerializedTypeScriptNativeType]()
        lock.lock {
            for (compilationPath, values) in typesByPath {
                for (tsTypeName, type) in values {
                    switch type .kind{
                    case .cls(let cls):
                        entries.append(SerializedTypeScriptNativeType(compilationPath: compilationPath, tsTypeName: tsTypeName, emittingBundleName: type.emittingBundleName, cls: cls, convertedClass: nil))
                    case .convertedClass(let convertedClass):
                        entries.append(SerializedTypeScriptNativeType(compilationPath: compilationPath, tsTypeName: tsTypeName, emittingBundleName: type.emittingBundleName, cls: nil, convertedClass: convertedClass))
                    }
                }
            }
        }

        entries.sort { left, right in
            if left.compilationPath < right.compilationPath {
                return true
            } else if left.compilationPath > right.compilationPath {
                return false
            }

            return left.tsTypeName < right.tsTypeName
        }

        return SerializedTypeScriptNativeTypeResolver(entries: entries)
    }

    private func makeSrcWithoutExtension(src: TypeScriptItemSrc) -> TypeScriptItemSrc {
        let compilationPath = src.compilationPath.removing(suffixes: FileExtensions.typescriptFileExtensionsDotted)
        return TypeScriptItemSrc(compilationPath: compilationPath, sourceURL: src.sourceURL)
    }

    func register(src: TypeScriptItemSrc,
                  emittingBundleName: String,
                  tsTypeName: String,
                  iosType: IOSType?,
                  androidClass: String?,
                  cppType: CPPType?,
                  kind: NativeTypeKind,
                  isGenerated: Bool,
                  marshallAsUntyped: Bool,
                  marshallAsUntypedMap: Bool) -> TypeScriptNativeClass {
        let standardizedSrc = makeSrcWithoutExtension(src: src)
        let nativeClass = TypeScriptNativeClass(tsTypeName: tsTypeName,
                                                iosType: iosType,
                                                androidClass: androidClass,
                                                cppType: cppType,
                                                kind: kind,
                                                isGenerated: isGenerated,
                                                marshallAsUntyped: marshallAsUntyped,
                                                marshallAsUntypedMap: marshallAsUntypedMap,
                                                convertedFromFunctionPath: nil)

        lock.lock {
            typesByPath[standardizedSrc.compilationPath, default: [:]][tsTypeName] = TypeScriptNativeType(emittingBundleName: emittingBundleName, kind: .cls(nativeClass))
        }

        return nativeClass
    }

    func registerTypeConverter(src: TypeScriptItemSrc,
                               emittingBundleName: String,
                               tsTypeName: String,
                               fromTypePath: String,
                               tsFromTypeName: String,
                               toTypePath: String,
                               tsToTypeName: String) {
        let standardizedSrc = makeSrcWithoutExtension(src: src)
        let functionPath = "\(tsTypeName)@\(standardizedSrc.compilationPath)"

        let standardizedFromType = makeSrcWithoutExtension(src: TypeScriptItemSrc(compilationPath: fromTypePath, sourceURL: src.sourceURL))
        let standardizedToType = makeSrcWithoutExtension(src: TypeScriptItemSrc(compilationPath: toTypePath, sourceURL: src.sourceURL))

        lock.lock {
            typesByPath[standardizedFromType.compilationPath, default: [:]][tsFromTypeName] = TypeScriptNativeType(emittingBundleName: emittingBundleName, kind: .convertedClass(TypeScriptNativeConvertedClass(convertFunctionPath: functionPath, toCompilationPath: standardizedToType.compilationPath, toTypeName: tsToTypeName)))
        }
    }

    private func lockFreeResolve(compilationPath: String, tsTypeName: String) -> TypeScriptNativeClass? {
        guard let types = typesByPath[compilationPath], let type = types[tsTypeName] else {
            return nil
        }

        switch type.kind {
        case .cls(let nativeClass):
            return nativeClass
        case .convertedClass(let convertedClass):
            guard let nativeClass = lockFreeResolve(compilationPath: convertedClass.toCompilationPath, tsTypeName: convertedClass.toTypeName) else {
                return nil
            }

            return TypeScriptNativeClass(tsTypeName: nativeClass.tsTypeName,
                                         iosType: nativeClass.iosType,
                                         androidClass: nativeClass.androidClass,
                                         cppType: nativeClass.cppType,
                                         kind: nativeClass.kind,
                                         isGenerated: nativeClass.isGenerated,
                                         marshallAsUntyped: nativeClass.marshallAsUntyped,
                                         marshallAsUntypedMap: nativeClass.marshallAsUntypedMap,
                                         convertedFromFunctionPath: convertedClass.convertFunctionPath)
        }
    }

    func resolve(src: TypeScriptItemSrc, tsTypeName: String) -> TypeScriptNativeClass? {
        let standardizedSrc = makeSrcWithoutExtension(src: src)
        return lock.lock {
            return lockFreeResolve(compilationPath: standardizedSrc.compilationPath, tsTypeName: tsTypeName)
        }
    }

    func resolve(filePath: String, tsTypeName: String) -> TypeScriptNativeClass? {
        let fileURL = URL(fileURLWithPath: filePath, relativeTo: rootURL)
        return resolve(src: TypeScriptItemSrc(compilationPath: filePath, sourceURL: fileURL), tsTypeName: tsTypeName)
    }

}
