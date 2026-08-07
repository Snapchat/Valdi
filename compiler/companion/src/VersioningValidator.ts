import * as ts from 'typescript';
import { hasExportModuleAnnotation, hasNativeExportAnnotation } from './AST';
import { Diagnostic } from './protocol';
import { getNodeComments, isNodeExported } from './TSUtils';

const PLACEHOLDER_VERSION = Number.MAX_SAFE_INTEGER;
const PLACEHOLDER_VERSION_TEXT = '__PLACEHOLDER__';
const VERSION_ANNOTATION_REGEX = /@Version\s*\(\s*(\d+|__PLACEHOLDER__)\s*\)/;
const VERSION_INTRINSIC_NAME = 'isVersionAtLeast';

export class VersioningValidator {
  private readonly versionCache = new WeakMap<ts.Node, number | undefined>();
  private readonly nativeContractCache = new WeakMap<ts.Node, boolean>();
  private readonly exportModuleCache = new WeakMap<ts.SourceFile, boolean>();
  private readonly diagnostics: Diagnostic[] = [];

  constructor(
    private readonly sourceFile: ts.SourceFile,
    private readonly typeChecker: ts.TypeChecker,
    private readonly makeDiagnostic: (sourceFile: ts.SourceFile, node: ts.Node, text: string) => Diagnostic,
    private readonly nativeApiMinVersion: number | undefined,
  ) {}

  validate(): Diagnostic[] {
    this.visit(this.sourceFile, this.nativeApiMinVersion);
    return this.diagnostics;
  }

  private getVersion(node: ts.Node | undefined): number | undefined {
    if (!node) {
      return undefined;
    }

    if (this.versionCache.has(node)) {
      return this.versionCache.get(node);
    }

    let version = this.parseVersion(node);
    if (version === undefined && ts.isVariableDeclaration(node) && ts.isVariableStatement(node.parent.parent)) {
      version = this.parseVersion(node.parent.parent);
    }
    if (
      version === undefined &&
      this.nativeApiMinVersion !== undefined &&
      this.isImplicitlyVersionedNativeDeclaration(node)
    ) {
      version = this.nativeApiMinVersion;
    }

    this.versionCache.set(node, version);
    return version;
  }

  private isImplicitlyVersionedNativeDeclaration(node: ts.Node): boolean {
    if (this.nativeContractCache.has(node)) {
      return this.nativeContractCache.get(node) ?? false;
    }

    const annotationNode = this.getAnnotationNode(node);
    let isNativeContract = hasNativeExportAnnotation(getNodeComments(annotationNode)?.text ?? '');

    if (!isNativeContract) {
      const containingContract = this.getContainingContractDeclaration(annotationNode);
      if (containingContract) {
        isNativeContract = this.isImplicitlyVersionedNativeDeclaration(containingContract);
      } else if (this.sourceFileHasExportModuleAnnotation(annotationNode.getSourceFile())) {
        const topLevelDeclaration = this.getTopLevelDeclaration(annotationNode);
        isNativeContract = topLevelDeclaration !== undefined && isNodeExported(topLevelDeclaration);
      }
    }

    this.nativeContractCache.set(node, isNativeContract);
    return isNativeContract;
  }

  private sourceFileHasExportModuleAnnotation(sourceFile: ts.SourceFile): boolean {
    const cached = this.exportModuleCache.get(sourceFile);
    if (cached !== undefined) {
      return cached;
    }

    const hasAnnotation = sourceFile.statements.some((statement) =>
      hasExportModuleAnnotation(getNodeComments(statement)?.text ?? ''),
    );
    this.exportModuleCache.set(sourceFile, hasAnnotation);
    return hasAnnotation;
  }

  private getAnnotationNode(node: ts.Node): ts.Node {
    if (ts.isVariableDeclaration(node) && ts.isVariableStatement(node.parent.parent)) {
      return node.parent.parent;
    }

    return node;
  }

