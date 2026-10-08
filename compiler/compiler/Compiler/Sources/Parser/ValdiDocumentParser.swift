//
//  ValdiDocumentParser.swift
//  Compiler
//
//  Created by Simon Corsin on 4/5/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation
import SwiftSoup

private extension Substring {

    var lineCount: Int {
        let newlines = CharacterSet.newlines
        var count = 0
        var range = startIndex..<endIndex
        while range.lowerBound != endIndex {
            if let foundRange = rangeOfCharacter(from: newlines, options: [], range: range) {
                count += 1
                range = foundRange.upperBound..<endIndex
            } else {
                break
            }
        }

        return count
    }

}

private extension Substring {

    var escapingQuotes: String {
        return replacingOccurrences(of: "'", with: "\\'").replacingOccurrences(of: "\"", with: "\\\"")
    }

}

private extension String {

    var extractingExpression: String? {
        if isEmpty {
            return nil
        }

        var currentIndex = startIndex
        let endIndex = self.endIndex

        var expressions = [String]()

        while currentIndex != endIndex {
            if let from = range(of: "{{", options: [], range: currentIndex..<endIndex, locale: nil),
                let to = range(of: "}}", options: [], range: from.upperBound..<endIndex, locale: nil) {

                if from.lowerBound > currentIndex {
                    expressions.append("'\(self[currentIndex..<from.lowerBound].escapingQuotes)'")
                }
                var extractedExpr = String(self[from.upperBound..<to.lowerBound]).trimmed
                if extractedExpr.last == ";" {
                    // Backward compat for attributes that had ; at the end
                    extractedExpr = extractedExpr.removingLastCharacter
                }
                expressions.append(extractedExpr)

                currentIndex = to.upperBound
            } else {
                if currentIndex == startIndex {
                    // We didn't find any expressions
                    return nil
                }

                expressions.append("'\(self[currentIndex..<endIndex].escapingQuotes)'")

                currentIndex = endIndex
            }
        }

        if expressions.count > 1 {
            return expressions.map { "( \($0) )" }.joined(separator: " + ")
        } else {
            return expressions[0]
        }
    }

}

class ValdiDocumentParser {
    private static let xmlAttributeValuePattern = "([a-zA-Z0-9-_:]+)=\"((?:.|\\s)*?)\""

    // Workaround to fix a crash in SwiftSoup. This library uses a static mutable variable
    // that is not behind a lock and gets lazily populated when the escape mode fields are created.
    // This ensures that the initialization of those fields is done ahead of time, inside a lock.
    private static let ensureSwiftSoupIsInitialized: Bool = {
        _ = [
            SwiftSoup.Entities.EscapeMode.xhtml,
            SwiftSoup.Entities.EscapeMode.base,
            SwiftSoup.Entities.EscapeMode.extended
        ]

        return true
    }()

    static let openTagRegex = try! NSRegularExpression(pattern: "<\\s*([a-zA-Z0-9-_]+)[^>]*>")
    static let viewModelRegex = try! NSRegularExpression(pattern: "\\s:\(ValdiDocumentParser.xmlAttributeValuePattern)")
    static let onViewModelRegex = try! NSRegularExpression(pattern: "\\s:@\(ValdiDocumentParser.xmlAttributeValuePattern)")
    static let viewModelOnceRegex = try! NSRegularExpression(pattern: "\\s:once:\(ValdiDocumentParser.xmlAttributeValuePattern)")
    static let onRegex = try! NSRegularExpression(pattern: "\\s@\(ValdiDocumentParser.xmlAttributeValuePattern)")

    var document = ValdiRawDocument()

    private let logger: ILogger
    private let content: String
    private let parser: SwiftSoup.Parser
    private let iosImportPrefix: String

    private init(logger: ILogger, content: String, iosImportPrefix: String) {
        self.logger = logger
        self.content = content
        self.document.content = content
        self.iosImportPrefix = iosImportPrefix

        _ = ValdiDocumentParser.ensureSwiftSoupIsInitialized
        let parser = SwiftSoup.Parser.xmlParser()
        parser.settings(.preserveCase)
        parser.setTrackErrors(16)
        self.parser = parser
    }

