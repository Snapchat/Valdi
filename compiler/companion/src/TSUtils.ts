import * as ts from 'typescript';

export function isExpressionNode(node: ts.Node): node is ts.Expression {
  switch (node.kind) {
    case ts.SyntaxKind.SuperKeyword:
    case ts.SyntaxKind.NullKeyword:
    case ts.SyntaxKind.TrueKeyword:
    case ts.SyntaxKind.FalseKeyword:
    case ts.SyntaxKind.RegularExpressionLiteral:
    case ts.SyntaxKind.ArrayLiteralExpression:
    case ts.SyntaxKind.ObjectLiteralExpression:
    case ts.SyntaxKind.PropertyAccessExpression:
    case ts.SyntaxKind.ElementAccessExpression:
    case ts.SyntaxKind.CallExpression:
    case ts.SyntaxKind.NewExpression:
    case ts.SyntaxKind.TaggedTemplateExpression:
    case ts.SyntaxKind.AsExpression:
    case ts.SyntaxKind.TypeAssertionExpression:
    case ts.SyntaxKind.NonNullExpression:
    case ts.SyntaxKind.ParenthesizedExpression:
    case ts.SyntaxKind.FunctionExpression:
    case ts.SyntaxKind.ClassExpression:
    case ts.SyntaxKind.ArrowFunction:
    case ts.SyntaxKind.VoidExpression:
    case ts.SyntaxKind.DeleteExpression:
    case ts.SyntaxKind.TypeOfExpression:
    case ts.SyntaxKind.PrefixUnaryExpression:
    case ts.SyntaxKind.PostfixUnaryExpression:
    case ts.SyntaxKind.BinaryExpression:
    case ts.SyntaxKind.ConditionalExpression:
    case ts.SyntaxKind.SpreadElement:
    case ts.SyntaxKind.TemplateExpression:
    case ts.SyntaxKind.OmittedExpression:
    case ts.SyntaxKind.JsxElement:
    case ts.SyntaxKind.JsxSelfClosingElement:
    case ts.SyntaxKind.JsxFragment:
    case ts.SyntaxKind.YieldExpression:
    case ts.SyntaxKind.AwaitExpression:
    case ts.SyntaxKind.MetaProperty:
      return true;
    case ts.SyntaxKind.QualifiedName:
      while (node.parent.kind === ts.SyntaxKind.QualifiedName) {
        node = node.parent;
      }
      return node.parent.kind === ts.SyntaxKind.TypeQuery || isJSXTagName(node);
    case ts.SyntaxKind.Identifier:
      if (node.parent.kind === ts.SyntaxKind.TypeQuery || isJSXTagName(node)) {
        return true;
      }
    // falls through

    case ts.SyntaxKind.NumericLiteral:
    case ts.SyntaxKind.BigIntLiteral:
    case ts.SyntaxKind.StringLiteral:
    case ts.SyntaxKind.NoSubstitutionTemplateLiteral:
    case ts.SyntaxKind.ThisKeyword:
      return isInExpressionContext(node);
    default:
      return false;
  }
}