  private getContainingContractDeclaration(
    node: ts.Node,
  ): ts.ClassDeclaration | ts.InterfaceDeclaration | ts.EnumDeclaration | undefined {
    const parent = node.parent;
    if (
      parent &&
      (ts.isClassDeclaration(parent) || ts.isInterfaceDeclaration(parent) || ts.isEnumDeclaration(parent))
    ) {
      return parent;
    }

    return undefined;
  }

  private getTopLevelDeclaration(node: ts.Node): ts.Statement | undefined {
    let current = node;
    while (current.parent && !ts.isSourceFile(current.parent)) {
      current = current.parent;
    }

    return ts.isStatement(current) ? current : undefined;
  }

  private parseVersion(node: ts.Node): number | undefined {
    const comments = getNodeComments(node);
    if (!comments) {
      return undefined;
    }

    const match = comments.text.match(VERSION_ANNOTATION_REGEX);
    if (!match) {
      return undefined;
    }

    if (match[1] === PLACEHOLDER_VERSION_TEXT) {
      return PLACEHOLDER_VERSION;
    }

    return Number(match[1]);
  }

  private formatVersion(version: number): string {
    if (version === PLACEHOLDER_VERSION) {
      return PLACEHOLDER_VERSION_TEXT;
    }

    return String(version);
  }

  private getVersionFromSymbol(symbol: ts.Symbol | undefined): number | undefined {
    if (!symbol) {
      return undefined;
    }

    if ((symbol.flags & ts.SymbolFlags.Alias) !== 0) {
      symbol = this.typeChecker.getAliasedSymbol(symbol);
    }

    const declarations = symbol.getDeclarations();
    if (!declarations) {
      return undefined;
    }

    let requiredVersion: number | undefined;
    for (const declaration of declarations) {
      requiredVersion = this.mergeVersions(requiredVersion, this.getDeclarationVersion(declaration));
    }

    return requiredVersion;
  }

  private isVersionSatisfied(currentVersion: number | undefined, requiredVersion: number): boolean {
    return currentVersion !== undefined && currentVersion >= requiredVersion;
  }

  private validateVersionedUse(
    node: ts.Node,
    currentVersion: number | undefined,
    requiredVersion: number,
    label: string,
  ) {
    if (this.isVersionSatisfied(currentVersion, requiredVersion)) {
      return;
    }

    this.diagnostics.push(
      this.makeDiagnostic(
        this.sourceFile,
        node,
        `${label} requires @Version(${this.formatVersion(
          requiredVersion,
        )}) or an enclosing isVersionAtLeast(${this.formatVersion(requiredVersion)}) block`,
      ),
    );
  }

