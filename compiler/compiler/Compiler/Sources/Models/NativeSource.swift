//
//  NativeSource.swift
//  Compiler
//
//  Created by Simon Corsin on 7/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct NativeSource {

    let relativePath: String?
    let filename: String
    let file: File
    /**
     An identifier which helps with grouping NativeSource instances together when using singleFileCodegen.
     */
    let groupingIdentifier: String
    /**
     An integer that helps with sorting NativeSource instances when using singleFileCodegen.
     */
    let groupingPriority: Int

    /**
     The list of local dependencies that this NativeSource has, where each entry is the filename that should match another NativeSource.
     This is used for single file codegen to help with dependency ordering. We write the sources that have no dependencies first.
     */
    let localFilenameDependencies: [String]

    init(relativePath: String?, filename: String, file: File, groupingIdentifier: String, groupingPriority: Int, localFilenameDependencies: [String] = []) {
        self.relativePath = relativePath
        self.filename = filename
        self.file = file
        self.groupingIdentifier = groupingIdentifier
        self.groupingPriority = groupingPriority
        self.localFilenameDependencies = localFilenameDependencies
    }

    static func iosNativeSourcesFromGeneratedCode(_ generatedCode: GeneratedCode,
                                                  iosType: IOSType,
                                                  bundleInfo: CompilationItem.BundleInfo) throws -> [NativeSource] {
        let typeName = iosType.name
        let headerFilename = "\(typeName).h"
        let implFilename = "\(typeName).m"

        // import the api header in api implementation
        generatedCode.apiImpl.appendHeader("#import \"\(iosType.importHeaderFilename(kind: .apiOnly))\"\n\n")
        // import the api header in utility header
        generatedCode.header.appendHeader("#import \(iosType.importHeaderStatement)\n\n")
        // import the utility header in utility implementation
        generatedCode.impl.appendHeader("#import \"\(iosType.importHeaderFilename(kind: .withUtilities))\"\n\n")

        let groupingPriority: Int
        switch iosType.kind {
        case .enum:
            groupingPriority = IOSType.enumTypeGroupingPriority
        case .stringEnum:
            groupingPriority = IOSType.enumStringTypeGroupingPriority
        case .class:
            groupingPriority = IOSType.classTypeGroupingPriority
        case .interface:
            groupingPriority = IOSType.interfaceTypeGroupingPriority
        }

        let headerData = try generatedCode.header.content.indented(indentPattern: "    ").utf8Data()
        let headerSource = NativeSource(relativePath: nil, filename: headerFilename, file: .data(headerData), groupingIdentifier: "\(bundleInfo.iosModuleName).h", groupingPriority: groupingPriority)

        let implData = try generatedCode.impl.content.indented(indentPattern: "    ").utf8Data()
        let implSource = NativeSource(relativePath: nil, filename: implFilename, file: .data(implData), groupingIdentifier: "\(bundleInfo.iosModuleName).m", groupingPriority: groupingPriority)

        var results = [headerSource, implSource]

        let apiSourcesDirectoryRelativePath = "../\(bundleInfo.iosModuleName)\(IOSType.HeaderImportKind.apiOnlyModuleNameSuffix)"
        let apiImplData = try generatedCode.apiImpl.content.indented(indentPattern: "    ").utf8Data()
        let apiImplFilename = "\(typeName).m"
        let apiImplSource = NativeSource(relativePath: apiSourcesDirectoryRelativePath,
                                         filename: apiImplFilename,
                                         file: .data(apiImplData),
                                         groupingIdentifier: "\(bundleInfo.iosModuleName)\(IOSType.HeaderImportKind.apiOnlyModuleNameSuffix).m", groupingPriority: groupingPriority)
        results.insert(apiImplSource, at: 0)

        let apiHeaderData = try generatedCode.apiHeader.content.indented(indentPattern: "    ").utf8Data()
        let apiHeaderFilename = "\(typeName).h"
        let apiHeaderSource = NativeSource(relativePath: apiSourcesDirectoryRelativePath,
                                           filename: apiHeaderFilename,
                                           file: .data(apiHeaderData),
                                           groupingIdentifier: "\(bundleInfo.iosModuleName)\(IOSType.HeaderImportKind.apiOnlyModuleNameSuffix).h", groupingPriority: groupingPriority)
        results.insert(apiHeaderSource, at: 0)

        return results
    }
}

struct NativeSourceAndPlatform {
    let source: NativeSource
    let platform: Platform
}
