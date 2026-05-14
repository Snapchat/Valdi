import * as ts from 'typescript';
import { isExpressionNode, resolveTypeOfExpression, SymbolType, Transpiler } from './TSUtils';
import { mergeSourceMaps } from './SourceMapUtils';
import { ILogger } from './logger/ILogger';
import { createConsoleLogTransformer } from './ConsoleLogTransformer';

enum JSXAttributeValueKind {
  dynamic = 1,
  static = 2,
  spread = 3,
}

interface JSXAttributeValue {
  kind: JSXAttributeValueKind;
  value: ts.Expression;
}

interface JSXAttribute {
  node: ts.Node;
  name: string | undefined;
  value: JSXAttributeValue;
}

interface JSXElement {
  element: ts.JsxOpeningElement | ts.JsxSelfClosingElement;
  closingElement: ts.JsxClosingElement | undefined;
  nodeType: ts.JsxTagNameExpression;
  attributes: JSXAttribute[];
  children: ts.JsxChild[];
}

interface JSXOutputState {
  nodePrototypeDeclarations: ts.Statement[];
  idSequence: { [nodeType: string]: number };
  nodeDepth: number;
  typeChecker: ts.TypeChecker;
}

interface JSXAttributeStatements {
  slot?: JSXAttributeValue;
  key: ts.Expression | undefined;
  nodeRef: ts.Expression | undefined;
  componentContext: ts.Expression | undefined;
  lazy: boolean;
  staticAttributesLiteral?: ts.Expression;
  dynamicAttributes: JSXAttribute[];
  injectedAttributes: JSXAttribute[];
  allAttributes: JSXAttribute[];
}

interface Slotted {
  slotName: ts.Expression | undefined;
  expressions: ts.JsxChild[];
}

const enum JSXNodeKind {
  element = 1,
  component = 2,
  functionComponent = 3,
  slot = 4,
}

interface JSXNodeOutput {
  beginRenderStatement: ts.Statement;
  endRenderStatement: ts.Statement | undefined;
  statements: ts.Statement[];
}

// TODO(3521): update to valdi
const rendererModulePath = 'valdi_core/src/JSX';

function createStringLiteral(value: string, transformContext: ts.TransformationContext): ts.StringLiteral {
  const literal = transformContext.factory.createStringLiteral(value);
  (literal as any).singleQuote = true;
  return literal;
}

export class JSXProcessor {
  constructor(readonly logger?: ILogger) {}

  makeTransformers(typeChecker: ts.TypeChecker): ts.TransformerFactory<ts.SourceFile>[] {
    const state: JSXOutputState = { nodePrototypeDeclarations: [], idSequence: {}, nodeDepth: 0, typeChecker };

    const transforms: ts.TransformerFactory<ts.SourceFile>[] = [
      (context) => (node) => this.transformSourceFile(node, state, context),
      (context) => (node) => this.injectDeclarations(node, state, context),
    ];

    return transforms;
  }

  private isAlphaNum(char: number): boolean {
    if (char >= 48 && char <= 57) {
      return true;
    }

    if (char >= 65 && char <= 90) {
      return true;
    }

    if (char >= 97 && char <= 122) {
      return true;
    }

    return false;
  }

  private toPascalCase(key: string): string {
    for (let i = 0; i < key.length; i++) {
      const charCode = key.charCodeAt(i);

      if (!this.isAlphaNum(charCode)) {
        const prefix = key.substr(0, i);
        const suffix = key.substr(i + 1);
        key = prefix + suffix;
        i--;
      }
    }

    return key.substr(0, 1).toUpperCase() + key.substr(1);
  }

  private resolveJSXAttributeStatements(
    jsxNodeKind: JSXNodeKind,
    jsxElement: JSXElement,
    transformContext: ts.TransformationContext,
  ): JSXAttributeStatements {
    const nodeTypeStr = jsxElement.nodeType.getText();
    const attributes = jsxElement.attributes;

    let key: ts.Expression | undefined;
    let slot: JSXAttributeValue | undefined;
    const dynamicAttributes: JSXAttribute[] = [];
    const injectedAttributes: JSXAttribute[] = [];
    const allAttributes: JSXAttribute[] = [];
    const staticAttributes: ts.Expression[] = [];
    let foundSpread = false;
    let lazy = false;
    let nodeRef: ts.Expression | undefined;
    let componentContext: ts.Expression | undefined;

    for (const attribute of attributes) {
      if (attribute.name === 'slot') {
        if (nodeTypeStr != 'slotted') {
          this.onError(jsxElement.element, `'slot' attribute is only supported in <slotted> elements`);
        }
        slot = attribute.value;
        continue;
      }
      if (attribute.name === 'key') {
        key = attribute.value.value;
        continue;
      }

      if (attribute.name === 'ref') {
        nodeRef = attribute.value.value;
        continue;
      }

      if (attribute.name === 'context' && jsxNodeKind === JSXNodeKind.component) {
        componentContext = attribute.value.value;
        continue;
      }
      if (attribute.name === 'lazy') {
        lazy = true;
      }

      allAttributes.push(attribute);

      if (attribute.value.kind === JSXAttributeValueKind.spread) {
        foundSpread = true;
        dynamicAttributes.push(attribute);
      } else if (attribute.name?.[0] === '$' && jsxNodeKind === JSXNodeKind.component) {
        injectedAttributes.push(attribute);
      } else if (attribute.value.kind === JSXAttributeValueKind.static && !foundSpread) {
        staticAttributes.push(createStringLiteral(attribute.name!, transformContext));
        staticAttributes.push(attribute.value.value);
      } else {
        dynamicAttributes.push(attribute);
      }
    }

    let staticAttributesLiteral: ts.Expression | undefined;
    if (staticAttributes.length) {
      staticAttributesLiteral = transformContext.factory.createArrayLiteralExpression(staticAttributes);
    }

    return {
      dynamicAttributes,
      injectedAttributes,
      allAttributes,
      slot,
      lazy,
      key,
      nodeRef,
      componentContext,
      staticAttributesLiteral,
    };
  }