  private visit(node: ts.Node, currentVersion: number | undefined): void {
    if (this.isVersionIntrinsicCall(node) && ts.isCallExpression(node)) {
      this.validateVersionIntrinsicCall(node);
    }

    if (ts.isIfStatement(node)) {
      this.visitIfStatement(node, currentVersion);
      return;
    }

    if (
      ts.isBinaryExpression(node) &&
      (node.operatorToken.kind === ts.SyntaxKind.AmpersandAmpersandToken ||
        node.operatorToken.kind === ts.SyntaxKind.BarBarToken)
    ) {
      this.visitVersionCondition(node, currentVersion);
      return;
    }

    if (ts.isConditionalExpression(node)) {
      this.visitConditionalExpression(node, currentVersion);
      return;
    }

    if (ts.isSourceFile(node) || ts.isBlock(node)) {
      this.visitStatements(node.statements, currentVersion);
      return;
    }

    if (this.isFunctionLikeDeclaration(node)) {
      this.visitFunctionLikeDeclaration(node, currentVersion);
      return;
    }

    if (ts.isInterfaceDeclaration(node) || ts.isClassDeclaration(node)) {
      this.validateContainerDeclaration(node);
      const containerVersion = this.mergeVersions(currentVersion, this.getDeclarationVersion(node));
      ts.forEachChild(node, (child) => {
        const isEagerStaticMember =
          ts.isClassDeclaration(node) &&
          (ts.isClassStaticBlockDeclaration(child) ||
            (ts.isPropertyDeclaration(child) && (ts.getCombinedModifierFlags(child) & ts.ModifierFlags.Static) !== 0));
        this.visit(child, isEagerStaticMember ? currentVersion : containerVersion);
      });
      return;
    }

    if (ts.isPropertyAccessExpression(node) && !this.isCalleePropertyAccess(node)) {
      this.validatePropertyAccess(node, currentVersion);
    }

    if (ts.isBindingElement(node) && ts.isObjectBindingPattern(node.parent)) {
      this.validateObjectBindingElement(node, currentVersion);
    }

    if (
      ts.isBinaryExpression(node) &&
      node.operatorToken.kind === ts.SyntaxKind.EqualsToken &&
      ts.isObjectLiteralExpression(node.left)
    ) {
      this.validateObjectDestructuringAssignment(
        node.left,
        this.typeChecker.getTypeAtLocation(node.right),
        currentVersion,
      );
    }

    if (ts.isCallExpression(node)) {
      this.validateCallExpression(node, currentVersion);
    }

    if (ts.isNewExpression(node)) {
      this.validateNewExpression(node, currentVersion);
    }

    if (ts.isJsxOpeningElement(node) || ts.isJsxSelfClosingElement(node)) {
      this.validateJsxComponent(node, currentVersion);
    }

    ts.forEachChild(node, (child) => this.visit(child, currentVersion));
  }

  private visitIfStatement(node: ts.IfStatement, currentVersion: number | undefined): void {
    const conditionVersion = this.visitVersionCondition(node.expression, currentVersion);

    const thenVersion = this.mergeVersions(currentVersion, conditionVersion);
    this.visit(node.thenStatement, thenVersion);

    if (node.elseStatement) {
      const elseVersion = this.mergeVersions(currentVersion, this.getVersionWhenConditionIsFalse(node.expression));
      this.visit(node.elseStatement, elseVersion);
    }
  }

  private visitConditionalExpression(node: ts.ConditionalExpression, currentVersion: number | undefined): void {
    const conditionVersion = this.visitVersionCondition(node.condition, currentVersion);
    const thenVersion = this.mergeVersions(currentVersion, conditionVersion);
    const elseVersion = this.mergeVersions(currentVersion, this.getVersionWhenConditionIsFalse(node.condition));

    this.visit(node.whenTrue, thenVersion);
    this.visit(node.whenFalse, elseVersion);
  }

  private visitStatements(statements: ts.NodeArray<ts.Statement>, currentVersion: number | undefined): void {
    let statementVersion = currentVersion;
    for (const statement of statements) {
      this.visit(statement, statementVersion);
      statementVersion = this.getVersionAfterStatement(statement, statementVersion);
    }
  }

  private getVersionAfterStatement(statement: ts.Statement, currentVersion: number | undefined): number | undefined {
    if (!ts.isIfStatement(statement)) {
      return currentVersion;
    }

    const thenTerminates = this.statementAlwaysTerminates(statement.thenStatement);
    const elseTerminates =
      statement.elseStatement !== undefined && this.statementAlwaysTerminates(statement.elseStatement);

    if (thenTerminates && !elseTerminates) {
      return this.mergeVersions(currentVersion, this.getVersionWhenConditionIsFalse(statement.expression));
    }

    if (elseTerminates && !thenTerminates) {
      return this.mergeVersions(currentVersion, this.getVersionWhenConditionIsTrue(statement.expression));
    }

    return currentVersion;
  }

  private statementAlwaysTerminates(statement: ts.Statement): boolean {
    if (ts.isReturnStatement(statement) || ts.isThrowStatement(statement)) {
      return true;
    }

    if (ts.isBlock(statement)) {
      return statement.statements.some((child) => this.statementAlwaysTerminates(child));
    }

    if (ts.isIfStatement(statement) && statement.elseStatement) {
      return (
        this.statementAlwaysTerminates(statement.thenStatement) &&
        this.statementAlwaysTerminates(statement.elseStatement)
      );
    }

    return false;
  }

