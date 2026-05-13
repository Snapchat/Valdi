//
//  TypeScriptGenerator.swift
//  Compiler
//
//  Created by Simon Corsin on 4/29/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct TypeScriptResult {

    var typeScript: String
}

fileprivate extension String {

    var escapingQuotesAndNewlines: String {
        return replacingOccurrences(of: "'", with: "\\'").replacingOccurrences(of: "\n", with: "\\n")
    }

}

final class TypeScriptGenerator {

    private let logger: ILogger
    private let elements: ValdiJSElement
    private let customComponentClass: String?
    private let actions: [ValdiAction]
    private let useLegacyActions: Bool
    private let hasUserScript: Bool
    private let sourceURL: URL
    private let symbolsToImport: [TypeScriptSymbol]
    private let emitDebug: Bool
    private let cssModulePath: String?

    private let nameAllocator = PropertyNameAllocator()
    private let nodePrototypesSection = CodeWriter()
    private let importSection = CodeWriter()
    private var importedComponents: [ComponentPath: String] = [:]

    init(logger: ILogger, customComponentClass: String?, elements: ValdiJSElement, actions: [ValdiAction], useLegacyActions: Bool, hasUserScript: Bool, sourceURL: URL, symbolsToImport: [TypeScriptSymbol], emitDebug: Bool, cssModulePath: String?) {
        self.logger = logger
        self.customComponentClass = customComponentClass
        self.elements = elements
        self.actions = actions
        self.useLegacyActions = useLegacyActions
        self.hasUserScript = hasUserScript
        self.sourceURL = sourceURL
        self.symbolsToImport = symbolsToImport
        self.emitDebug = emitDebug
        self.cssModulePath = cssModulePath
    }

    private func resolvePreviousSiblingRenderIfResults(element: ValdiJSElement, parent: ValdiJSElement?) -> [String] {
        guard let parent = parent,
              let onlyElementChildren = parent.children.compactMap({ $0.element }).nonEmpty,
              let index = onlyElementChildren.firstIndex(where: { $0 === element }), index > 0 else {
             return []
         }

        let previousElement = onlyElementChildren[index - 1]

        var resultVars = [String]()
        if previousElement.renderElseIfExpr != nil {
            // This previous element also depends on previous siblings, we need to resolve them as well
            resultVars += resolvePreviousSiblingRenderIfResults(element: previousElement, parent: parent)
        }

        resultVars.append(TypeScriptGenerator.makePrivateVar(named: TypeScriptGenerator.renderIfResultName, forId: previousElement.id))

        return resultVars
    }

    private func appendDebugLine(context: String, attribute: ValdiJSAttribute, lines: inout [String]) {
        // TODO(simon): Not supported currently in new renderer, see whether we should re-introduce it

//        guard let debugInfo = attribute.debugInfo, emitDebug else {
//            return
//        }
//
//        lines.append("renderer.willEvaluate('\(context.escapingQuotesAndNewlines)', '\(attribute.value.escapingQuotesAndNewlines)', \(debugInfo.line + 1), \(debugInfo.column));")
    }

    private func toPropertyList(dict: [String: String]) -> String {
        if dict.isEmpty {
            return "[]"
        }

        let keyValues = dict.sorted(by: { $0 < $1 })

        let body = keyValues.map { "\n  '\($0)', \($1)" }.joined(separator: ",")
        return "[\(body)\n]"
    }

    private func makeClassName(fileName: String, suffix: String) -> String {
        let name = fileName.split(separator: "/").last!.split(separator: ".").first!.pascalCased
        return nameAllocator.allocate(property: name + suffix).description
    }

    private func importComponent(componentPath: ComponentPath) throws -> String {
        if let variableName = importedComponents[componentPath] {
            return variableName
        }

        let moduleVariableName = makeClassName(fileName: componentPath.fileName, suffix: "")

        importSection.appendBody("import * as \(moduleVariableName) from '\(componentPath.fileName)';\n")

        let resolvedVariableName = "\(moduleVariableName).\(componentPath.exportedMember)"
        importedComponents[componentPath] = resolvedVariableName
        return resolvedVariableName
    }