    func parse() throws {
        var deferredProcesses = [() throws -> Void]()

        // Extract tags from content
        var searchIndex = self.content.startIndex
        while searchIndex < self.content.endIndex {
            var matches = ValdiDocumentParser.openTagRegex.matches(in: self.content, options: [], range: NSRange(searchIndex..., in: self.content))
            guard !matches.isEmpty else {
                break
            }

            guard let startTagRange = Range(matches[0].range, in: self.content) else {
                break
            }
            let sectionStart = startTagRange.lowerBound
            let contentStart = startTagRange.upperBound
            let startTag = self.content[startTagRange]
            let lineOffset = self.content[..<sectionStart].lineCount

            let tagName = self.content[Range(matches[0].range(at: 1), in: self.content)!]

            let closeTagPattern = "</\\s*\(tagName)\\s*\\s*>"
            let closeTagRegex = try NSRegularExpression(pattern: closeTagPattern)
            matches = closeTagRegex.matches(in: self.content, options: [], range: NSRange(contentStart..., in: self.content))
            if matches.count == 0 {
                throw CompilerError(type: "Parse error",
                                    message: "Missing close tag for \(tagName).",
                                    atZeroIndexedLine: lineOffset,
                                    column: nil,
                                    inDocument: self.content)
            }

            guard let endTagRange = Range(matches[0].range, in: self.content) else {
                break
            }
            let contentEnd = endTagRange.lowerBound
            let sectionEnd = endTagRange.upperBound
            let endTag = self.content[endTagRange]

            let outerContent = self.content[sectionStart..<sectionEnd]
            let innerContent = self.content[contentStart..<contentEnd]

            switch tagName {
            case "actions":
                try process(actionsElement: parseSection(content: outerContent, tagName: tagName, lineOffset: lineOffset))
            case "class-mapping":
                try process(classMappingElement: parseSection(content: outerContent, tagName: tagName, lineOffset: lineOffset))
            case "script":
                try process(scriptElementStartTag: startTag, content: innerContent, scriptElementEndTag: endTag, lineOffset: lineOffset)
            case "style":
                // Sass styles can use special characters such as "&" and ">" that need to be escaped
                let xmlEscapedOuterContent = String(outerContent[sectionStart..<contentStart]) +
                    String(outerContent[contentStart..<contentEnd]).xmlEscaped +
                    String(outerContent[contentEnd..<sectionEnd])
                let styleElement = try parseSection(content: Substring(xmlEscapedOuterContent), tagName: tagName, lineOffset: lineOffset)
                try process(styleElement: styleElement)
            case "template":
                deferredProcesses.append {
                    let lang = self.document.scripts.compactMap { $0.lang }.first
                    var expandedContent = self.expandOnAttributes(content: String(innerContent), lang: lang)
                    expandedContent = self.expandViewModelAttributes(content: expandedContent)

                    try self.process(templateElementStartTag: startTag, content: Substring(expandedContent), templateElementEndTag: endTag, lineOffset: lineOffset)
                }
            default:
                throw CompilerError(type: "Parse error",
                                    message: "Unsupported valdi section '\(tagName)'. Supported sections are style, template, script, view-model, class-mapping, and actions.",
                                    atZeroIndexedLine: lineOffset,
                                    column: nil,
                                    inDocument: self.content)
            }
            searchIndex = sectionEnd
        }

        try deferredProcesses.forEach { try $0() }
    }

    private func parseSection(content: Substring, tagName: Substring, lineOffset: Int) throws -> SwiftSoup.Element {
        do {
            let xmlDoc = try self.parser.parseInput(String(content), "")
            guard xmlDoc.children().count == 1 else {
                throw CompilerError("There should be exactly one root element in section '\(tagName)'")
            }
            let element = xmlDoc.child(0)
            return element
        } catch {
            throw CompilerError(type: "XML parse error for section '\(tagName)'",
            message: error.legibleLocalizedDescription,
            atZeroIndexedLine: 0 + lineOffset, // FIXME: error.line - 1 + lineOffset,
            column: nil,
            inDocument: self.content)
        }
    }

    private func process(styleElement: SwiftSoup.Element) throws {
        if styleElement.children().count > 0 {
            throw CompilerError("Style section cannot have children")
        }

        var style = ValdiStyleSheet()

        let allAttributes = styleElement.getAttributes()?.asList() ?? []
        for attribute in allAttributes {
            switch attribute.getKey() {
            case "src":
                style.source = attribute.getValue()
            case "lang":
                // Ignore
                continue
            default:
                throw CompilerError("Unsupported attribute '\(attribute.getKey())' in style. Only 'src' is supported.")
            }
        }

        style.content = styleElement.textNodes().first?.getWholeText().trimmed ?? ""
        document.styles.append(style)
    }

    private func xmlEscape(s: Substring) -> String {
        return String(s).xmlEscaped
    }