  private visitVersionCondition(node: ts.Expression, currentVersion: number | undefined): number | undefined {
    if (ts.isParenthesizedExpression(node)) {
      return this.visitVersionCondition(node.expression, currentVersion);
    }

    if (ts.isPrefixUnaryExpression(node) && node.operator === ts.SyntaxKind.ExclamationToken) {
      this.visitVersionCondition(node.operand, currentVersion);
      return this.getVersionWhenConditionIsFalse(node.operand);
    }

    if (this.isVersionIntrinsicCall(node) && ts.isCallExpression(node)) {
      this.visit(node, currentVersion);
      return this.getVersionIntrinsicArgument(node);
    }

    if (ts.isBinaryExpression(node) && node.operatorToken.kind === ts.SyntaxKind.AmpersandAmpersandToken) {
      const leftVersion = this.visitVersionCondition(node.left, currentVersion);
      const rightVersion = this.visitVersionCondition(node.right, this.mergeVersions(currentVersion, leftVersion));
      return this.mergeVersions(leftVersion, rightVersion);
    }

    if (ts.isBinaryExpression(node) && node.operatorToken.kind === ts.SyntaxKind.BarBarToken) {
      const leftVersion = this.visitVersionCondition(node.left, currentVersion);
      const rightVersion = this.visitVersionCondition(
        node.right,
        this.mergeVersions(currentVersion, this.getVersionWhenConditionIsFalse(node.left)),
      );
      return leftVersion === undefined || rightVersion === undefined ? undefined : Math.min(leftVersion, rightVersion);
    }

    this.visit(node, currentVersion);
    return undefined;
  }

  private getVersionWhenConditionIsTrue(node: ts.Expression): number | undefined {
    if (ts.isParenthesizedExpression(node)) {
      return this.getVersionWhenConditionIsTrue(node.expression);
    }

    if (ts.isPrefixUnaryExpression(node) && node.operator === ts.SyntaxKind.ExclamationToken) {
      return this.getVersionWhenConditionIsFalse(node.operand);
    }

    if (this.isVersionIntrinsicCall(node)) {
      return this.getVersionIntrinsicArgument(node);
    }

    if (ts.isBinaryExpression(node) && node.operatorToken.kind === ts.SyntaxKind.AmpersandAmpersandToken) {
      return this.mergeVersions(
        this.getVersionWhenConditionIsTrue(node.left),
        this.getVersionWhenConditionIsTrue(node.right),
      );
    }

    if (ts.isBinaryExpression(node) && node.operatorToken.kind === ts.SyntaxKind.BarBarToken) {
      const leftVersion = this.getVersionWhenConditionIsTrue(node.left);
      const rightVersion = this.getVersionWhenConditionIsTrue(node.right);
      return leftVersion === undefined || rightVersion === undefined ? undefined : Math.min(leftVersion, rightVersion);
    }

    return undefined;
  }

  private getVersionWhenConditionIsFalse(node: ts.Expression): number | undefined {
    if (ts.isParenthesizedExpression(node)) {
      return this.getVersionWhenConditionIsFalse(node.expression);
    }

    if (ts.isPrefixUnaryExpression(node) && node.operator === ts.SyntaxKind.ExclamationToken) {
      return this.getVersionWhenConditionIsTrue(node.operand);
    }

    if (ts.isBinaryExpression(node) && node.operatorToken.kind === ts.SyntaxKind.BarBarToken) {
      return this.mergeVersions(
        this.getVersionWhenConditionIsFalse(node.left),
        this.getVersionWhenConditionIsFalse(node.right),
      );
    }

    return undefined;
  }

  private mergeVersions(left: number | undefined, right: number | undefined): number | undefined {
    if (left === undefined) {
      return right;
    }

    if (right === undefined) {
      return left;
    }

    return Math.max(left, right);
  }