    private func makeSetAttributeStatement(evalOnceCheck: String, setAttributeStatement: String, evaluateOnce: Bool) -> String {
        if evaluateOnce {
            return """
                if (!renderer.\(evalOnceCheck)) {
                renderer.\(setAttributeStatement);
                }
                """
        } else {
            return "renderer.\(setAttributeStatement);"
        }
    }

    private func generateRenderTemplateInnerBody(element: ValdiJSElement, parent: ValdiJSElement?, key: String?) throws -> [String] {
        var resolvedAttributes = element.attributes

        var staticProperties: [String: String] = [:]
        staticProperties["tag"] = "'\(element.nodeType)'"
        if !element.id.hasPrefix("!") {
            staticProperties["id"] = "'\(element.id)'"

            if resolvedAttributes.contains(where: { $0.attribute == "ref" }) {
                throw CompilerError("Cannot use both id and ref attribute on the same element")
            }

            resolvedAttributes.append(ValdiJSAttribute(attribute: "ref", value: "this.elementRefs['\(element.id)']", valueIsExpr: true, evaluateOnce: false, isViewModelField: false, debugInfo: nil))
        }

        if cssModulePath != nil {
            staticProperties["cssDocument"] = "__cssDocument"
        }

        var componentClassToSet: String?
        var componentRefToSet: String?
        var componentPathToSet: String?
        var onCreateCallback: String?
        var onDestroyCallback: String?

        var lines = [String]()
        var elementLines = [String]()

        for attribute in resolvedAttributes {
            if attribute.attribute == "componentHandler" {
                throw CompilerError("componentHandler was removed, please use the componentClass attribute instead and provide the constructor for the component directly")
            }

            if attribute.isViewModelField {
                let setAttributeStatement = "setViewModelProperty('\(attribute.attribute)', \(attribute.value))"
                let evalOnceCheck = "hasViewModelProperty('\(attribute.attribute)')"

                elementLines.append(makeSetAttributeStatement(evalOnceCheck: evalOnceCheck, setAttributeStatement: setAttributeStatement, evaluateOnce: attribute.evaluateOnce))
            } else if !attribute.valueIsExpr {
                if attribute.attribute == "onCreate" {
                    onCreateCallback = attribute.value
                    continue
                }
                if attribute.attribute == "onDestroy" {
                    onDestroyCallback = attribute.value
                    continue
                }

                let node = Valdi_NodeAttribute(logger: logger, name: attribute.attribute, value: attribute.value)

                let exprRepresentation: String
                switch node.type {
                case .nodeAttributeTypeDouble:
                    exprRepresentation = "\(node.doubleValue)"
                case .nodeAttributeTypeInt:
                    exprRepresentation = "\(node.intValue)"
                case .nodeAttributeTypeString:
                    exprRepresentation = "'\(attribute.value.escapingQuotesAndNewlines)'"
                case .UNRECOGNIZED:
                    continue
                }

                if attribute.attribute == "context" {
                    elementLines.append("""
                                          if (!renderer.hasContext()) {
                                              renderer.setContext(\(exprRepresentation));
                                          }
                                          """)
                } else {
                    staticProperties[attribute.attribute] = exprRepresentation
                }
            } else if element.slotName != nil {
                staticProperties[attribute.attribute] = attribute.value
            } else {
                if attribute.attribute == "componentClass" {
                    componentClassToSet = attribute.value
                } else if attribute.attribute == "componentRef" {
                    componentRefToSet = attribute.value
                } else if attribute.attribute == "componentPath" {
                    componentPathToSet = attribute.value
                } else if attribute.attribute == "onCreate" {
                    onCreateCallback = attribute.value
                } else if attribute.attribute == "onDestroy" {
                    onDestroyCallback = attribute.value
                } else {
                    appendDebugLine(context: "attribute '\(attribute.attribute)'", attribute: attribute, lines: &elementLines)

                    if attribute.attribute == "viewModel" {
                        if attribute.evaluateOnce {
                            throw CompilerError("once: is not supported on viewModel attribute")
                        }
                        elementLines.append("renderer.setViewModelProperties(\(attribute.value));")
                    } else if attribute.attribute == "context" {
                        elementLines.append("""
                        if (!renderer.hasContext()) {
                            renderer.setContext(\(attribute.value));
                        }
                        """)
                    } else {
                        let evalOnceCheck: String
                        let setAttributeStatement: String

                        if element.componentPath != nil && parent != nil {
                            evalOnceCheck = "hasInjectedAttribute('$\(attribute.attribute)')"
                            setAttributeStatement = "setInjectedAttribute('$\(attribute.attribute)', \(attribute.value))"
                        } else {
                            evalOnceCheck = "hasAttribute('\(attribute.attribute)')"
                            setAttributeStatement = "setAttribute('\(attribute.attribute)', \(attribute.value))"
                        }

                        elementLines.append(makeSetAttributeStatement(evalOnceCheck: evalOnceCheck, setAttributeStatement: setAttributeStatement, evaluateOnce: attribute.evaluateOnce))
                    }
                }
            }
        }

        let isComponent = element.componentPath != nil || componentClassToSet != nil || componentPathToSet != nil

        let startRenderLine: String
        let endRenderLine: String

        if let slotName = element.slotName {
            if let refExpr = element.slotRefExpr {
                startRenderLine = "renderer.renderNamedSlot('\(slotName)', this, \(refExpr));"
            } else {
                startRenderLine = "renderer.renderNamedSlot('\(slotName)', this);"
            }
            endRenderLine = ""
        } else if isComponent, parent != nil {
            let variableName: String
            if let componentPath = element.componentPath {
                variableName = try importComponent(componentPath: componentPath)
            } else if let componentPathToSet = componentPathToSet {
                variableName = componentPathToSet
            } else {
                variableName = componentClassToSet!
            }

            for (attributeName, attributeValue) in staticProperties.sorted(by: { $0 < $1 }) {
                elementLines.append("renderer.setInjectedAttribute('$\(attributeName)', \(attributeValue));")
            }
            staticProperties.removeAll()

            var beginComponentParameters = [variableName, key, componentRefToSet]
            let beginComponentFunction: String
            if componentPathToSet != nil {
                beginComponentParameters = ["require"] + beginComponentParameters
                beginComponentFunction = "beginComponentFromPath"
            } else {
                beginComponentFunction = "beginComponentWithoutPrototype"
            }
            let beginComponentParametersStr = beginComponentParameters.map { $0 ?? "undefined" }.joined(separator: ", ")
            startRenderLine = "renderer.\(beginComponentFunction)(\(beginComponentParametersStr));"

            if let onCreateCallback = onCreateCallback {
                elementLines.append("renderer.setComponentOnCreateObserver(\(onCreateCallback));")
            }
            if let onDestroyCallback = onDestroyCallback {
                elementLines.append("renderer.setComponentOnDestroyObserver(\(onDestroyCallback));")
            }

            endRenderLine = "renderer.endComponent();"
        } else {
            let nodePrototypeVariableName = "__node\(nameAllocator.allocate(property: element.nodeType))"

             if let key = key {
                 startRenderLine = "renderer.beginRender(\(nodePrototypeVariableName), \(key));"
             } else {
                 startRenderLine = "renderer.beginRender(\(nodePrototypeVariableName));"
             }
             endRenderLine = "renderer.endRender();"

            let nodeType: String
            if let jsxName = element.jsxName {
                nodeType = jsxName
            } else {
                nodeType = "custom-view"
                if let androidViewClass = element.androidViewClass {
                    staticProperties["androidClass"] = "'\(androidViewClass)'"
                }
                if let iosViewClass = element.iosViewClass {
                    staticProperties["iosClass"] = "'\(iosViewClass)'"
                }
                if let webViewClass = element.webViewClass {
                    staticProperties["webClass"] = "'\(webViewClass)'"
                }
                if let macosViewClass = element.macosViewClass {
                    staticProperties["macosClass"] = "'\(macosViewClass)'"
                }
            }

            let staticPropertiesContent = toPropertyList(dict: staticProperties)

            nodePrototypesSection.appendBody("const \(nodePrototypeVariableName) = renderer.makeNodePrototype('\(nodeType)', \(staticPropertiesContent));\n")
        }

        var shouldRenderExprs = [String]()

        // Get the previous expression in the if-else chain of elements, if there is one.
        var renderConditional = element.renderIfExpr
        if let renderElseIfExpr = element.renderElseIfExpr {
            let dependentRenderIfResults = resolvePreviousSiblingRenderIfResults(element: element, parent: parent)
            for dependentRenderIfResult in dependentRenderIfResults {
                shouldRenderExprs.append("!\(dependentRenderIfResult)")
            }

            renderConditional = renderElseIfExpr
        }

        // Construct the custom render-if expression for this element if applicable.
        if let renderIf = renderConditional {
            let renderIfExpr = "!(!(\(renderIf.value)))"
            let shouldRenderExpr = renderIfExpr

            appendDebugLine(context: "renderIf", attribute: renderIf, lines: &lines)

            shouldRenderExprs.append(shouldRenderExpr)
        }

        let childrenElementLines = try element.children.flatMap { try generateRenderTemplateBody(node: $0, parent: element) }
        let hasRenderIf = !shouldRenderExprs.isEmpty

        if hasRenderIf {
            let renderIfResultVar = TypeScriptGenerator.makePrivateVar(named: TypeScriptGenerator.renderIfResultName, forId: element.id)
            lines.append("const \(renderIfResultVar) = \(shouldRenderExprs.joined(separator: " && "));")

            lines.append("if (\(renderIfResultVar)) {")
            lines.append(startRenderLine)
            lines += elementLines
            lines += childrenElementLines
            lines.append(endRenderLine)
            lines.append("}")
        } else {
            lines.append(startRenderLine)
            lines += elementLines
            lines += childrenElementLines
            lines.append(endRenderLine)
        }

        return lines
    }