    private func process(scriptElementStartTag: Substring, content: Substring, scriptElementEndTag: Substring, lineOffset: Int) throws {
        // Escape the contents first
        let escapedContents = "\(scriptElementStartTag)\(xmlEscape(s: content))\(scriptElementEndTag)"

        let parsedScriptSection = try self.parser.parseInput(escapedContents, "")
        guard parsedScriptSection.children().count == 1 else {
            throw CompilerError("The 'script' section should have exactly one element")
        }
        let scriptElement = parsedScriptSection.child(0)

        var script = ValdiScript()

        let allAttributes = scriptElement.getAttributes()?.asList() ?? []
        for attribute in allAttributes {
            switch attribute.getKey() {
            case "src":
                script.source = attribute.getValue().trimmed
            case "lang":
                script.lang = attribute.getValue().trimmed
            default:
                throw CompilerError("Unsupported attribute '\(attribute.getKey())' in script. Only 'src' is supported.")
            }
        }

        let scriptContent = scriptElement.textNodes().first?.getWholeText().trimmed ?? ""

        if !scriptContent.isEmpty {
            script.content = scriptContent
        }
        script.lineOffset = lineOffset
        document.scripts.append(script)
    }

    private func expandOnAttributes(content: String, lang: String?) -> String {
        let replacement: String

        if lang == "kt" {
            replacement = "{ params: dynamic -> this.$2(params) }"
        } else {
            replacement = "this.$2.bind(this)"
        }

        // TODO: should be able to avoid requiring the namespace/prefix with SwiftSoup
        let result = ValdiDocumentParser.onViewModelRegex.stringByReplacingMatches(in: content, options: [], range: content.nsrange, withTemplate: " view-model-once:on-$1=\"{{ \(replacement) }}\"")

        return ValdiDocumentParser.onRegex.stringByReplacingMatches(in: result, options: [], range: result.nsrange, withTemplate: " once:on-$1=\"{{ \(replacement) }}\"")
    }

    // TODO: should be able to avoid requiring the namespace/prefix with SwiftSoup
    private func expandViewModelAttributes(content: String) -> String {
        let result = ValdiDocumentParser.viewModelOnceRegex.stringByReplacingMatches(in: content, options: [], range: content.nsrange, withTemplate: " view-model-once:$1=\"{{ $2 }}\"")

        return ValdiDocumentParser.viewModelRegex.stringByReplacingMatches(in: result, options: [], range: result.nsrange, withTemplate: " view-model:$1=\"{{ $2 }}\"")
    }