  private visitFunctionLikeDeclaration(node: ts.FunctionLikeDeclaration, currentVersion: number | undefined): void {
    const declaredVersion = this.getDeclarationVersion(node);
    const effectiveDeclarationVersion = this.mergeVersions(currentVersion, declaredVersion);
    this.validateSignature(node, effectiveDeclarationVersion);

    for (const parameter of node.parameters) {
      if (ts.isObjectBindingPattern(parameter.name) || ts.isArrayBindingPattern(parameter.name)) {
        this.visit(parameter.name, effectiveDeclarationVersion);
      }
    }

    if (node.body) {
      const bodyVersion =
        this.nativeApiMinVersion === undefined
          ? declaredVersion ?? currentVersion
          : this.mergeVersions(currentVersion, declaredVersion);
      this.visit(node.body, bodyVersion);
    }
  }

  private getDeclarationVersion(node: ts.Node | undefined): number | undefined {
    if (!node) {
      return undefined;
    }

    if (
      (ts.isFunctionExpression(node) || ts.isArrowFunction(node)) &&
      (ts.isVariableDeclaration(node.parent) || ts.isPropertyDeclaration(node.parent))
    ) {
      node = node.parent;
    }

    const declaredVersion = this.getVersion(node);
    const containingDeclaration = this.getContainingContractDeclaration(this.getAnnotationNode(node));
    return this.mergeVersions(declaredVersion, this.getVersion(containingDeclaration));
  }

  private isFunctionLikeDeclaration(node: ts.Node): node is ts.FunctionLikeDeclaration {
    return (
      ts.isFunctionDeclaration(node) ||
      ts.isMethodDeclaration(node) ||
      ts.isConstructorDeclaration(node) ||
      ts.isGetAccessorDeclaration(node) ||
      ts.isSetAccessorDeclaration(node) ||
      ts.isFunctionExpression(node) ||
      ts.isArrowFunction(node)
    );
  }

  private validateContainerDeclaration(node: ts.InterfaceDeclaration | ts.ClassDeclaration): void {
    const containerVersion = this.mergeVersions(this.nativeApiMinVersion, this.getVersion(node));

    if (ts.isClassDeclaration(node) && node.heritageClauses) {
      for (const heritageClause of node.heritageClauses) {
        if (heritageClause.token !== ts.SyntaxKind.ExtendsKeyword) {
          continue;
        }

        for (const type of heritageClause.types) {
          this.validateTypeNode(type, containerVersion, type);
        }
      }
    }

    for (const member of node.members) {
      if (this.isFunctionLikeDeclaration(member)) {
        continue;
      }

      const memberVersion = this.mergeVersions(containerVersion, this.getDeclarationVersion(member));
      if (this.isSignatureMember(member)) {
        this.validateSignature(member, memberVersion);
        continue;
      }

      if (
        (ts.isPropertySignature(member) || ts.isPropertyDeclaration(member)) &&
        this.parseVersion(member) !== undefined
      ) {
        this.validateTypeNode(member.type, memberVersion, member.type ?? member);
      }
    }
  }

  private isSignatureMember(node: ts.Node): node is ts.SignatureDeclaration {
    return (
      ts.isMethodSignature(node) ||
      ts.isCallSignatureDeclaration(node) ||
      ts.isConstructSignatureDeclaration(node) ||
      ts.isIndexSignatureDeclaration(node)
    );
  }

  private validateSignature(node: ts.SignatureDeclaration, declarationVersion: number | undefined): void {
    for (const parameter of node.parameters) {
      this.validateTypeNode(parameter.type, declarationVersion, parameter.type ?? parameter);
    }

    this.validateTypeNode(node.type, declarationVersion, node.type ?? node);
  }