    private func resolveForEachExpr(forEachExpr: String) -> String {
        if forEachExpr.hasPrefix("const ") || forEachExpr.hasPrefix("let ") {
            return forEachExpr
        }
        return "const \(forEachExpr)"
    }

    private func generateRenderTemplateBody(node: ValdiJSNode, parent: ValdiJSElement?) throws -> [String] {
        switch node {
        case let .expression(expression):
            return try generateRenderTemplateBody(expression: expression, parent: parent)
        case let .element(element):
            return try generateRenderTemplateBody(element: element, parent: parent)
        }
    }

    private func generateRenderTemplateBody(expression: ValdiJSExpression, parent: ValdiJSElement?) throws -> [String] {
        return [ expression.expression ]
    }

    private func generateRenderTemplateBody(element: ValdiJSElement, parent: ValdiJSElement?) throws -> [String] {
        var lines = [String]()

        if let forEachExpr = element.forEachExpr {
            let indexVar = TypeScriptGenerator.makePrivateVar(named: "index", forId: element.id)

            lines.append("""
                let \(indexVar) = 0;
                """)

            appendDebugLine(context: "forEach", attribute: forEachExpr, lines: &lines)

            var keyLines = [String]()
            let forEachKeyExpr: String
            if let forEachKey = element.forEachKey {
                forEachKeyExpr = forEachKey.value
                appendDebugLine(context: "forEach key", attribute: forEachKey, lines: &keyLines)
            } else {
                forEachKeyExpr = indexVar
            }

            let keyName = nameAllocator.allocate(property: "__key")

            let keySelector = "const \(keyName) = (\(forEachKeyExpr)).toString();"

            let elementLines = try generateRenderTemplateInnerBody(element: element, parent: parent, key: keyName.name)

            keyLines.append(keySelector)

            let resolvedForEachExpr = resolveForEachExpr(forEachExpr: forEachExpr.value)
            lines.append("""
                for (\(resolvedForEachExpr)) {
                    \(keyLines.joined(separator: "\n"))
                    \(elementLines.joined(separator: "\n"))
                    \(indexVar) += 1;
                }
                """)
        } else {
            if let key = element.forEachKey {
                lines += try generateRenderTemplateInnerBody(element: element, parent: parent, key: key.value)
            } else {
                lines += try generateRenderTemplateInnerBody(element: element, parent: parent, key: nil)
            }
        }

        if let slotName = element.insertInSlotName {
            lines.insert("renderer.setNamedSlot('\(slotName)', () => {", at: 0)
            lines.append("});")
        }

        return lines
    }

