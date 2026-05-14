//
//  ValdiJSElement.swift
//  Compiler
//
//  Created by Simon Corsin on 12/12/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct ValdiJSAttributeDebugInfo {
    let originalExpression: String
    let line: Int
    let column: Int
}

struct ValdiJSAttribute {
    let attribute: String
    let value: String
    let valueIsExpr: Bool
    let evaluateOnce: Bool
    let isViewModelField: Bool
    let debugInfo: ValdiJSAttributeDebugInfo?
}

enum ValdiJSNode {
    case expression(ValdiJSExpression)
    case element(ValdiJSElement)

    var element: ValdiJSElement? {
        if case let .element(element) = self {
            return element
        }
        return nil
    }

    var expression: ValdiJSExpression? {
        if case let .expression(expression) = self {
            return expression
        }
        return nil
    }
}

struct ValdiJSExpression {
    let expression: String
}

class ValdiJSElement {

    let id: String
    let componentPath: ComponentPath?
    let nodeType: String
    let jsxName: String?

    var androidViewClass: String?
    var iosViewClass: String?
    var webViewClass: String?
    var macosViewClass: String?

    var forEachExpr: ValdiJSAttribute?
    var renderIfExpr: ValdiJSAttribute?
    var renderElseIfExpr: ValdiJSAttribute?
    var insertInSlotName: String?
    var slotName: String?
    var slotRefExpr: String?

    var children: [ValdiJSNode] = []
    var attributes: [ValdiJSAttribute] = []
    var forEachKey: ValdiJSAttribute?

    init(id: String, componentPath: ComponentPath?, nodeType: String, jsxName: String?) {
        self.id = id
        self.componentPath = componentPath
        self.nodeType = nodeType
        self.jsxName = jsxName
    }
}