  private validateTypeNode(
    typeNode: ts.TypeNode | undefined,
    declarationVersion: number | undefined,
    diagnosticNode: ts.Node,
  ): void {
    if (!typeNode) {
      return;
    }

    const requiredVersion = this.getRequiredVersionForTypeNode(typeNode);
    if (requiredVersion !== undefined && !this.isVersionSatisfied(declarationVersion, requiredVersion)) {
      this.diagnostics.push(
        this.makeDiagnostic(
          this.sourceFile,
          diagnosticNode,
          `Type '${typeNode.getText(this.sourceFile)}' requires @Version(${this.formatVersion(
            requiredVersion,
          )}) on the containing declaration`,
        ),
      );
    }
  }

  private getRequiredVersionForTypeNode(typeNode: ts.TypeNode): number | undefined {
    let requiredVersion: number | undefined;

    const recordVersion = (version: number | undefined) => {
      if (version === undefined) {
        return;
      }
      requiredVersion = Math.max(requiredVersion ?? version, version);
    };

    const visitType = (node: ts.Node) => {
      if (ts.isTypeReferenceNode(node)) {
        recordVersion(this.getVersionFromSymbol(this.typeChecker.getSymbolAtLocation(node.typeName)));
      } else if (ts.isExpressionWithTypeArguments(node)) {
        recordVersion(this.getVersionFromSymbol(this.typeChecker.getSymbolAtLocation(node.expression)));
      } else if (ts.isTypeQueryNode(node)) {
        recordVersion(this.getVersionFromSymbol(this.typeChecker.getSymbolAtLocation(node.exprName)));
      }

      ts.forEachChild(node, visitType);
    };

    visitType(typeNode);
    return requiredVersion;
  }

  private validatePropertyAccess(node: ts.PropertyAccessExpression, currentVersion: number | undefined): void {
    const requiredVersion = this.getVersionFromSymbol(this.typeChecker.getSymbolAtLocation(node.name));
    if (requiredVersion !== undefined) {
      this.validateVersionedUse(node.name, currentVersion, requiredVersion, `Property '${node.name.text}'`);
    }
  }

  private validateObjectBindingElement(node: ts.BindingElement, currentVersion: number | undefined): void {
    if (node.dotDotDotToken) {
      return;
    }

    const propertyName = node.propertyName ?? node.name;
    const propertyNameText = this.getDestructuredPropertyName(propertyName);
    if (propertyNameText === undefined) {
      return;
    }

    const sourceType = this.typeChecker.getTypeAtLocation(node.parent);
    const symbol = this.typeChecker.getPropertyOfType(sourceType, propertyNameText);
    const requiredVersion = this.getVersionFromSymbol(symbol);
    if (requiredVersion !== undefined) {
      this.validateVersionedUse(propertyName, currentVersion, requiredVersion, `Property '${propertyNameText}'`);
    }
  }

  private validateObjectDestructuringAssignment(
    pattern: ts.ObjectLiteralExpression,
    sourceType: ts.Type,
    currentVersion: number | undefined,
  ): void {
    for (const element of pattern.properties) {
      if (!ts.isShorthandPropertyAssignment(element) && !ts.isPropertyAssignment(element)) {
        continue;
      }

      const propertyNameText = this.getDestructuredPropertyName(element.name);
      if (propertyNameText === undefined) {
        continue;
      }

      const symbol = this.typeChecker.getPropertyOfType(sourceType, propertyNameText);
      const requiredVersion = this.getVersionFromSymbol(symbol);
      if (requiredVersion !== undefined) {
        this.validateVersionedUse(element.name, currentVersion, requiredVersion, `Property '${propertyNameText}'`);
      }

      if (ts.isPropertyAssignment(element) && ts.isObjectLiteralExpression(element.initializer) && symbol) {
        const propertyType = this.typeChecker.getTypeOfSymbolAtLocation(symbol, element.name);
        this.validateObjectDestructuringAssignment(element.initializer, propertyType, currentVersion);
      }
    }
  }