export function isInExpressionContext(node: ts.Node): boolean {
  const { parent } = node;
  switch (parent.kind) {
    case ts.SyntaxKind.VariableDeclaration:
    case ts.SyntaxKind.Parameter:
    case ts.SyntaxKind.PropertyDeclaration:
    case ts.SyntaxKind.PropertySignature:
    case ts.SyntaxKind.EnumMember:
    case ts.SyntaxKind.PropertyAssignment:
    case ts.SyntaxKind.BindingElement:
      return (parent as ts.HasInitializer).initializer === node;
    case ts.SyntaxKind.ExpressionStatement:
    case ts.SyntaxKind.IfStatement:
    case ts.SyntaxKind.DoStatement:
    case ts.SyntaxKind.WhileStatement:
    case ts.SyntaxKind.ReturnStatement:
    case ts.SyntaxKind.WithStatement:
    case ts.SyntaxKind.SwitchStatement:
    case ts.SyntaxKind.CaseClause:
    case ts.SyntaxKind.ThrowStatement:
      return (<ts.ExpressionStatement>parent).expression === node;
    case ts.SyntaxKind.ForStatement:
      const forStatement = <ts.ForStatement>parent;
      return (
        (forStatement.initializer === node &&
          forStatement.initializer.kind !== ts.SyntaxKind.VariableDeclarationList) ||
        forStatement.condition === node ||
        forStatement.incrementor === node
      );
    case ts.SyntaxKind.ForInStatement:
    case ts.SyntaxKind.ForOfStatement:
      const forInStatement = <ts.ForInStatement | ts.ForOfStatement>parent;
      return (
        (forInStatement.initializer === node &&
          forInStatement.initializer.kind !== ts.SyntaxKind.VariableDeclarationList) ||
        forInStatement.expression === node
      );
    case ts.SyntaxKind.TypeAssertionExpression:
    case ts.SyntaxKind.AsExpression:
      return node === (<ts.AssertionExpression>parent).expression;
    case ts.SyntaxKind.TemplateSpan:
      return node === (<ts.TemplateSpan>parent).expression;
    case ts.SyntaxKind.ComputedPropertyName:
      return node === (<ts.ComputedPropertyName>parent).expression;
    case ts.SyntaxKind.Decorator:
    case ts.SyntaxKind.JsxExpression:
    case ts.SyntaxKind.JsxAttribute:
    case ts.SyntaxKind.JsxSpreadAttribute:
    case ts.SyntaxKind.SpreadAssignment:
      return true;
    case ts.SyntaxKind.ExpressionWithTypeArguments:
      return (<ts.ExpressionWithTypeArguments>parent).expression === node;
    case ts.SyntaxKind.ShorthandPropertyAssignment:
      return (<ts.ShorthandPropertyAssignment>parent).objectAssignmentInitializer === node;
    default:
      return isExpressionNode(parent);
  }
}

export function isJSXTagName(node: ts.Node) {
  const { parent } = node;
  if (
    parent.kind === ts.SyntaxKind.JsxOpeningElement ||
    parent.kind === ts.SyntaxKind.JsxSelfClosingElement ||
    parent.kind === ts.SyntaxKind.JsxClosingElement
  ) {
    return (<ts.JsxOpeningLikeElement>parent).tagName === node;
  }
  return false;
}

export interface NodeComments {
  text: string;
  start: number;
  end: number;
}

export function getNodeComments(node: ts.Node): NodeComments | undefined {
  const leadingTriviaWidth = node.getLeadingTriviaWidth();
  if (leadingTriviaWidth === 0) {
    return undefined;
  }
  const nodeStart = node.getFullStart();
  const sourceFileText = node.getSourceFile().text;

  const leadingTriva = sourceFileText.substring(nodeStart, nodeStart + leadingTriviaWidth);
  const trimmedLeft = leadingTriva.trimStart();

  const trimLeftShrunkSize = leadingTriva.length - trimmedLeft.length;

  let resolvedStart = nodeStart + trimLeftShrunkSize;
  let resolvedWidth = leadingTriviaWidth - trimLeftShrunkSize;

  const trimmedRight = trimmedLeft.trimEnd();

  const trimRightShrunkSize = trimmedLeft.length - trimmedRight.length;
  resolvedWidth -= trimRightShrunkSize;

  if (!trimmedRight) {
    return undefined;
  }

  return {
    text: trimmedRight,
    start: resolvedStart,
    end: resolvedStart + resolvedWidth,
  };
}

export interface NodeToDump {
  node: ts.Node;
  leadingComments: NodeComments | undefined;
}

export function isNodeExported(node: ts.Node): boolean {
  const modifiers = ts.getCombinedModifierFlags(node as ts.Declaration);

  return (modifiers & ts.ModifierFlags.Export) !== 0;
}

export function getRootNodesToDump(
  sourceFileNode: ts.Node,
  predicate: ((node: NodeToDump) => boolean) | undefined,
): NodeToDump[] {
  const children = sourceFileNode.getChildAt(0).getChildren();

  const result: NodeToDump[] = [];
  for (const child of children) {
    const leadingTriva = getNodeComments(child);
    const nodeToDump: NodeToDump = {
      node: child,
      leadingComments: leadingTriva,
    };
    if (!predicate || predicate(nodeToDump)) {
      result.push(nodeToDump);
    }
  }
  return result;
}