    private func process(templateElementStartTag: Substring, content: Substring, templateElementEndTag: Substring, lineOffset: Int) throws {
        // Escape the contents of the mustached template sections first
        var escaped = ""
        var startIndex = content.startIndex
        while startIndex < content.endIndex {
            guard let start = content.range(of: "{{", range: startIndex..<content.endIndex) else {
                escaped += content[startIndex...]
                break
            }

            guard let end = content.range(of: "}}", range: start.upperBound..<content.endIndex) else {
                throw CompilerError(type: "XML parse error for section 'template'",
                    message: "Unmatched template {{",
                    atZeroIndexedLine: content[..<start.lowerBound].lineCount + lineOffset,
                    column: nil,
                    inDocument: self.content)
            }

            let templateEscaped = xmlEscape(s: content[start.upperBound..<end.lowerBound])
            escaped += "\(content[startIndex..<start.lowerBound]){{ \(templateEscaped) }}"

            startIndex = end.upperBound
        }

        let adjustedXML = "\(templateElementStartTag)\(escaped)\(templateElementEndTag)"
        let parsedDoc = try self.parser.parseInput(adjustedXML, "")
        guard parsedDoc.children().count == 1 else {
            throw CompilerError("The 'template' section should have exactly one root element")
        }
        let rootElement = parsedDoc.child(0)

        let parserDelegate = TemplateParserDelegate()

        func visit(_ element: SwiftSoup.Element) throws {
            let tagName = element.tagName()
            // Keep source order: same-name attributes (e.g. `class` and `:class`) tie in the later
            // name sort, and a Dictionary's per-process hash order made the output nondeterministic.
            let attributes = element.getAttributes()?.map { ($0.getKey(), $0.getValue()) } ?? []
            parserDelegate.parser(self.parser, didStartElement: tagName, attributes: attributes)
            if parserDelegate.error != nil {
                return
            }

            for node in element.getChildNodes() {
                if let textNode = node as? TextNode {
                    if let text = textNode.getWholeText().trimmed.nonEmpty {
                        parserDelegate.parser(parser, didEncounterTextNode: text)
                    }
                } else if let elementNode = node as? Element {
                    try visit(elementNode)
                    if parserDelegate.error != nil {
                        return
                    }
                } else if node is Comment {
                    // we don't currently do anything for comments
                } else {
                    throw CompilerError("Unknown node type: \(node)")
                }
            }

            parserDelegate.parser(self.parser, didEndElement: tagName)
            if parserDelegate.error != nil {
                return
            }
        }

        try visit(rootElement)

        if let parserError = parserDelegate.error {
            // FIXME: line/col
            let parserErrorMessage: String? = nil
            throw CompilerError(type: "XML parse error for section 'template'",
                                message: parserErrorMessage ?? parserError.legibleLocalizedDescription,
                                atZeroIndexedLine: lineOffset, // (parserErrorLineNumber ?? parser.lineNumber) - 1 + lineOffset,
                                column: nil, // parserErrorColumnNumber ?? parser.columnNumber,
                                inDocument: self.content)
        }

        var template = ValdiRawTemplate()

        guard let templateRoot = parserDelegate.finish() else {
            throw CompilerError(type: "XML parse error for section 'template'",
                                message: "No template root element",
                atZeroIndexedLine: lineOffset - 1,
                column: nil,
                inDocument: self.content)
        }

        for attr in templateRoot.customAttributes {
            switch attr.name {
            case "jsViewClass":
                logger.warn("js-view-class is deprecated, use js-component-class instead")
                template.jsComponentClass = attr.value
            case "jsComponentClass":
                template.jsComponentClass = attr.value
            default:
                if attr.name.hasPrefix("xmlns:") {
                    continue
                }
                throw CompilerError(type: "XML parse error for section 'template'",
                                    message: "Unrecognized attribute '\(attr.name)' in 'template'",
                                    atZeroIndexedLine: lineOffset,
                                    column: nil,
                                    inDocument: self.content)
            }
        }

        guard !templateRoot.children.isEmpty else {
            throw CompilerError(type: "XML parse error for section 'template'",
                                message: "Template is empty",
                                atZeroIndexedLine: lineOffset,
                                column: nil,
                                inDocument: self.content)
        }

        template.rootNode = templateRoot.children.first?.node

        document.template = template
    }

    private func process(actionsElement: SwiftSoup.Element) throws {
        for actionElement in actionsElement.children() {
            let type: ValdiActionType

            switch actionElement.tagName().lowercased() {
            case "native":
                type = .native
            case "js", "javascript":
                type = .javaScript
            default:
                throw CompilerError("Unrecognized action type '\(actionElement.tagName())'. Only 'native' and 'javascript' are supported")
            }

            let attributes = actionElement.getAttributes()?.asList() ?? []
            for attribute in attributes {
                let attributeName = attribute.getKey().camelCased
                switch attributeName {
                case "name":
                    document.actions.append(ValdiAction(name: attribute.getValue().trimmed, type: type))
                default:
                    throw CompilerError("Unrecognized property '\(attribute.getKey())' in actions section. Only 'name' is supported.")
                }
            }
        }
    }

    private func process(classMappingElement: SwiftSoup.Element) throws {
        let classMapping = try ValdiDocumentParser.parseClassMapping(classMappingElement: classMappingElement, iosImportPrefix: iosImportPrefix)
        document.classMapping = classMapping
    }

    class func parseClassMapping(classMappingElement: SwiftSoup.Element, iosImportPrefix: String) throws -> ValdiClassMapping {
        var classMapping = ValdiClassMapping()

        for child in classMappingElement.children() {
            var nodeMapping = ValdiNodeClassMapping(
                tsType: "",
                iosType: nil,
                androidClassName: nil,
                cppType: nil,
                kind: .class)

            var localIosImportPrefix: String?

            let attributes = child.getAttributes()?.asList() ?? []
            for attribute in attributes {
                switch attribute.getKey() {
                case "android":
                    nodeMapping.androidClassName = attribute.getValue().trimmed
                case "ios":
                    nodeMapping.iosType = IOSType(name: attribute.getValue().trimmed, bundleInfo: nil, kind: .class, iosLanguage: .objc)
                case "iosImportPrefix":
                    localIosImportPrefix = attribute.getValue().trimmed
                default:
                    throw CompilerError("Unrecognized attribute '\(attribute.getKey())' in class mapping")
                }
            }
            nodeMapping.iosType?.applyImportPrefix(iosImportPrefix: localIosImportPrefix ?? iosImportPrefix, isOverridden: false)

            classMapping.nodeMappingByClass[child.tagName()] = nodeMapping
        }