  private getDestructuredPropertyName(node: ts.PropertyName | ts.BindingName): string | undefined {
    if (ts.isIdentifier(node) || ts.isStringLiteralLike(node) || ts.isNumericLiteral(node)) {
      return node.text;
    }

    if (!ts.isComputedPropertyName(node)) {
      return undefined;
    }

    const computedType = this.typeChecker.getTypeAtLocation(node.expression);
    if (computedType.isStringLiteral()) {
      return computedType.value;
    }
    if (computedType.isNumberLiteral()) {
      return String(computedType.value);
    }

    return undefined;
  }

  private validateCallExpression(node: ts.CallExpression, currentVersion: number | undefined): void {
    if (this.isVersionIntrinsicCall(node)) {
      return;
    }

    const requiredVersion = this.getRequiredVersionForInvocation(node);
    if (requiredVersion !== undefined) {
      this.validateVersionedUse(node.expression, currentVersion, requiredVersion, 'Function call');
    }
  }

  private validateNewExpression(node: ts.NewExpression, currentVersion: number | undefined): void {
    const requiredVersion = this.getRequiredVersionForInvocation(node);
    if (requiredVersion !== undefined) {
      this.validateVersionedUse(node.expression, currentVersion, requiredVersion, 'Constructor call');
    }
  }

  private validateJsxComponent(node: ts.JsxOpeningLikeElement, currentVersion: number | undefined): void {
    const requiredVersion = this.getRequiredVersionForInvocation(node);
    if (requiredVersion !== undefined) {
      this.validateVersionedUse(
        node.tagName,
        currentVersion,
        requiredVersion,
        `Component '${node.tagName.getText(this.sourceFile)}'`,
      );
    }
  }

  private isCalleePropertyAccess(node: ts.PropertyAccessExpression): boolean {
    return (
      ((ts.isCallExpression(node.parent) || ts.isNewExpression(node.parent)) && node.parent.expression === node) ||
      ((ts.isJsxOpeningElement(node.parent) || ts.isJsxSelfClosingElement(node.parent)) && node.parent.tagName === node)
    );
  }

  private getRequiredVersionForInvocation(
    node: ts.CallExpression | ts.NewExpression | ts.JsxOpeningLikeElement,
  ): number | undefined {
    const expression = ts.isJsxOpeningLikeElement(node) ? node.tagName : node.expression;
    let requiredVersion = this.getVersionFromSymbol(this.typeChecker.getSymbolAtLocation(expression));

    const signature = this.typeChecker.getResolvedSignature(node);
    if (!signature?.declaration || signature.declaration.pos < 0) {
      return requiredVersion;
    }

    const declarationVersion = this.getDeclarationVersion(signature.declaration);
    if (declarationVersion !== undefined) {
      requiredVersion = Math.max(requiredVersion ?? declarationVersion, declarationVersion);
    }

    return requiredVersion;
  }

  private isVersionIntrinsicCall(node: ts.Node): boolean {
    return (
      ts.isCallExpression(node) && ts.isIdentifier(node.expression) && node.expression.text === VERSION_INTRINSIC_NAME
    );
  }

  private getVersionIntrinsicArgument(node: ts.Expression): number | undefined {
    if (!this.isVersionIntrinsicCall(node) || !ts.isCallExpression(node)) {
      return undefined;
    }

    if (node.arguments.length !== 1) {
      return undefined;
    }

    const argument = node.arguments[0];
    if (ts.isNumericLiteral(argument)) {
      return Number(argument.text);
    }
    if (ts.isIdentifier(argument) && argument.text === PLACEHOLDER_VERSION_TEXT) {
      return PLACEHOLDER_VERSION;
    }
    return undefined;
  }

  private validateVersionIntrinsicCall(node: ts.CallExpression): void {
    if (this.getVersionIntrinsicArgument(node) === undefined) {
      this.diagnostics.push(
        this.makeDiagnostic(
          this.sourceFile,
          node,
          'isVersionAtLeast expects exactly one numeric literal or __PLACEHOLDER__ argument',
        ),
      );
    }
  }
}
