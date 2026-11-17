// Copyright © 2024 Snap, Inc. All rights reserved.

struct NativeSourceParameters {
    let bundleInfo: CompilationItem.BundleInfo
    let sourceFileName: GeneratedSourceFilename
    let classMapping: ResolvedClassMapping
    let iosType: IOSType?
    let androidTypeName: String?
    let cppType: CPPType?
}

protocol NativeSourceGenerator {
    func generateSwiftSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource]
    func generateObjCSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource]
    func generateKotlinSources(parameters: NativeSourceParameters, fullTypeName: String) throws -> [NativeSource]
    func generateCppSources(parameters: NativeSourceParameters, cppType: CPPType) throws -> [NativeSource]
    func generateRustSources(parameters: NativeSourceParameters, cppType: CPPType) throws -> [NativeSource]
}

extension NativeSourceGenerator {
    func generate(parameters: NativeSourceParameters) throws -> [NativeSourceAndPlatform] {
        var sources = [NativeSourceAndPlatform]()

        if let iosType = parameters.iosType {
            switch parameters.bundleInfo.iosLanguage {
            case IOSLanguage.swift:
                sources += try generateSwiftSources(parameters: parameters, type: iosType).map {
                    NativeSourceAndPlatform(source: $0, platform: .ios)
                }
            case IOSLanguage.objc:
                sources += try generateObjCSources(parameters: parameters, type: iosType).map {
                    NativeSourceAndPlatform(source: $0, platform: .ios)
                }
            case IOSLanguage.both:
                sources += try generateSwiftSources(parameters: parameters, type: iosType).map {
                    NativeSourceAndPlatform(source: $0, platform: .ios)
                }
                sources += try generateObjCSources(parameters: parameters, type: iosType).map {
                    NativeSourceAndPlatform(source: $0, platform: .ios)
                }
            }
        }

        if let androidName = parameters.androidTypeName {
            sources += try generateKotlinSources(parameters: parameters, fullTypeName: androidName).map {
                NativeSourceAndPlatform(source: $0, platform: .android)
            }
        }
        if let cppType = parameters.cppType {
            if parameters.bundleInfo.nativeLanguage == .rust {
                sources += try generateRustSources(parameters: parameters, cppType: cppType).map {
                    NativeSourceAndPlatform(source: $0, platform: .cpp)
                }
            } else {
                sources += try generateCppSources(parameters: parameters, cppType: cppType).map {
                    NativeSourceAndPlatform(source: $0, platform: .cpp)
                }
            }
        }

        return sources
    }
}