        return classMapping
    }

    class func parse(logger: ILogger, content: String, iosImportPrefix: String) throws -> ValdiRawDocument {
        let parser = ValdiDocumentParser(logger: logger, content: content, iosImportPrefix: iosImportPrefix)
        try parser.parse()
        return parser.document
    }

    class TemplateParserDelegate {
        static let rootNodeType = "__ROOT__"

        class ParseNode {
            enum Child {
                case expression(String)
                case node(ParseNode)

                var node: ParseNode? {
                    guard case let .node(node) = self else {
                        return nil
                    }
                    return node
                }
            }
            var rawNode: ValdiRawNode
            var children: [Child] = []

            var childrenNodes: [ParseNode] {
                return children.compactMap(\.node)
            }

            init(rawNode: ValdiRawNode) {
                self.rawNode = rawNode
            }
        }

        private var stack: [ParseNode] = []
        private var finished = false

        var error: Error?

        init() {
            var rootRawNode = ValdiRawNode()
            rootRawNode.nodeType = TemplateParserDelegate.rootNodeType

            let root = ParseNode(rawNode: rootRawNode)
            stack.append(root)
        }

        func finish() -> ValdiRawNode? {
            guard !finished else {
                fatalError("finish() called twice")
            }

            guard error == nil else {
                return nil
            }

            guard let root = stack.last else {
                return nil
            }

            // Walk the parse tree to fill the raw nodes
            fillRawNodeChildren(parseNode: root)

            self.finished = true

            return root.rawNode.children.first?.node
        }

        private static let viewModelNamespace = "viewModel:"
        private static let viewModelOnceNamespace = "viewModelOnce:"
        private static let evaluateOnceNamespace = "once:"

        private func parsePrefix(name: inout String, prefix: String) -> Bool {
            if name.hasPrefix(prefix) {
                name = String(name[prefix.endIndex...])
                return true
            }
            return false
        }

        private func makeNodeAttribute(attributeName: String, trimmedText: String) -> ValdiRawNodeAttribute {
            var attr = ValdiRawNodeAttribute()
            attr.name = attributeName
            attr.value = trimmedText

            if parsePrefix(name: &attr.name, prefix: TemplateParserDelegate.viewModelNamespace) {
                attr.isViewModelField = true
                attr.valueIsExpr = true
            } else if parsePrefix(name: &attr.name, prefix: TemplateParserDelegate.viewModelOnceNamespace) {
                attr.isViewModelField = true
                attr.evaluateOnce = true
            } else if parsePrefix(name: &attr.name, prefix: TemplateParserDelegate.evaluateOnceNamespace) {
                attr.evaluateOnce = true
            }

            if let expression = attr.value.extractingExpression {
                attr.value = expression
                attr.valueIsExpr = true
            }

            return attr
        }

        private func fillRawNodeChildren(parseNode: ParseNode) {
            for parseChild in parseNode.children {
                let rawChild: ValdiRawNode.Child

                switch parseChild {
                case let .expression(expression):
                    rawChild = .expression(expression)
                case let .node(parseChildNode):
                    fillRawNodeChildren(parseNode: parseChildNode)
                    rawChild = .node(parseChildNode.rawNode)
                }

                parseNode.rawNode.children.append(rawChild)
            }
        }

        private func error(parser: SwiftSoup.Parser, message: String) {
            error = CompilerError(message)
//            parser.abortParsing()
        }