export function findNode(
  node: ts.Node,
  position: number | undefined,
  visit: (node: ts.Node) => boolean,
): ts.Node | undefined {
  const start = node.getStart();
  const end = node.getEnd();

  if (position !== undefined && (position < start || position > end)) {
    return undefined;
  }

  if (visit(node)) {
    return node;
  }

  const childCount = node.getChildCount();
  for (let i = 0; i < childCount; i++) {
    const child = node.getChildAt(i);
    const bestNode = findNode(child, position, visit);
    if (bestNode) {
      return bestNode;
    }
  }

  return undefined;
}

export const enum SymbolType {
  any = 1,
  undefined,
  null,
  number,
  boolean,
  string,
  object,
  function,
  bigint,
}

export interface SymbolObjectInfo {
  readonly name: string;
  readonly declaration?: ts.Declaration;
  readonly symbol?: ts.Symbol;
}

export interface ResolvedSymbol {
  readonly type: SymbolType;
  readonly objectInfo?: SymbolObjectInfo;
}

function symbolObjectInfoEquals(left: SymbolObjectInfo | undefined, right: SymbolObjectInfo | undefined): boolean {
  if (!left || !right) {
    return left === right;
  }

  return left.declaration === right.declaration;
}

function appendResolvedSymbolIfNeeded(resolvedSymbol: ResolvedSymbol, output: ResolvedSymbol[]): boolean {
  for (const existingSymbol of output) {
    if (
      resolvedSymbol.type === existingSymbol.type &&
      symbolObjectInfoEquals(resolvedSymbol.objectInfo, existingSymbol.objectInfo)
    ) {
      return false;
    }
  }

  output.push(resolvedSymbol);
  return true;
}

function getResolvedSymbolForType(type: ts.Type): ResolvedSymbol[] {
  if (type.isUnionOrIntersection()) {
    const out: ResolvedSymbol[] = [];

    for (const childType of type.types) {
      const childSymbols = getResolvedSymbolForType(childType);
      for (const childSymbol of childSymbols) {
        appendResolvedSymbolIfNeeded(childSymbol, out);
      }
    }

    return out;
  }

  const typeFlags = type.getFlags();

  if (typeFlags & ts.TypeFlags.BigIntLike) {
    return [{ type: SymbolType.bigint }];
  }

  if (typeFlags & ts.TypeFlags.NumberLike) {
    return [{ type: SymbolType.number }];
  }

  if (typeFlags & ts.TypeFlags.StringLike) {
    return [{ type: SymbolType.string }];
  }

  if (typeFlags & ts.TypeFlags.Undefined) {
    return [{ type: SymbolType.undefined }];
  }

  if (typeFlags & ts.TypeFlags.Null) {
    return [{ type: SymbolType.null }];
  }

  if (typeFlags & ts.TypeFlags.Object) {
    const objectType = type as ts.ObjectType;
    if (objectType.objectFlags & ts.ObjectFlags.Reference) {
      const reference = objectType as ts.TypeReference;
      if (reference.target !== reference) {
        return getResolvedSymbolForType(reference.target);
      }
    }

    const symbol = type.getSymbol();
    if (symbol) {
      const declaration = symbol.declarations?.[0];
      const symbolName = symbol.getName();
      const type =
        declaration && (ts.isFunctionLike(declaration) || symbolName === 'Function')
          ? SymbolType.function
          : SymbolType.object;

      return [{ type, objectInfo: { name: symbolName, declaration, symbol } }];
    }
  }

  return [{ type: SymbolType.any }];
}

export function resolvedSymbolsOfExpression(expression: ts.Expression, typeChecker: ts.TypeChecker): ResolvedSymbol[] {
  const type = typeChecker.getTypeAtLocation(expression);
  return getResolvedSymbolForType(type);
}