  private makeIdentifier(prefix: string, type: string, output: JSXOutputState): string {
    let humanReadableIdPrefix = prefix + this.toPascalCase(type);

    let currentSequence = (output.idSequence[humanReadableIdPrefix] | 0) + 1;
    output.idSequence[humanReadableIdPrefix] = currentSequence;

    return `${humanReadableIdPrefix}${currentSequence}`;
  }

  private getSlotValueExpr(
    jsxAttributes: ts.JsxAttributes,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Expression | undefined {
    const attributes = this.extractJSXAttributes(jsxAttributes, output, transformContext);

    if (attributes.length === 0) {
      return undefined;
    }

    if (attributes.length > 1 || attributes[0].name !== 'slot') {
      this.onError(jsxAttributes, `slotted only support the 'slot' attribute`);
    }

    return attributes[0].value.value;
  }

  private toStatement(node: ts.Node, transformContext: ts.TransformationContext): ts.Statement {
    if (isExpressionNode(node)) {
      return transformContext.factory.createExpressionStatement(node);
    }

    // TODO(simon): Any edge cases here?
    return node as ts.Statement;
  }

  private toExpression(node: ts.Node, transformContext: ts.TransformationContext): ts.Expression {
    if (isExpressionNode(node)) {
      return node;
    }

    if (ts.isExpressionStatement(node)) {
      return node.expression;
    }

    // Convert to an immediately called lambda

    const arrowFunction = transformContext.factory.createArrowFunction(
      undefined,
      undefined,
      [],
      undefined,
      undefined,
      this.toBlock([this.toStatement(node, transformContext)], transformContext),
    );

    return transformContext.factory.createCallExpression(arrowFunction, [], []);
  }

  private toBlock(statements: ts.Statement[], transformContext: ts.TransformationContext): ts.Block {
    if (statements.length === 1 && ts.isBlock(statements[0])) {
      return statements[0] as ts.Block;
    }

    return transformContext.factory.createBlock(statements, true);
  }

  private mergeStatements(statements: ts.Statement[], transformContext: ts.TransformationContext): ts.Statement {
    if (!statements.length) {
      return transformContext.factory.createEmptyStatement();
    }

    return this.toBlock(statements, transformContext);
  }

  private outputJSXChildren(
    node: ts.Node,
    children: ts.JsxChild[],
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const processedStatements: ts.JsxChild[] = [];
    const statements: ts.Statement[] = [];

    for (const child of children) {
      if (this.isJsxText(child)) {
        continue;
      }

      processedStatements.push(child);

      const resultNode = this.transformNode(child, output, transformContext);
      statements.push(this.toStatement(resultNode, transformContext));
    }

    return this.flattenBlocks(node, processedStatements, statements);
  }

  private createRendererCallExpr(
    functionName: string,
    callArguments: ts.Expression[],
    transformContext: ts.TransformationContext,
  ): ts.Expression {
    const propertyAccess = transformContext.factory.createPropertyAccessExpression(
      transformContext.factory.createIdentifier('__Renderer'),
      functionName,
    );
    return transformContext.factory.createCallExpression(propertyAccess, undefined, callArguments);
  }

  private createRendererCall(
    functionName: string,
    callArguments: ts.Expression[],
    originalNode: ts.Node | undefined,
    transformContext: ts.TransformationContext,
  ): ts.Statement {
    const callExpression = this.createRendererCallExpr(functionName, callArguments, transformContext);
    const expressionStatement = transformContext.factory.createExpressionStatement(callExpression);

    if (originalNode) {
      ts.setTextRange(expressionStatement, originalNode);
    }
    return expressionStatement;
  }

  private isCompilerIntrinsicCall(node: ts.Node, intrisicCallName: string): node is ts.CallExpression {
    if (!ts.isCallExpression(node)) {
      return false;
    }

    if (!ts.isIdentifier(node.expression)) {
      return false;
    }

    return node.expression.text === intrisicCallName;
  }

  private isNamedSlotsExpression(expression: ts.Node): expression is ts.CallExpression {
    return this.isCompilerIntrinsicCall(expression, '$namedSlots');
  }

  private isUnnamedSlotsExpression(expression: ts.Node): expression is ts.CallExpression {
    return this.isCompilerIntrinsicCall(expression, '$slot');
  }

  private outputSlotExpression(
    sourceNode: ts.Node,
    intrinsicCallArguments: ts.NodeArray<ts.Node>,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement {
    if (intrinsicCallArguments.length !== 1) {
      this.onError(sourceNode, `Intrinsic slot call should have 1 argument`);
    }
    const node = this.transformNode(intrinsicCallArguments[0], output, transformContext);

    return this.createRendererCall(
      'setUnnamedSlot',
      [this.toExpression(node, transformContext)],
      undefined,
      transformContext,
    );
  }

  private isOptimizableNamedSlots(
    properties: ts.NodeArray<ts.ObjectLiteralElementLike>,
  ): properties is ts.NodeArray<ts.PropertyAssignment> {
    for (const property of properties) {
      if (!ts.isPropertyAssignment(property)) {
        return false;
      }
      if (!(ts.isIdentifier(property.name) || ts.isStringLiteral(property.name))) {
        return false;
      }
    }

    return true;
  }

  private toStringLiteral(node: ts.PropertyName, transformContext: ts.TransformationContext): ts.Expression {
    if (ts.isStringLiteral(node)) {
      return node;
    }
    if (ts.isIdentifier(node)) {
      return transformContext.factory.createStringLiteral(node.text, true);
    }

    this.onError(node, 'Cannot convert node to string literal');
  }

  private outputNamedSlotsExpression(
    sourceNode: ts.Node,
    intrinsicCallArguments: ts.NodeArray<ts.Node>,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    if (intrinsicCallArguments.length !== 1) {
      this.onError(sourceNode, `Intrinsic named slots call should have 1 argument`);
    }

    const arg = intrinsicCallArguments[0];

    if (ts.isObjectLiteralExpression(arg) && this.isOptimizableNamedSlots(arg.properties)) {
      return arg.properties.map((property: ts.PropertyAssignment) => {
        const node = this.transformNode(property.initializer, output, transformContext);

        return this.createRendererCall(
          'setNamedSlot',
          [this.toStringLiteral(property.name, transformContext), this.toExpression(node, transformContext)],
          sourceNode,
          transformContext,
        );
      });
    } else {
      const node = this.transformNode(arg, output, transformContext);

      return [
        this.createRendererCall(
          'setNamedSlots',
          [this.toExpression(node, transformContext)],
          sourceNode,
          transformContext,
        ),
      ];
    }
  }

  private getSingleJsxExpression(children: ts.JsxChild[]): ts.Expression | undefined {
    if (children.length !== 1) {
      return undefined;
    }

    const uniqueChild = children[0];
    if (!ts.isJsxExpression(uniqueChild)) {
      return undefined;
    }

    return uniqueChild.expression;
  }

  private outputSlot(
    node: ts.Node,
    children: ts.JsxChild[],
    slotName: ts.Expression | undefined,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const childrenWithoutTexts = children.filter(
      (child) => !(this.isEmptyJsxExpression(child) || this.isJsxText(child)),
    );
    const singleExpr = this.getSingleJsxExpression(childrenWithoutTexts);

    if (singleExpr && this.isNamedSlotsExpression(singleExpr)) {
      return this.outputNamedSlotsExpression(singleExpr, singleExpr.arguments, output, transformContext);
    } else if (singleExpr && this.isUnnamedSlotsExpression(singleExpr)) {
      return [this.outputSlotExpression(singleExpr, singleExpr.arguments, output, transformContext)];
    } else {
      const innerStatements = this.outputJSXChildren(node, childrenWithoutTexts, output, transformContext);
      const lambdaInner = transformContext.factory.createArrowFunction(
        undefined,
        undefined,
        [],
        undefined,
        undefined,
        this.toBlock(innerStatements, transformContext),
      );

      if (slotName) {
        return [this.createRendererCall('setNamedSlot', [slotName, lambdaInner], undefined, transformContext)];
      } else {
        return [this.createRendererCall('setUnnamedSlot', [lambdaInner], undefined, transformContext)];
      }
    }
  }

  private createConstVariable(
    variableName: string,
    expr: ts.Expression,
    transformContext: ts.TransformationContext,
  ): ts.VariableStatement {
    const variable = transformContext.factory.createVariableDeclaration(variableName, undefined, undefined, expr);
    const declarationList = transformContext.factory.createVariableDeclarationList([variable], ts.NodeFlags.Const);
    return transformContext.factory.createVariableStatement(undefined, declarationList);
  }

  private appendInitializedVariable(
    variablePrefix: string,
    variableSuffix: string,
    expr: ts.Expression,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Expression {
    const variableName = this.makeIdentifier(variablePrefix, variableSuffix, output);

    output.nodePrototypeDeclarations.push(this.createConstVariable(variableName, expr, transformContext));

    return transformContext.factory.createIdentifier(variableName);
  }

  private resolveSetAttributeCall(node: ts.Expression, output: JSXOutputState): string {
    const resolvedType = resolveTypeOfExpression(node, output.typeChecker);

    if (resolvedType.effectiveType === SymbolType.boolean) {
      return 'setAttributeBool';
    } else if (resolvedType.effectiveType === SymbolType.number) {
      return 'setAttributeNumber';
    } else if (resolvedType.effectiveType === SymbolType.string) {
      return 'setAttributeString';
    } else if (resolvedType.effectiveType === SymbolType.function) {
      return 'setAttributeFunction';
    } else if (resolvedType.effectiveType === SymbolType.object && resolvedType.effectiveObjectInfo?.name === 'Style') {
      return 'setAttributeStyle';
    }

    return 'setAttribute';
  }

  private outputElementInner(
    jsxElement: JSXElement,
    attributeStatements: JSXAttributeStatements,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const statements: ts.Statement[] = [];

    if (attributeStatements.nodeRef) {
      const transformedValueNode = this.transformNode(attributeStatements.nodeRef, output, transformContext);
      const transformedValue = this.toExpression(transformedValueNode, transformContext);

      statements.push(
        this.createRendererCall('setAttributeRef', [transformedValue], attributeStatements.nodeRef, transformContext),
      );
    }

    for (const attribute of attributeStatements.dynamicAttributes) {
      if (attribute.value.kind === JSXAttributeValueKind.spread) {
        statements.push(
          this.createRendererCall('setAttributes', [attribute.value.value], attribute.node, transformContext),
        );
      } else {
        const transformedValueNode = this.transformNode(attribute.value.value, output, transformContext);
        const transformedValue = this.toExpression(transformedValueNode, transformContext);
        const setAttributeCall = this.resolveSetAttributeCall(attribute.value.value, output);
        statements.push(
          this.createRendererCall(
            setAttributeCall,
            [createStringLiteral(attribute.name!, transformContext), transformedValue],
            attribute.node,
            transformContext,
          ),
        );
      }
    }

    statements.push(...this.outputJSXChildren(jsxElement.element, jsxElement.children, output, transformContext));

    statements.push(this.createRendererCall('endRender', [], undefined, transformContext));
    return statements;
  }

  private outputComponentViewModelProperties(
    attributeStatements: JSXAttributeStatements,
    nodeTypeStr: string,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const statements: ts.Statement[] = [];

    for (const attribute of attributeStatements.dynamicAttributes) {
      if (attribute.value.kind === JSXAttributeValueKind.spread) {
        statements.push(
          this.createRendererCall('setViewModelProperties', [attribute.value.value], attribute.node, transformContext),
        );
      } else {
        const transformedValueNode = this.transformNode(attribute.value.value, output, transformContext);
        const transformedValue = this.toExpression(transformedValueNode, transformContext);
        statements.push(
          this.createRendererCall(
            'setViewModelProperty',
            [createStringLiteral(attribute.name!, transformContext), transformedValue],
            attribute.node,
            transformContext,
          ),
        );
      }
    }

    return statements;
  }

  private insertChildrenToUnnamedSlot(children: ts.JsxChild[], slotteds: Slotted[]) {
    for (const slot of slotteds) {
      if (!slot.slotName) {
        if (slotteds[slotteds.length - 1] !== slot) {
          this.onError(
            children[children.length - 1],
            'All Elements or Components slotted into the default slot need to be direct siblings',
          );
        }

        slot.expressions.push(...children);
        return;
      }
    }

    slotteds.push({
      slotName: undefined,
      expressions: children,
    });
  }

  private isJsxText(child: ts.JsxChild): boolean {
    if (!ts.isJsxText(child)) {
      return false;
    }

    // Tolerance for `;` and whitespace characters in text, we just ignore them.
    let value = child.text.replace(/(\s|;)+/, '');

    if (value) {
      this.onError(child, 'Text elements are not supported in JSX. Use <label/> to render a text node.');
    }

    return true;
  }

  private isEmptyJsxExpression(child: ts.JsxChild): boolean {
    if (!ts.isJsxExpression(child)) {
      return false;
    }

    if (child.expression) {
      return false;
    }

    return true;
  }

  private ensureImplicitSlottedNotUsedWithExplicit(
    node: ts.Node,
    hasExplicitSlotted: boolean,
    hasImplicitSlotted: boolean,
  ) {
    if (hasExplicitSlotted && hasImplicitSlotted) {
      this.onError(
        node,
        'Cannot mix implicitly slotted elements with explicitly slotted elements. Please either use <slotted/> for all children of the component, or remove <slotted/> from all children.',
      );
    }
  }

  private outputComponentChildren(
    node: ts.Node,
    children: ts.JsxChild[],
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const slotteds: Slotted[] = [];
    let hasExplicitSlotted = false;
    let hasImplicitSlotted = false;

    for (const child of children) {
      if (this.isEmptyJsxExpression(child) || this.isJsxText(child)) {
        continue;
      }

      if (ts.isJsxElement(child) && child.openingElement.tagName.getText() === 'slotted') {
        hasExplicitSlotted = true;

        const slotName = this.getSlotValueExpr(child.openingElement.attributes, output, transformContext);
        if (slotName) {
          slotteds.push({ slotName, expressions: child.children.map((item) => item) });
        } else {
          this.insertChildrenToUnnamedSlot(
            child.children.map((item) => item),
            slotteds,
          );
        }
      } else if (ts.isJsxSelfClosingElement(child) && child.tagName.getText() === 'slotted') {
        hasExplicitSlotted = true;

        const slotName = this.getSlotValueExpr(child.attributes, output, transformContext);
        if (slotName) {
          slotteds.push({ slotName, expressions: [] });
        } else {
          this.insertChildrenToUnnamedSlot([], slotteds);
        }
      } else {
        hasImplicitSlotted = true;

        this.insertChildrenToUnnamedSlot([child], slotteds);
      }

      this.ensureImplicitSlottedNotUsedWithExplicit(node, hasExplicitSlotted, hasImplicitSlotted);
    }

    const statements: ts.Statement[] = [];

    for (const slotted of slotteds) {
      statements.push(...this.outputSlot(node, slotted.expressions, slotted.slotName, output, transformContext));
    }

    return statements;
  }

  private toParametersArray(
    parameters: readonly (ts.Expression | undefined)[],
    transformContext: ts.TransformationContext,
  ): ts.Expression[] {
    const trimmedParameters = [...parameters];
    while (trimmedParameters.length && !trimmedParameters[trimmedParameters.length - 1]) {
      trimmedParameters.pop();
    }

    return trimmedParameters.map((p) => p ?? transformContext.factory.createIdentifier('undefined'));
  }

  private outputRenderSlot(jsxElement: JSXElement, transformContext: ts.TransformationContext): ts.Statement {
    let slotName: ts.Expression | undefined;
    let slotRef: ts.Expression | undefined;
    let slotKey: ts.Expression | undefined;
    for (const attribute of jsxElement.attributes) {
      if (attribute.name === 'name') {
        slotName = attribute.value.value;
      } else if (attribute.name === 'ref') {
        slotRef = attribute.value.value;
      } else if (attribute.name === 'key') {
        slotKey = attribute.value.value;
      } else {
        this.onError(jsxElement.element, `Unsupported attribute '${attribute.name}' in slot`);
      }
    }

    if (jsxElement.children.length) {
      this.onError(jsxElement.element, 'Cannot declare children in a slot');
    }

    const thisParameter = transformContext.factory.createThis();

    if (slotName) {
      const parameters = [slotName, thisParameter, slotRef, slotKey];

      return this.createRendererCall(
        'renderNamedSlot',
        this.toParametersArray(parameters, transformContext),
        jsxElement.element,
        transformContext,
      );
    } else {
      const parameters = [thisParameter, slotRef, slotKey];

      return this.createRendererCall(
        'renderUnnamedSlot',
        this.toParametersArray(parameters, transformContext),
        jsxElement.element,
        transformContext,
      );
    }
  }

  private getJsxNodeKind(tag: ts.JsxTagNameExpression, nodeTypeStr: string, output: JSXOutputState): JSXNodeKind {
    if (ts.isIdentifier(tag)) {
      const isIntrinsic = nodeTypeStr[0].toLowerCase() === nodeTypeStr[0];
      if (isIntrinsic) {
        if (nodeTypeStr === 'slot') {
          return JSXNodeKind.slot;
        } else {
          if (nodeTypeStr === 'slotted') {
            this.onError(tag, '"slotted" elements must be direct children of components');
          }
          return JSXNodeKind.element;
        }
      }
    }

    if (tag.kind === ts.SyntaxKind.JsxNamespacedName) {
      this.onError(tag, 'JSX Namespaced Names are not supported');
    }

    const resolvedType = resolveTypeOfExpression(tag, output.typeChecker);

    const symbol = resolvedType.effectiveObjectInfo?.symbol;
    if ((symbol && symbol.flags & ts.SymbolFlags.Function) || resolvedType.effectiveType === SymbolType.function) {
      return JSXNodeKind.functionComponent;
    }

    // Consider any expressions as constructors
    return JSXNodeKind.component;
  }

  private outputJSXElement(
    jsxElement: JSXElement,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
    nodeTypeStr: string,
    attributeStatements: JSXAttributeStatements,
  ): JSXNodeOutput {
    let makeNodePrototypeParams: ts.Expression[] = [createStringLiteral(nodeTypeStr, transformContext)];

    if (attributeStatements.staticAttributesLiteral) {
      makeNodePrototypeParams.push(attributeStatements.staticAttributesLiteral);
    }

    const nodeIdentifier = this.appendInitializedVariable(
      '__node',
      nodeTypeStr,
      this.createRendererCallExpr('makeNodePrototype', makeNodePrototypeParams, transformContext),
      output,
      transformContext,
    );

    const keyOrUndefined = attributeStatements.key || transformContext.factory.createIdentifier('undefined');

    if (attributeStatements.lazy) {
      const beginIfNeeded = this.createRendererCallExpr(
        'beginRenderIfNeeded',
        [nodeIdentifier, keyOrUndefined],
        transformContext,
      );

      const innerStatements = this.outputElementInner(jsxElement, attributeStatements, output, transformContext);
      const endRenderStatement = innerStatements[innerStatements.length - 1];

      const beginRenderStatement = transformContext.factory.createIfStatement(
        beginIfNeeded,
        transformContext.factory.createBlock(innerStatements),
      );
      return {
        beginRenderStatement,
        endRenderStatement,
        statements: [beginRenderStatement],
      };
    } else {
      const beginRenderStatement = this.createRendererCall(
        'beginRender',
        [nodeIdentifier, keyOrUndefined],
        undefined,
        transformContext,
      );
      const statements: ts.Statement[] = [];
      statements.push(beginRenderStatement);
      statements.push(...this.outputElementInner(jsxElement, attributeStatements, output, transformContext));
      const endRenderStatement = statements[statements.length - 1];

      return {
        beginRenderStatement,
        endRenderStatement,
        statements,
      };
    }
  }

  private outputJSXSlot(
    jsxElement: JSXElement,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
    nodeTypeStr: string,
    attributeStatements: JSXAttributeStatements,
  ): JSXNodeOutput {
    const beginRenderStatement = this.outputRenderSlot(jsxElement, transformContext);
    return {
      beginRenderStatement,
      endRenderStatement: undefined,
      statements: [beginRenderStatement],
    };
  }

  private throwInvalidJSXFunctionComponentAttribute(jsxElement: JSXElement, attributeName: string) {
    this.onError(jsxElement.element, `'${attributeName}' attribute is not supported in Inline Function Components`);
  }

  private outputJSXFunctionComponent(
    jsxElement: JSXElement,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
    nodeTypeStr: string,
    attributeStatements: JSXAttributeStatements,
  ): JSXNodeOutput {
    if (attributeStatements.key) {
      this.throwInvalidJSXFunctionComponentAttribute(jsxElement, 'key');
    }
    if (attributeStatements.lazy) {
      this.throwInvalidJSXFunctionComponentAttribute(jsxElement, 'lazy');
    }
    if (attributeStatements.slot) {
      this.throwInvalidJSXFunctionComponentAttribute(jsxElement, 'slot');
    }
    if (attributeStatements.nodeRef) {
      this.throwInvalidJSXFunctionComponentAttribute(jsxElement, 'ref');
    }

    const transformedNodes = jsxElement.children
      .filter((child) => !(this.isEmptyJsxExpression(child) || this.isJsxText(child)))
      .map((child) => this.transformNode(child, output, transformContext));

    const viewModelProperties: ts.ObjectLiteralElementLike[] = [];

    for (const attribute of attributeStatements.allAttributes) {
      if (attribute.name) {
        viewModelProperties.push(
          transformContext.factory.createPropertyAssignment(
            transformContext.factory.createIdentifier(attribute.name),
            attribute.value.value,
          ),
        );
      } else {
        if (attribute.value.kind !== JSXAttributeValueKind.spread) {
          this.onError(attribute.node, 'Unexpectedly got attribute without a name');
        }

        viewModelProperties.push(transformContext.factory.createSpreadAssignment(attribute.value.value));
      }
    }

    if (transformedNodes.length) {
      const renderFunction = transformContext.factory.createArrowFunction(
        undefined,
        undefined,
        [],
        undefined,
        undefined,
        this.toBlock(
          transformedNodes.map((node) => this.toStatement(node, transformContext)),
          transformContext,
        ),
      );
      viewModelProperties.push(
        transformContext.factory.createPropertyAssignment(
          transformContext.factory.createIdentifier('children'),
          renderFunction,
        ),
      );
    }

    const viewModelLiteral = transformContext.factory.createObjectLiteralExpression(viewModelProperties, true);

    if (jsxElement.element.tagName.kind === ts.SyntaxKind.JsxNamespacedName) {
      this.onError(jsxElement.nodeType, 'JSX Namespaced Names are not supported');
    }

    const statement = this.createRendererCall(
      'renderFnComponent',
      [jsxElement.element.tagName, viewModelLiteral],
      jsxElement.element,
      transformContext,
    );

    return {
      beginRenderStatement: statement,
      endRenderStatement: undefined,
      statements: [statement],
    };
  }

  private outputJSXComponent(
    jsxElement: JSXElement,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
    nodeTypeStr: string,
    attributeStatements: JSXAttributeStatements,
  ): JSXNodeOutput {
    let makeComponentPrototypeParams: ts.Expression[] = [];

    if (attributeStatements.staticAttributesLiteral) {
      makeComponentPrototypeParams.push(attributeStatements.staticAttributesLiteral);
    }

    const componentIdentifier = this.appendInitializedVariable(
      '__component',
      nodeTypeStr,
      this.createRendererCallExpr('makeComponentPrototype', makeComponentPrototypeParams, transformContext),
      output,
      transformContext,
    );

    const componentRef = attributeStatements.nodeRef ?? transformContext.factory.createIdentifier('undefined');
    if (jsxElement.nodeType.kind === ts.SyntaxKind.JsxNamespacedName) {
      this.onError(jsxElement.nodeType, 'JSX Namespaced Names are not supported');
    }
    const beginRenderStatement = this.createRendererCall(
      'beginComponent',
      [
        jsxElement.nodeType,
        componentIdentifier,
        attributeStatements.key || transformContext.factory.createIdentifier('undefined'),
        componentRef,
      ],
      undefined,
      transformContext,
    );

    const statements: ts.Statement[] = [];
    statements.push(beginRenderStatement);

    if (attributeStatements.componentContext) {
      let foundComponentContext: boolean;
      if (ts.isIdentifier(attributeStatements.componentContext)) {
        foundComponentContext = attributeStatements.componentContext.text !== 'undefined';
      } else {
        foundComponentContext = isExpressionNode(attributeStatements.componentContext);
      }

      if (foundComponentContext) {
        const hasContextCallExpr = this.createRendererCallExpr('hasContext', [], transformContext);
        const setContextCall = this.createRendererCall(
          'setContext',
          [attributeStatements.componentContext],
          attributeStatements.componentContext,
          transformContext,
        );

        const ifStatement = transformContext.factory.createIfStatement(
          transformContext.factory.createPrefixUnaryExpression(ts.SyntaxKind.ExclamationToken, hasContextCallExpr),
          transformContext.factory.createBlock([setContextCall]),
        );
        statements.push(ifStatement);
      }
    }

    statements.push(
      ...this.outputComponentViewModelProperties(attributeStatements, nodeTypeStr, output, transformContext),
    );

    for (const injectedAttribute of attributeStatements.injectedAttributes) {
      const transformedValueNode = this.transformNode(injectedAttribute.value.value, output, transformContext);
      const transformedValue = this.toExpression(transformedValueNode, transformContext);
      statements.push(
        this.createRendererCall(
          'setInjectedAttribute',
          [createStringLiteral(injectedAttribute.name!, transformContext), transformedValue],
          injectedAttribute.node,
          transformContext,
        ),
      );
    }

    statements.push(...this.outputComponentChildren(jsxElement.element, jsxElement.children, output, transformContext));

    const endRenderStatement = this.createRendererCall('endComponent', [], undefined, transformContext);
    statements.push(endRenderStatement);

    return {
      beginRenderStatement,
      endRenderStatement,
      statements,
    };
  }

  private outputJSX(
    jsxElement: JSXElement,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const nodeTypeStr = jsxElement.nodeType.getText();

    const nodeKind = this.getJsxNodeKind(jsxElement.nodeType, nodeTypeStr, output);
    const attributeStatements = this.resolveJSXAttributeStatements(nodeKind, jsxElement, transformContext);

    let nodeOutput: JSXNodeOutput;

    switch (nodeKind) {
      case JSXNodeKind.element:
        nodeOutput = this.outputJSXElement(jsxElement, output, transformContext, nodeTypeStr, attributeStatements);
        break;
      case JSXNodeKind.slot:
        nodeOutput = this.outputJSXSlot(jsxElement, output, transformContext, nodeTypeStr, attributeStatements);
        break;
      case JSXNodeKind.component:
        nodeOutput = this.outputJSXComponent(jsxElement, output, transformContext, nodeTypeStr, attributeStatements);
        break;
      case JSXNodeKind.functionComponent:
        nodeOutput = this.outputJSXFunctionComponent(
          jsxElement,
          output,
          transformContext,
          nodeTypeStr,
          attributeStatements,
        );
        break;
    }

    ts.setTextRange(nodeOutput.beginRenderStatement, jsxElement.element.tagName);
    if (nodeOutput.endRenderStatement) {
      if (jsxElement.closingElement) {
        ts.setTextRange(nodeOutput.endRenderStatement, jsxElement.closingElement.tagName);
      } else if (ts.isJsxSelfClosingElement(jsxElement.element)) {
        // We set the text range of the endComponent/endRender on the slash element
        // of the self closing element.
        let slashElement: ts.Node | undefined;
        for (const child of jsxElement.element.getChildren()) {
          if (child.kind === ts.SyntaxKind.SlashToken) {
            slashElement = child;
          }
        }

        if (slashElement) {
          ts.setTextRange(nodeOutput.endRenderStatement, slashElement);
        } else {
          this.logger?.error?.('Could not find Slash element in JSX self closing element');
        }
      }
    }

    return nodeOutput.statements;
  }

  private findDeclarationsInsertionIndex(previousStatements: ts.NodeArray<ts.Statement>): number {
    for (let i = 0; i < previousStatements.length; i++) {
      const statement = previousStatements[i];

      if (
        ts.isExpressionStatement(statement) &&
        ts.isStringLiteral(statement.expression) &&
        statement.expression.text === 'use strict'
      ) {
        return i + 1;
      }
    }

    return 0;
  }

  private injectDeclarations(
    node: ts.SourceFile,
    outputState: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.SourceFile {
    const requireCall = transformContext.factory.createCallExpression(
      transformContext.factory.createIdentifier('require'),
      undefined,
      [createStringLiteral(rendererModulePath, transformContext)],
    );
    const accessToJsx = transformContext.factory.createPropertyAccessExpression(requireCall, 'jsx');

    const rendererVariableStatement = this.createConstVariable('__Renderer', accessToJsx, transformContext);
    outputState.nodePrototypeDeclarations.splice(0, 0, rendererVariableStatement);

    const allStatements: ts.Statement[] = [];

    const insertionIndex = this.findDeclarationsInsertionIndex(node.statements);

    for (const oldStatement of node.statements) {
      allStatements.push(oldStatement);
    }

    allStatements.splice(insertionIndex, 0, ...outputState.nodePrototypeDeclarations);

    return this.doUpdateSourceFileNode(node, allStatements, transformContext);
  }

  private processJSXElement(
    node: ts.JsxElement,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const attributes = this.extractJSXAttributes(node.openingElement.attributes, output, transformContext);

    const children: ts.JsxChild[] = [];
    for (const child of node.children) {
      children.push(child);
    }

    return this.outputJSX(
      {
        element: node.openingElement,
        closingElement: node.closingElement,
        nodeType: node.openingElement.tagName,
        attributes,
        children,
      },
      output,
      transformContext,
    );
  }

  private processSelfClosingJSXElement(
    node: ts.JsxSelfClosingElement,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Statement[] {
    const attributes = this.extractJSXAttributes(node.attributes, output, transformContext);

    return this.outputJSX(
      {
        element: node,
        closingElement: undefined,
        nodeType: node.tagName,
        attributes,
        children: [],
      },
      output,
      transformContext,
    );
  }

  private logNode(node: ts.Node, state: JSXOutputState) {
    let indent = '';
    for (let i = 0; i < state.nodeDepth; i++) {
      indent += '  ';
    }
    console.log(`${indent}Processing node: ${node.kind}`);
  }

  /**
   * Merge any added blocks into the statements list
   */
  private flattenBlocks(
    node: ts.Node,
    previousStatements: ts.NodeArray<ts.Node> | ts.Node[],
    newStatements: ts.Statement[],
  ): ts.Statement[] {
    if (newStatements.length !== previousStatements.length) {
      this.onError(
        node,
        `Invalid output statements, had length ${previousStatements.length} new length ${newStatements.length}`,
      );
    }

    const out: ts.Statement[] = [];

    for (let i = 0; i < previousStatements.length; i++) {
      const oldStatement = previousStatements[i];
      const newStatement = newStatements[i];

      if (ts.isBlock(newStatement) && !ts.isBlock(oldStatement)) {
        // We created a block, merge its content into the statements
        out.push(...newStatement.statements);
      } else {
        out.push(newStatement);
      }
    }

    return out;
  }

  private doUpdateSourceFileNode(
    node: ts.SourceFile,
    newStatements: ts.Statement[],
    transformContext: ts.TransformationContext,
  ): ts.SourceFile {
    const newNode = transformContext.factory.updateSourceFile(
      node,
      newStatements,
      undefined,
      undefined,
      undefined,
      undefined,
      undefined,
    );
    ts.addEmitHelpers(newNode, transformContext.readEmitHelpers());
    return newNode;
  }

  private transformSourceFile(
    node: ts.SourceFile,
    outputState: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.SourceFile {
    const newStatements: ts.Statement[] = [];

    for (const statement of node.statements) {
      const result = this.transformNode(statement, outputState, transformContext);
      newStatements.push(this.toStatement(result, transformContext));
    }

    return this.doUpdateSourceFileNode(
      node,
      this.flattenBlocks(node, node.statements, newStatements),
      transformContext,
    );
  }

  private transformNode(
    node: ts.Node,
    outputState: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Node {
    // this.logNode(node, outputState);

    outputState.nodeDepth++;

    try {
      if (ts.isJsxElement(node)) {
        const result = this.processJSXElement(node, outputState, transformContext);
        return this.mergeStatements(result, transformContext);
      } else if (ts.isJsxSelfClosingElement(node)) {
        const result = this.processSelfClosingJSXElement(node, outputState, transformContext);
        return this.mergeStatements(result, transformContext);
      }

      if (ts.isExpressionStatement(node)) {
        const result = this.transformNode(node.expression, outputState, transformContext);
        if (isExpressionNode(result)) {
          return transformContext.factory.updateExpressionStatement(node, result);
        }
        // We are not an expression anymore
        return result;
      }

      if (ts.isJsxExpression(node) && node.expression) {
        const result = this.transformNode(node.expression, outputState, transformContext);
        if (isExpressionNode(result)) {
          return transformContext.factory.updateJsxExpression(node, result);
        }
        return result;
      }

      if (ts.isParenthesizedExpression(node)) {
        const result = this.transformNode(node.expression, outputState, transformContext);
        if (isExpressionNode(result)) {
          return transformContext.factory.updateParenthesizedExpression(node, result);
        }

        return result;
      }

      if (ts.isBlock(node)) {
        const statements = node.statements.map((statement) =>
          this.toStatement(this.transformNode(statement, outputState, transformContext), transformContext),
        );

        const newStatements = this.flattenBlocks(node, node.statements, statements);
        return transformContext.factory.updateBlock(node, newStatements);
      }

      return ts.visitEachChild(
        node,
        (childNode) => this.transformNodeGeneric(node, childNode, outputState, transformContext),
        transformContext,
      );
    } finally {
      outputState.nodeDepth--;
    }
  }

  // A version of transformNode which which will ensure that we transform statements into expressions
  // if our node was an expression
  private transformNodeGeneric(
    parent: ts.Node,
    node: ts.Node,
    outputState: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): ts.Node {
    const result = this.transformNode(node, outputState, transformContext);

    if (isExpressionNode(node) && !isExpressionNode(result)) {
      if (ts.isArrowFunction(parent) && parent.body === node) {
        // Special case for arrow functions, we don't need to convert them to expression since
        // the body can be a statement
        return result;
      }

      // We were an expression but we are not anymore, convert to an expression
      return this.toExpression(result, transformContext);
    }

    return result;
  }

  private onError(node: ts.Node, message: string): never {
    const { line, character } = node.getSourceFile().getLineAndCharacterOfPosition(node.getStart());
    throw new Error(`${message} at (${line + 1},${character + 1}) (node content: ${node.getText()})`);
  }

  private log(node: ts.Node, message: string) {
    const { line, character } = node.getSourceFile().getLineAndCharacterOfPosition(node.getStart());
    console.log(`(${line + 1},${character + 1}) with type '${node.kind}' (node content: ${node.getText()}: ${message}`);
  }

  private onUnexpectedNodeKind(node: ts.Node) {
    this.onError(node, `Unexpectedly got node kind ${node.kind}`);
  }

  private checkNodeKind(node: ts.Node, expectedKind: ts.SyntaxKind) {
    if (node.kind !== expectedKind) {
      this.onError(node, `Unexpectedly got node kind ${node.kind} (expected ${expectedKind})`);
    }
  }

  private isExpressionNonStaticMethodDeclaration(expression: ts.Expression, output: JSXOutputState): boolean {
    const resolvedType = resolveTypeOfExpression(expression, output.typeChecker);

    return (
      resolvedType.effectiveType === SymbolType.function &&
      resolvedType.effectiveObjectInfo?.declaration?.kind === ts.SyntaxKind.MethodDeclaration &&
      (ts.getCombinedModifierFlags(resolvedType.effectiveObjectInfo.declaration) & ts.ModifierFlags.Static) === 0
    );
  }

  private getJSXAttributeValue(value: ts.Node, output: JSXOutputState): JSXAttributeValue {
    if (value.kind === ts.SyntaxKind.JsxExpression) {
      const children = value.getChildren();

      if (children.length !== 3) {
        this.onError(value, `Should have 3 tokens (got ${children.length})`);
      }

      const expression = children[1];

      if (expression.kind === ts.SyntaxKind.NumericLiteral || expression.kind === ts.SyntaxKind.StringLiteral) {
        // Shortcut for numeric and string literals, they are never changing so we can consider them constants
        return {
          value: expression as ts.LiteralExpression,
          kind: JSXAttributeValueKind.static,
        };
      }

      if (
        ts.isPropertyAccessExpression(expression) &&
        this.isExpressionNonStaticMethodDeclaration(expression, output)
      ) {
        this.onError(
          value,
          'Resolved TSX attribute value is a method declaration. Please make sure to use a lambda as a stored property to avoid unintentional scoping of `this`, for example `private yourLambdaProperty = () => { ... }`',
        );
      }

      return {
        value: expression as ts.Expression,
        kind: JSXAttributeValueKind.dynamic,
      };
    } else {
      return {
        value: value as ts.Expression,
        kind: JSXAttributeValueKind.static,
      };
    }
  }

  private extractJSXAttribute(
    attribute: ts.Node,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): JSXAttribute {
    const children = attribute.getChildren();
    if (!children.length) {
      this.onError(attribute, 'Could not resolve JSX attribute');
    }

    this.checkNodeKind(children[0], ts.SyntaxKind.Identifier);
    if (children.length != 1 && children.length !== 3) {
      this.onError(attribute, `Should have 1 or 3 tokens (got ${children.length})`);
    }

    const name = children[0].getText();

    if (name.indexOf('-') >= 0) {
      this.onError(attribute, `JSX attributes cannot contain dashes`);
    }

    let value: JSXAttributeValue;

    if (children.length === 3) {
      value = this.getJSXAttributeValue(children[2], output);
    } else {
      value = {
        value: transformContext.factory.createTrue(),
        kind: JSXAttributeValueKind.static,
      };
    }

    return {
      node: attribute,
      name,
      value,
    };
  }

  private extractJSXSpreadAttribute(node: ts.Node): JSXAttributeValue {
    const children = node.getChildren();
    if (children.length !== 4) {
      this.onError(node, `Should have 4 tokens (got ${children.length})`);
    }

    const object = children[2];
    return {
      value: object as ts.Expression,
      kind: JSXAttributeValueKind.spread,
    };
  }

  private extractJSXAttributes(
    node: ts.Node,
    output: JSXOutputState,
    transformContext: ts.TransformationContext,
  ): JSXAttribute[] {
    const jsxAttributes: JSXAttribute[] = [];
    ts.forEachChild(node, (attribute) => {
      switch (attribute.kind) {
        case ts.SyntaxKind.JsxAttribute:
          {
            let jsxAttribute = this.extractJSXAttribute(attribute, output, transformContext);
            jsxAttributes.push(jsxAttribute);
          }
          break;
        case ts.SyntaxKind.JsxSpreadAttribute:
          {
            let jsxAttribute = this.extractJSXSpreadAttribute(attribute);
            jsxAttributes.push({
              node: attribute,
              name: undefined,
              value: jsxAttribute,
            });
          }
          break;
        default:
          this.onUnexpectedNodeKind(node);
      }
    });

    return jsxAttributes;
  }

  static createFromFile(fileName: string, fileContent: string, includeSourceMapping: boolean): JSXFileProcessor {
    return new JSXFileProcessor(fileName, fileContent, includeSourceMapping);
  }
}

export class JSXFileProcessor extends JSXProcessor {
  private fileName: string;
  private fileContent: string;
  private includeSourceMapping: boolean;

  constructor(fileName: string, fileContent: string, includeSourceMapping: boolean) {
    super();
    this.fileName = fileName;
    this.fileContent = fileContent;
    this.includeSourceMapping = includeSourceMapping;
  }

  process(): string {
    let compilerOptions: ts.CompilerOptions = {
      target: ts.ScriptTarget.ES2019,
      jsx: ts.JsxEmit.Preserve,
      removeComments: false,
    };

    if (this.includeSourceMapping) {
      compilerOptions.sourceMap = true;
      compilerOptions.sourceRoot = '/';
      compilerOptions.inlineSourceMap = true;
      // Uncomment to enable show sources in safari debugger
      // compilerOptions.inlineSources = true;
    }

    const transpiler = new Transpiler(this.fileContent, {
      compilerOptions: compilerOptions,
      fileName: this.fileName,
    });

    const transforms = this.makeTransformers(transpiler.program.getTypeChecker());
    transforms.push(createConsoleLogTransformer());
    const result = transpiler.transpile({ before: transforms });

    let compiledJs = result.outputText;

    if (this.includeSourceMapping) {
      compiledJs = mergeSourceMaps(this.fileContent, compiledJs);
    }

    return compiledJs;
  }
}