        func parser(_ parser: SwiftSoup.Parser, didStartElement elementName: String, attributes attributeDict: [(String, String)] = []) {
            guard error == nil else {
                return
            }

            var node = ValdiRawNode()
            // FIXME: line/col numbers
//            node.lineNumber = parser.lineNumber - 1
//            if attributeDict.isEmpty {
//                node.columnNumber = max(parser.columnNumber - elementName.count, 0)
//            } else {
//                // TODO(simon): The columnNumber won't be correct if have attributes.
//                // We need to have some kind of way to get the line and column of
//                // when the element started as opposed to when the > has been found.
//                node.columnNumber = parser.columnNumber
//            }
            node.nodeType = elementName

            guard let parent = stack.last else {
                error(parser: parser, message: "Invalid parser state: no parent element")
                return
            }

            for (rawAttributeName, attributeText) in attributeDict {
                let nodeAttribute = makeNodeAttribute(attributeName: rawAttributeName.camelCased, trimmedText: attributeText.trimmed)

                switch nodeAttribute.name {
                case "forEach":
                    if !nodeAttribute.valueIsExpr {
                        error(parser: parser, message: "'for-each' must be an expression using {{ }}")
                        return
                    }
                    if nodeAttribute.evaluateOnce {
                        // We can't do eval once on for-each because we need the emitted local variable
                        // name
                        error(parser: parser, message: "'for-each' does not support `once` tag.")
                        return
                    }

                    node.forEachExpr = nodeAttribute
                case "id":
                    if nodeAttribute.valueIsExpr {
                        error(parser: parser, message: "Attribute 'id' cannot have a dynamic expression yet")
                        return
                    }
                    node.id = nodeAttribute.value
                    node.customAttributes.append(nodeAttribute)
                case "renderIf":
                    if !nodeAttribute.valueIsExpr {
                        error(parser: parser, message: "render-if must be an expression using {{ }}")
                        return
                    }
                    node.ifExpr = nodeAttribute
                case "renderElseIf":
                    if !nodeAttribute.valueIsExpr {
                        error(parser: parser, message: "render-else-if must be an expression using {{ }}")
                        return
                    }
                    let prevSibling = parent.childrenNodes.last
                    if prevSibling == nil || (prevSibling!.rawNode.ifExpr == nil && prevSibling!.rawNode.elseIfExpr == nil) {
                        error(parser: parser, message: "render-else-if must be sibling to a render-if or render-else-if")
                        return
                    }
                    node.elseIfExpr = nodeAttribute
                case "renderElse":
                    if nodeAttribute.value != "" {
                        error(parser: parser, message: "render-else must not have an expression")
                        return
                    }
                    let prevSibling = parent.childrenNodes.last
                    if prevSibling == nil || (prevSibling!.rawNode.ifExpr == nil && prevSibling!.rawNode.elseIfExpr == nil) {
                        error(parser: parser, message: "render-else must be sibling to a render-if or render-else-if")
                        return
                    }
                    node.elseExpr = nodeAttribute
                case "renderUnless":
                    if !nodeAttribute.valueIsExpr {
                        error(parser: parser, message: "render-unless must be an expression using {{ }}")
                        return
                    }
                    node.unlessExpr = nodeAttribute
                case "slot":
                    if nodeAttribute.valueIsExpr {
                        error(parser: parser, message: "'slot' cannot support dynamic expressions")
                        return
                    }
                    node.destSlotName = nodeAttribute.value
                case "isLayout":
                    if nodeAttribute.valueIsExpr {
                        error(parser: parser, message: "'is-layout' cannot support dynamic expressions")
                        return
                    }
                    guard let bool = Bool(nodeAttribute.value) else {
                        error(parser: parser, message: "'is-layout' should be a bool")
                        return
                    }
                    node.isLayout = bool
                default:
                    node.customAttributes.append(nodeAttribute)
                }
            }

            node.customAttributes.sort { (left, right) -> Bool in
                return left.name < right.name
            }

            let parseNode = ParseNode(rawNode: node)
            parent.children.append(.node(parseNode))
            stack.append(parseNode)
        }

        func parser(_ parser: SwiftSoup.Parser, didEncounterTextNode value: String) {
            guard error == nil else {
                return
            }

            guard let node = stack.last else {
                error(parser: parser, message: "Encountered text node without a currently-open tag. Shouldn't be possible")
                return
            }

            guard let value = value.trimmed.nonEmpty else {
                return
            }

            guard node.rawNode.customAttributes.filter({ $0.name == "value" }).isEmpty else {
                error(parser: parser, message: "Cannot have both an XML text and a 'value' attribute. Text values are automatically converted to a 'value' attribute")
                return
            }

            if ["Label", "Button", "TextField"].contains(node.rawNode.nodeType) {
                let attribute = makeNodeAttribute(attributeName: "value", trimmedText: value)
                node.rawNode.customAttributes.append(attribute)
            } else if let expression = value.extractingExpression {
                node.children.append(.expression(expression))
            }
        }

        func parser(_ parser: SwiftSoup.Parser, didEndElement elementName: String) {
            guard error == nil else {
                return
            }

            guard let node = stack.popLast() else {
                error(parser: parser, message: "Unmatched tag '\(elementName)'")
                return
            }

            if node.rawNode.nodeType != elementName {
                error(parser: parser, message: "Invalid end element '\(elementName)'")
                return
            }
        }
    }
}