    private func generateRenderTemplate(functionName: String, className: String) throws -> String {
        let renderTemplateBody = try generateRenderTemplateBody(element: elements, parent: nil).joined(separator: "\n")

        return """
        function \(functionName)(this: \(className)) {
        const viewModel = this.viewModel;
        const state = this.state;

        \(renderTemplateBody)
        }
        """
    }

    private func extractAllUserProvidedIds(element: ValdiJSElement, out: inout Set<String>) {
        if !element.id.hasPrefix("!") {
            out.insert(element.id)
        }
        element.children.compactMap { $0.element }.forEach { extractAllUserProvidedIds(element: $0, out: &out) }
    }

    private func generateElementInitializer(element: ValdiJSElement) -> String {
        var allIds: Set<String> = []

        extractAllUserProvidedIds(element: element, out: &allIds)

        let ids = allIds.map { "'\($0)'" }.sorted()
        return "[\(ids.joined(separator: ", "))]"
    }

    private func generateComponentSetup(className: String, renderTemplateArgument: String) throws -> String {
        let out = CodeWriter()

        let resolvedClassName: String
        if className == JSConstants.autoImportedComponentIdentifier {
            resolvedClassName = makeClassName(fileName: self.elements.componentPath?.fileName ?? "Function", suffix: "Component")
            out.appendBody("class \(resolvedClassName) extends \(JSConstants.autoImportedComponentIdentifier) {}\n")
        } else {
            resolvedClassName = className
        }

        guard let rootPath = self.elements.componentPath else {
            throw CompilerError("Missing componentPath on root element")
        }

        let elementByIdBody = generateElementInitializer(element: elements)

        out.appendBody("""
        export const \(rootPath.exportedMember) = \(resolvedClassName);

        const elements: ElementsSpecs = \(elementByIdBody);
        LegacyVueComponent.setup(\(rootPath.exportedMember), elements, \(renderTemplateArgument));
        """)

        return out.content
    }

