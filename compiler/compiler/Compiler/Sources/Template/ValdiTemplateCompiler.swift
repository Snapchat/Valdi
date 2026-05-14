//
//  ValdiTemplateCompiler.swift
//  Compiler
//
//  Created by Simon Corsin on 4/6/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

private extension String {

    var isImplicitAction: Bool {
        // checking if string is `onSomething`
        guard hasPrefix("on") && count >= 3 else { return false }

        return self[2].isUppercase
    }

}

class ValdiTemplateCompiler {

    private let template: ValdiRawTemplate
    private var viewModel: ValdiModel?
    private let actions: [ValdiAction]

    private var idSequence = 0
    private var assignedIds: Set<String> = []
    private var slotNames: Set<String> = []

    private let classMapping: ResolvedClassMapping

    private let compileIOS: Bool
    private let compileAndroid: Bool
    private let documentContent: String

    init(template: ValdiRawTemplate,
         viewModel: ValdiModel?,
         classMapping: ResolvedClassMapping,
         actions: [ValdiAction],
         compileIOS: Bool,
         compileAndroid: Bool,
         documentContent: String) {
        self.template = template
        self.actions = actions
        self.viewModel = viewModel
        self.classMapping = classMapping
        self.compileIOS = compileIOS
        self.compileAndroid = compileAndroid
        self.documentContent = documentContent
    }

    private func generateId() -> String {
        idSequence += 1

        return "!\(idSequence)"
    }

    private func makeAttributeKey(variable: String, nodeId: String, attributeName: String) -> String {
        return "\(variable)['\(nodeId)']['\(attributeName)']"
    }

    private func throwError(node: ValdiRawNode, message: String) throws -> Never {
        throw CompilerError(type: "Compilation error", message: message, atZeroIndexedLine: node.lineNumber, column: node.columnNumber, inDocument: documentContent)
    }

    private func doResolve(node: ValdiRawNode) throws -> ValdiClass {
        do {
            return try classMapping.resolve(nodeType: node.nodeType)
        } catch let error {
            try throwError(node: node, message: error.legibleLocalizedDescription)
        }
    }

    private func makeDebugInfo(node: ValdiRawNode, attribute: ValdiRawNodeAttribute) -> ValdiJSAttributeDebugInfo {
        // TODO(simon): Move to a custom parser instead of NSXMLParser so that we can get the exact line and column number from
        // the vue file.
        return ValdiJSAttributeDebugInfo(originalExpression: "\(attribute.name)=\"{{ \(attribute.value) }}\"", line: node.lineNumber, column: node.columnNumber)
    }