export interface ResolvedType {
  readonly resolvedSymbols: ResolvedSymbol[];
  readonly isNullable: boolean;
  readonly effectiveType: SymbolType;
  readonly effectiveObjectInfo: SymbolObjectInfo | undefined;
}

export function getSyntaxKindString(syntaxKind: ts.SyntaxKind): string {
  return ts.SyntaxKind[syntaxKind];
}

const MAX_NODE_DEBUG_LENGTH = 50;

export function getNodeDebugDescription(node: ts.Node): string {
  const sourceFile = node.getSourceFile();
  const position = sourceFile.getLineAndCharacterOfPosition(node.getStart());

  let text = node.getText();
  if (text.length > MAX_NODE_DEBUG_LENGTH) {
    text = text.substr(0, MAX_NODE_DEBUG_LENGTH);
  }

  return `(node kind ${getSyntaxKindString(node.kind)}, content '${text}') at ${sourceFile.fileName}:${
    position.line + 1
  }:${position.character + 1}`;
}

export function resolveTypeOfExpression(expression: ts.Expression, typeChecker: ts.TypeChecker): ResolvedType {
  let effectiveType: SymbolType | undefined;
  let effectiveObjectInfo: SymbolObjectInfo | undefined;
  let isNullable = false;

  const resolvedSymbols = resolvedSymbolsOfExpression(expression, typeChecker);

  for (const resolvedSymbol of resolvedSymbols) {
    if (resolvedSymbol.type === SymbolType.any) {
      effectiveType = SymbolType.any;
    } else if (resolvedSymbol.type === SymbolType.undefined || resolvedSymbol.type === SymbolType.null) {
      isNullable = true;
    } else {
      if (effectiveType === undefined) {
        effectiveType = resolvedSymbol.type;
        effectiveObjectInfo = resolvedSymbol.objectInfo;
      } else {
        effectiveType = SymbolType.any;
      }
    }
  }

  if (effectiveType === undefined) {
    effectiveType = SymbolType.any;
  }

  if (effectiveType === SymbolType.any) {
    isNullable = true;
    effectiveObjectInfo = undefined;
  }

  return { effectiveType, isNullable, resolvedSymbols, effectiveObjectInfo };
}

export class Transpiler {
  readonly diagnostics: ts.Diagnostic[] = [];
  readonly program: ts.Program;
  readonly sourceFile: ts.SourceFile;

  private outputText: string | undefined;
  private sourceMapText: string | undefined;

  constructor(input: string, transpileOptions: ts.TranspileOptions) {
    const options = { ...(transpileOptions.compilerOptions ?? {}) };
    // mix in default options

    const defaultOptions = ts.getDefaultCompilerOptions();
    for (const key in defaultOptions) {
      if (defaultOptions[key] !== undefined && options[key] === undefined) {
        options[key] = defaultOptions[key];
      }
    }

    // transpileModule does not write anything to disk so there is no need to verify that there are no conflicts between input and output paths.
    options.suppressOutputPathCheck = true;

    // Filename can be non-ts file.
    options.allowNonTsExtensions = true;

    // if jsx is specified then treat file as .tsx
    const inputFileName =
      transpileOptions.fileName ||
      (transpileOptions.compilerOptions && transpileOptions.compilerOptions.jsx ? 'module.tsx' : 'module.ts');
    this.sourceFile = ts.createSourceFile(inputFileName, input, options.target!); // TODO: GH#18217
    if (transpileOptions.moduleName) {
      this.sourceFile.moduleName = transpileOptions.moduleName;
    }

    const newLine = '\n';

    // Create a compilerHost object to allow the compiler to read and write files
    const compilerHost: ts.CompilerHost = {
      getSourceFile: (fileName) => (fileName === inputFileName ? this.sourceFile : undefined),
      writeFile: (name, text) => {
        if (name.endsWith('.map')) {
          this.sourceMapText = text;
        } else {
          this.outputText = text;
        }
      },
      getDefaultLibFileName: () => 'lib.d.ts',
      useCaseSensitiveFileNames: () => false,
      getCanonicalFileName: (fileName) => fileName,
      getCurrentDirectory: () => '',
      getNewLine: () => newLine,
      fileExists: (fileName): boolean => fileName === inputFileName,
      readFile: () => '',
      directoryExists: () => true,
      getDirectories: () => [],
    };

    this.program = ts.createProgram([inputFileName], options, compilerHost);

    if (transpileOptions.reportDiagnostics) {
      this.diagnostics.push(...this.program.getSyntacticDiagnostics(this.sourceFile));
      this.diagnostics.push(...this.program.getOptionsDiagnostics());
    }
  }

