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

    const declarations = symbol.getDeclarations();
    if (!declarations) {
      return undefined;
    }

    for (const declaration of declarations) {
      const version = this.getVersion(declaration);
      if (version !== undefined) {
        return version;
      }
    }

    return undefined;
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

    if (this.isFunctionLikeDeclaration(node)) {
      this.visitFunctionLikeDeclaration(node, currentVersion);
      return;
    }

    if (ts.isInterfaceDeclaration(node) || ts.isClassDeclaration(node)) {
      this.validateContainerDeclaration(node);
    }

    if (ts.isPropertyAccessExpression(node) && !this.isCalleePropertyAccess(node)) {
      this.validatePropertyAccess(node, currentVersion);
    }

    if (ts.isCallExpression(node)) {
      this.validateCallExpression(node, currentVersion);
    }

    ts.forEachChild(node, (child) => this.visit(child, currentVersion));
  }

  private visitIfStatement(node: ts.IfStatement, currentVersion: number | undefined): void {
    const conditionVersion = this.visitVersionCondition(node.expression, currentVersion);

    const thenVersion = this.mergeVersions(currentVersion, conditionVersion);
    this.visit(node.thenStatement, thenVersion);

    if (node.elseStatement) {
      this.visit(node.elseStatement, currentVersion);
    }
  }

  private visitVersionCondition(node: ts.Expression, currentVersion: number | undefined): number | undefined {
    if (ts.isParenthesizedExpression(node)) {
      return this.visitVersionCondition(node.expression, currentVersion);
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

    this.visit(node, currentVersion);
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
    const effectiveDeclarationVersion = this.mergeVersions(this.nativeApiMinVersion, declaredVersion);
    this.validateSignature(node, effectiveDeclarationVersion);

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

    if (ts.isMethodDeclaration(node) || ts.isGetAccessorDeclaration(node) || ts.isSetAccessorDeclaration(node)) {
      return this.getVersion(node);
    }

    if (ts.isFunctionDeclaration(node) || ts.isClassDeclaration(node) || ts.isInterfaceDeclaration(node)) {
      return this.getVersion(node);
    }

    if ((ts.isFunctionExpression(node) || ts.isArrowFunction(node)) && ts.isVariableDeclaration(node.parent)) {
      return this.getVersion(node.parent);
    }

    return this.getVersion(node);
  }

  private isFunctionLikeDeclaration(node: ts.Node): node is ts.FunctionLikeDeclaration {
    return (
      ts.isFunctionDeclaration(node) ||
      ts.isMethodDeclaration(node) ||
      ts.isGetAccessorDeclaration(node) ||
      ts.isSetAccessorDeclaration(node) ||
      ts.isFunctionExpression(node) ||
      ts.isArrowFunction(node)
    );
  }

  private validateContainerDeclaration(node: ts.InterfaceDeclaration | ts.ClassDeclaration): void {
    const containerVersion = this.mergeVersions(this.nativeApiMinVersion, this.getVersion(node));

    if (node.heritageClauses) {
      for (const heritageClause of node.heritageClauses) {
        for (const type of heritageClause.types) {
          this.validateTypeNode(type, containerVersion, type);
        }
      }
    }

    for (const member of node.members) {
      if (this.isFunctionLikeDeclaration(member)) {
        continue;
      }

      const declaredMemberVersion = this.getVersion(member);
      const memberVersion =
        this.nativeApiMinVersion === undefined
          ? declaredMemberVersion ?? containerVersion
          : this.mergeVersions(containerVersion, declaredMemberVersion);
      if (this.isSignatureMember(member)) {
        this.validateSignature(member, memberVersion);
        continue;
      }

      if (ts.isPropertySignature(member) || ts.isPropertyDeclaration(member)) {
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

  private validateCallExpression(node: ts.CallExpression, currentVersion: number | undefined): void {
    if (this.isVersionIntrinsicCall(node)) {
      return;
    }

    const requiredVersion = this.getRequiredVersionForCall(node);
    if (requiredVersion !== undefined) {
      this.validateVersionedUse(node.expression, currentVersion, requiredVersion, 'Function call');
    }
  }

  private isCalleePropertyAccess(node: ts.PropertyAccessExpression): boolean {
    return ts.isCallExpression(node.parent) && node.parent.expression === node;
  }

  private getRequiredVersionForCall(node: ts.CallExpression): number | undefined {
    let requiredVersion = this.getVersionFromSymbol(this.typeChecker.getSymbolAtLocation(node.expression));

    const signature = this.typeChecker.getResolvedSignature(node);
    if (!signature) {
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