    private func generateRequires() -> String {
        // TODO(3521): Update to valdi_core
        var baseRequires = """
        import { LegacyVueComponent, ElementsSpecs } from 'valdi_core/src/Valdi';
        import * as JSX from 'valdi_core/src/JSX';
        import { PropertyList } from 'valdi_core/src/utils/PropertyList';
        import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';
        declare const runtime: ValdiRuntime;

        const renderer = JSX.jsx;
        """

        if !symbolsToImport.isEmpty {
            let symbolsByDefault = self.symbolsToImport.groupBy { (symbol) -> Bool in
                return symbol.isDefault
            }

            let defaultImport = symbolsByDefault[true]?.map { $0.symbol }.joined(separator: ", ")
            let regularImport = symbolsByDefault[false]?.map { $0.symbol }.joined(separator: ", ")

            let allImports = [defaultImport, regularImport.map { "{ \($0) }" }].compactMap { $0 }.joined(separator: ", ")

            baseRequires += "\nimport \(allImports) from './\(sourceURL.lastPathComponent)';"
        }

        return baseRequires
    }

    func generate() throws -> TypeScriptResult? {
        let viewClassName = self.customComponentClass ?? JSConstants.autoImportedComponentIdentifier

        if let cssModulePath = self.cssModulePath {
            importSection.appendBody("import { getNativeModule as __getNativeModule } from 'valdi_core/src/CSSModule';\n")
            nodePrototypesSection.appendBody("const __cssDocument = __getNativeModule('\(cssModulePath)');\n")
        }
        importSection.appendBody("import { RequireFunc } from 'valdi_core/src/ModuleLoader';\n")
        importSection.appendBody("declare const require: RequireFunc;\n")

        let renderTemplateFunctionName = "renderTemplate"
        let renderTemplateFunction = try generateRenderTemplate(functionName: renderTemplateFunctionName, className: viewClassName)

        if renderTemplateFunction.isEmpty && !hasUserScript {
            return nil
        }

        let requires = generateRequires()

        let setupBody = try generateComponentSetup(className: viewClassName, renderTemplateArgument: renderTemplateFunctionName)

        let generatedScript = ([requires, importSection.content, nodePrototypesSection.content, renderTemplateFunction, setupBody] ).joined(separator: "\n").indented

        return TypeScriptResult(typeScript: generatedScript)
    }

    class func objectFunctionName(fromActionFunctionName actionFunctionName: String) -> String {
        return String(actionFunctionName.split(separator: "_").last!)
    }

    static let renderIfResultName = "renderIfResult"

    class func makePrivateVar(named name: String, forId id: String) -> String {
        return "__\(name)\(id.pascalCased)"
    }
}