  transpile(transformers?: ts.CustomTransformers): ts.TranspileOutput {
    // Emit
    this.program.emit(
      /*targetSourceFile*/ undefined,
      /*writeFile*/ undefined,
      /*cancellationToken*/ undefined,
      /*emitOnlyDtsFiles*/ undefined,
      transformers,
    );

    if (this.outputText === undefined) {
      throw new Error('Output generation failed');
    }

    return { outputText: this.outputText, diagnostics: this.diagnostics, sourceMapText: this.sourceMapText };
  }
}

export const enum OuterExpressionKinds {
  Parentheses = 1 << 0,
  TypeAssertions = 1 << 1,
  NonNullAssertions = 1 << 2,
  PartiallyEmittedExpressions = 1 << 3,

  Assertions = TypeAssertions | NonNullAssertions,
  All = Parentheses | Assertions | PartiallyEmittedExpressions,

  ExcludeJSDocTypeAssertion = 1 << 4,
}

export type OuterExpression =
  | ts.ParenthesizedExpression
  | ts.TypeAssertion
  | ts.AsExpression
  | ts.NonNullExpression
  | ts.PartiallyEmittedExpression;

export function isOuterExpression(node: ts.Node, kinds = OuterExpressionKinds.All): node is OuterExpression {
  switch (node.kind) {
    case ts.SyntaxKind.ParenthesizedExpression:
      return (kinds & OuterExpressionKinds.Parentheses) !== 0;
    case ts.SyntaxKind.TypeAssertionExpression:
    case ts.SyntaxKind.AsExpression:
      return (kinds & OuterExpressionKinds.TypeAssertions) !== 0;
    case ts.SyntaxKind.NonNullExpression:
      return (kinds & OuterExpressionKinds.NonNullAssertions) !== 0;
    case ts.SyntaxKind.PartiallyEmittedExpression:
      return (kinds & OuterExpressionKinds.PartiallyEmittedExpressions) !== 0;
  }
  return false;
}

export function skipOuterExpressions(node: ts.Node, kinds = OuterExpressionKinds.All) {
  while (isOuterExpression(node, kinds)) {
    node = node.expression;
  }
  return node;
}

export function skipParentheses(node: ts.Node, excludeJSDocTypeAssertions?: boolean): ts.Node {
  return skipOuterExpressions(node, OuterExpressionKinds.Parentheses);
}

export function isSuperCall(n: ts.Node): n is ts.SuperCall {
  return (
    n.kind === ts.SyntaxKind.CallExpression && (n as ts.CallExpression).expression.kind === ts.SyntaxKind.SuperKeyword
  );
}

export function getSuperCallFromStatement(statement: ts.Statement): ts.Expression | undefined {
  if (!ts.isExpressionStatement(statement)) {
    return undefined;
  }

  const expression = skipParentheses(statement.expression);
  return isSuperCall(expression) ? expression : undefined;
}

export function findSuperStatementIndex(
  statements: ts.NodeArray<ts.Statement>,
  indexAfterLastPrologueStatement: number,
): number {
  for (let i = indexAfterLastPrologueStatement; i < statements.length; i += 1) {
    const statement = statements[i];

    if (getSuperCallFromStatement(statement)) {
      return i;
    }
  }

  return -1;
}

// If the (potentially composite) statement contains a super call
export function statementHasSuperCall(node: ts.Node): boolean {
  if (isSuperCall(node)) {
    return true;
  }
  for (const child of node.getChildren()) {
    if (statementHasSuperCall(child)) {
      return true;
    }
  }
  return false;
}
