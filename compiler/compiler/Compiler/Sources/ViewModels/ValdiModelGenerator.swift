//
//  ValdiModelGenerator.swift
//  Compiler
//
//  Created by Simon Corsin on 4/6/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

class ValdiModelGenerator: NativeSourceGenerator {

    private let model: ValdiModel

    init(model: ValdiModel) {
        self.model = model
    }

    func generateSwiftSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        let iosGenerator = SwiftModelGenerator(iosType: type,
                                               bundleInfo: parameters.bundleInfo,
                                               typeParameters: model.typeParameters,
                                               properties: model.properties,
                                               classMapping: parameters.classMapping,
                                               sourceFileName: parameters.sourceFileName,
                                               isInterface: model.exportAsInterface,
                                               comments: model.comments)
        return try iosGenerator.write()
    }

    func generateObjCSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        let iosGenerator = ObjCModelGenerator(iosType: type,
                                              bundleInfo: parameters.bundleInfo,
                                              typeParameters: model.typeParameters,
                                              properties: model.properties,
                                              classMapping: parameters.classMapping,
                                              sourceFileName: parameters.sourceFileName,
                                              isInterface: model.exportAsInterface,
                                              emitLegacyConstructors: model.legacyConstructors,
                                              comments: model.comments)

        return try iosGenerator.write()
    }

    func generateKotlinSources(parameters: NativeSourceParameters, fullTypeName: String) throws -> [NativeSource] {
        let androidGenerator = KotlinModelGenerator(bundleInfo: parameters.bundleInfo,
                                                    className: fullTypeName,
                                                    typeParameters: model.typeParameters,
                                                    properties: model.properties,
                                                    classMapping: parameters.classMapping,
                                                    sourceFileName: parameters.sourceFileName,
                                                    isInterface: model.exportAsInterface,
                                                    emitLegacyConstructors: model.legacyConstructors,
                                                    comments: model.comments,
                                                    usePublicFields: model.usePublicFields)

        return try androidGenerator.write()
    }

    func generateCppSources(parameters: NativeSourceParameters, cppType: CPPType) throws -> [NativeSource] {
        let cppGenerator = CppModelGenerator(cppType: cppType,
                                             bundleInfo: parameters.bundleInfo,
                                             typeParameters: model.typeParameters,
                                             properties: model.properties,
                                             classMapping: parameters.classMapping,
                                             sourceFileName: parameters.sourceFileName,
                                             isInterface: model.exportAsInterface,
                                             comments: model.comments)

        return try cppGenerator.write()
    }

    func generateRustSources(parameters: NativeSourceParameters, cppType: CPPType) throws -> [NativeSource] {
        return try RustModelGenerator(model: model,
                                      cppType: cppType,
                                      bundleInfo: parameters.bundleInfo,
                                      classMapping: parameters.classMapping,
                                      sourceFileName: parameters.sourceFileName).write()
    }

}
