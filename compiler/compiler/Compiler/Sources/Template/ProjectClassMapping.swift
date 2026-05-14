//
//  ProjectClassMapping.swift
//  Compiler
//
//  Created by Simon Corsin on 8/12/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct ValdiClass: Equatable, Codable {
    var androidClassName: String?
    var iosType: IOSType?
    var webViewClassName: String?
    var macosViewClassName: String?
    let jsxName: String?
    var valdiViewPath: ComponentPath?
    var isLayout: Bool
    var isSlot: Bool
    let sourceFilePath: String
    let sourceModuleName: String
}

func==(lhs: ValdiClass, rhs: ValdiClass) -> Bool {
    return lhs.androidClassName == rhs.androidClassName && lhs.iosType?.name == rhs.iosType?.name && lhs.iosType?.importPrefix == rhs.iosType?.importPrefix && lhs.webViewClassName == rhs.webViewClassName && lhs.macosViewClassName == rhs.macosViewClassName && lhs.valdiViewPath == rhs.valdiViewPath && lhs.sourceModuleName == rhs.sourceModuleName
}

class ValdiClassResolveError: CompilerError {

    init(nodeType: String, error: String) {
        super.init("Could not resolve Valdi class '\(nodeType)': \(error)")
    }
}

struct RegisteredValdiClass {
    var valdiClass: ValdiClass
    var allowOverride: Bool
}

struct ProjectClassMapping {

    private var classByFullyQualifiedName = [String: RegisteredValdiClass]()

    private let allowMappingOverride: Bool

    init(allowMappingOverride: Bool) {
        self.allowMappingOverride = allowMappingOverride
    }

    mutating func register(bundle: CompilationItem.BundleInfo, nodeType: String, valdiViewPath: ComponentPath?, iosType: IOSType?, androidClassName: String?, webViewClassName: String? = nil, macosViewClassName: String? = nil, jsxName: String?, isLayout: Bool, sourceFilePath: String) throws {
        try register(bundle: bundle, nodeType: nodeType, valdiViewPath: valdiViewPath, iosType: iosType, androidClassName: androidClassName, webViewClassName: webViewClassName, macosViewClassName: macosViewClassName, jsxName: jsxName, isLayout: isLayout, isSlot: false, sourceFilePath: sourceFilePath)
    }

    mutating func register(bundle: CompilationItem.BundleInfo, nodeType: String, valdiViewPath: ComponentPath?, iosType: IOSType?, androidClassName: String?, webViewClassName: String? = nil, macosViewClassName: String? = nil, jsxName: String?, isLayout: Bool, isSlot: Bool, sourceFilePath: String) throws {
        let classDescription = ValdiClass(androidClassName: androidClassName, iosType: iosType, webViewClassName: webViewClassName, macosViewClassName: macosViewClassName, jsxName: jsxName, valdiViewPath: valdiViewPath, isLayout: isLayout, isSlot: isSlot, sourceFilePath: sourceFilePath, sourceModuleName: bundle.name)

        let className =  "\(bundle.name).\(nodeType)"

        if !allowMappingOverride {
            if let existingMapping = classByFullyQualifiedName[className], !existingMapping.allowOverride {
                throw CompilerError("Root node class conflict for \(className), was previously registered by \(existingMapping.valdiClass.sourceFilePath), attempting to register by \(sourceFilePath)")
            }
        }

        classByFullyQualifiedName[className] = RegisteredValdiClass(valdiClass: classDescription, allowOverride: false)
    }

    mutating func registerResolved(fullyQualifiedName: String, klass: ValdiClass) {
        classByFullyQualifiedName[fullyQualifiedName] = RegisteredValdiClass(valdiClass: klass, allowOverride: true)
    }

    func resolveClass(nodeType: String, currentBundle: CompilationItem.BundleInfo, localMapping: ValdiClassMapping?) throws -> ValdiClass {
        // First local at local mapping (declared inside the .vue file)
        if let resolvedLocalMapping = localMapping?.nodeMappingByClass[nodeType] {
            return ValdiClass(androidClassName: resolvedLocalMapping.androidClassName, iosType: resolvedLocalMapping.iosType, webViewClassName: nil, macosViewClassName: nil, jsxName: nil, valdiViewPath: nil, isLayout: false, isSlot: false, sourceFilePath: "", sourceModuleName: currentBundle.name)
        }

        let components = nodeType.split(separator: ".")
        guard components.count <= 2 else {
            throw CompilerError("Invalid nodeType '\(nodeType)', can only have one dot to specify the bundle")
        }

        if components.count == 2 {
            // Absolute import
            guard let valdiClass = classByFullyQualifiedName[nodeType]?.valdiClass else {
                throw ValdiClassResolveError(nodeType: nodeType, error: "The class was not found in the class path")
            }
            let currentModuleDeps = currentBundle.dependencies.map { $0.name }
            if !currentModuleDeps.contains(valdiClass.sourceModuleName) {
                throw ValdiClassResolveError(nodeType: nodeType, error: "The class was found but the module \(currentBundle.name) does not have a dependency on \(valdiClass.sourceModuleName)")
            }

            return valdiClass
        } else {
            // Implicit import

            // First look inside the current module
            if let valdiClass = classByFullyQualifiedName["\(currentBundle.name).\(nodeType)"]?.valdiClass {
                return valdiClass
            }

            // Then look if it exists in another module
            let modulesToSearch = currentBundle.dependencies
            let foundClasses = modulesToSearch.map { "\($0.name).\(nodeType)" }.compactMap { classByFullyQualifiedName[$0]?.valdiClass }
            if foundClasses.isEmpty {
                let modulesList = modulesToSearch.map { $0.name }.joined(separator: ", ")
                throw ValdiClassResolveError(nodeType: nodeType, error: "The class was not found in the modules \(modulesList)")
            }
            if foundClasses.count > 1 {
                let modulesList = foundClasses.map { $0.sourceModuleName }.joined(separator: ", ")
                throw ValdiClassResolveError(nodeType: nodeType, error: """
                    The class was found in \(foundClasses.count) modules: \(modulesList).
                    You can fix this issue by explicitly specifying which class to use using
                    <\(foundClasses[0].sourceModuleName).\(nodeType)/> in your vue XML for instance, or by removing a dependency in your module.yaml.
                    """)
            }
            return foundClasses[0]
        }
    }

    func copyMappings() -> [String: ValdiClass] {
        var out: [String: ValdiClass] = [:]
        for (key, registeredClass) in classByFullyQualifiedName {
            out[key] = registeredClass.valdiClass
        }
        return out
    }
}