    private func compile(node: ValdiRawNode, parentJSElement: ValdiJSElement?, actions: inout [ValdiAction], parentMapping: ValdiClass?, parentIsRoot: Bool, rootClass: ValdiClass?) throws -> ValdiJSElement {
        let mapping = try doResolve(node: node)

        if let rootClass = rootClass, rootClass == mapping {
            try throwError(node: node, message: "Component is trying to recursively inflate itself")
        }

        let componentPath = mapping.valdiViewPath

        let id = node.id.isEmpty ? generateId() : node.id

        let jsElement = ValdiJSElement(id: id, componentPath: componentPath, nodeType: node.nodeType, jsxName: mapping.jsxName)

        if let forEachExpr = node.forEachExpr {
            jsElement.forEachExpr = ValdiJSAttribute(attribute: "forEach", value: forEachExpr.value, valueIsExpr: true, evaluateOnce: false, isViewModelField: false, debugInfo: makeDebugInfo(node: node, attribute: forEachExpr))
        }

        guard !assignedIds.contains(id) else {
            try throwError(node: node, message: "Node id \(id) was already assigned.")
        }
        assignedIds.insert(id)

        let nonNilCount = [node.ifExpr, node.elseIfExpr, node.elseExpr, node.unlessExpr].count { $0 != nil }
        if nonNilCount > 1 {
            try throwError(node: node, message: "Attributes 'render-if', 'render-else-if', 'render-else', and 'render-unless' are mutually exclusive")
        }

        if let ifExpr = node.ifExpr {
            // Truthty evaluation converted to bool
            jsElement.renderIfExpr = ValdiJSAttribute(attribute: "shouldRender", value: ifExpr.value, valueIsExpr: true, evaluateOnce: ifExpr.evaluateOnce, isViewModelField: false, debugInfo: makeDebugInfo(node: node, attribute: ifExpr))
        } else if let elseIfExpr = node.elseIfExpr {
            jsElement.renderElseIfExpr = ValdiJSAttribute(attribute: "shouldRender", value: elseIfExpr.value, valueIsExpr: true, evaluateOnce: elseIfExpr.evaluateOnce, isViewModelField: false, debugInfo: makeDebugInfo(node: node, attribute: elseIfExpr))
        } else if let elseExpr = node.elseExpr {
            jsElement.renderElseIfExpr = ValdiJSAttribute(attribute: "shouldRender", value: "true", valueIsExpr: true, evaluateOnce: elseExpr.evaluateOnce, isViewModelField: false, debugInfo: makeDebugInfo(node: node, attribute: elseExpr))
        } else if let unlessExpr = node.unlessExpr {
            // renderUnless is not conditional render, as it inflates the view immediately before going to JS
            jsElement.renderIfExpr = ValdiJSAttribute(attribute: "shouldRender", value: "!(\(unlessExpr.value))", valueIsExpr: true, evaluateOnce: unlessExpr.evaluateOnce, isViewModelField: false, debugInfo: makeDebugInfo(node: node, attribute: unlessExpr))
        }

        for attribute in node.customAttributes {
            let attributeValue = attribute.value

            if attribute.name == "key" && !attribute.isViewModelField {
                if attribute.evaluateOnce || !attribute.valueIsExpr {
                    try throwError(node: node, message: "'key' need to be a regular JS expression")
                }
                jsElement.forEachKey = ValdiJSAttribute(attribute: "key", value: attribute.value, valueIsExpr: true, evaluateOnce: attribute.evaluateOnce, isViewModelField: attribute.isViewModelField, debugInfo: makeDebugInfo(node: node, attribute: attribute))
                continue
            }

            if mapping.isSlot {
                if attribute.name == "name" {
                    jsElement.slotName = attributeValue
                    continue
                }
                if attribute.name == "ref" {
                    if !attribute.valueIsExpr {
                        try throwError(node: node, message: "ref must be an expression")
                    }
                    jsElement.slotRefExpr = attributeValue
                    continue
                }

                try throwError(node: node, message: "Attributes are not supported on Slots.")
            }

            jsElement.attributes.append(ValdiJSAttribute(attribute: attribute.name, value: attributeValue, valueIsExpr: attribute.valueIsExpr, evaluateOnce: attribute.evaluateOnce, isViewModelField: attribute.isViewModelField, debugInfo: makeDebugInfo(node: node, attribute: attribute)))
        }

        // If parent is a component we process the elements slot name
        if let parentJSElement = parentJSElement, !parentIsRoot, parentMapping?.valdiViewPath != nil || parentJSElement.attributes.contains(where: { $0.attribute == "componentClass" }) {
            let destSlotName = node.destSlotName ?? "default"
            jsElement.insertInSlotName = destSlotName
        }

        // We use the default view if we can't resolve the class
        if let androidClassName = mapping.androidClassName {
            jsElement.androidViewClass = androidClassName
        } else if !mapping.isSlot {
            if mapping.isLayout {
                jsElement.androidViewClass = "Layout"
            } else if parentJSElement == nil || componentPath != nil {
                jsElement.androidViewClass = try classMapping.resolve(nodeType: "View").androidClassName!
            } else if compileAndroid && mapping.valdiViewPath == nil {
                try throwError(node: node, message: "Could not resolve Android class for node type \(node.nodeType)")
            }
        }
        if let iosTypeName = mapping.iosType?.name {
            jsElement.iosViewClass = iosTypeName
        } else if !mapping.isSlot {
            if mapping.isLayout {
                jsElement.iosViewClass = "Layout"
            } else if parentJSElement == nil || componentPath != nil {
                jsElement.iosViewClass = try classMapping.resolve(nodeType: "View").iosType!.name
            } else if compileIOS && mapping.valdiViewPath == nil {
                try throwError(node: node, message: "Could not resolve Android class for node type \(node.nodeType)")
            }
        }
        if let webViewClassName = mapping.webViewClassName {
            jsElement.webViewClass = webViewClassName
        }
        if let macosViewClassName = mapping.macosViewClassName {
            jsElement.macosViewClass = macosViewClassName
        }

        if mapping.isSlot {
            let slotName = jsElement.slotName ?? "default"
            jsElement.slotName = slotName

            if slotNames.contains(slotName) {
                try throwError(node: node, message: "slot '\(slotName)' was already assigned")
            }

            slotNames.insert(slotName)
        }

        jsElement.children += try node.children.map {
            switch $0 {
            case let .expression(expression):
                return .expression(ValdiJSExpression(expression: expression))
            case let .node(node):
                return .element(try compile(node: node,
                                     parentJSElement: jsElement,
                                     actions: &actions,
                                     parentMapping: mapping,
                                     parentIsRoot: parentJSElement == nil,
                                     rootClass: rootClass))
            }
        }

        return jsElement
    }

    func compile() throws -> TemplateCompilerResult {
        idSequence = 0
        assignedIds.removeAll()
        slotNames.removeAll()

        guard let rootNode = self.template.rootNode else {
            throw CompilerError("This template has no root node")
        }

        var actions = self.actions
        let rootElement = try compile(node: rootNode, parentJSElement: nil, actions: &actions, parentMapping: nil, parentIsRoot: false, rootClass: nil)

        return TemplateCompilerResult(rootElement: rootElement, actions: actions)
    }

}
