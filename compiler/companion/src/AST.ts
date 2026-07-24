import * as ts from 'typescript';
import {
  getNodeComments,
  getNodeDebugDescription,
  getRootNodesToDump,
  isNodeExported,
  NodeComments,
  NodeToDump,
} from './TSUtils';

export type ASTNodeKind = 'interface';

export namespace AST {
  export interface Node {
    start: number;
    end: number;
  }

  export interface NamedNode extends Node {
    name: string;
  }

  export interface Comments {
    text: string;
    start: number;
    end: number;
  }

  export interface EnumMember extends NamedNode {
    numberValue?: string;
    stringValue?: string;
    leadingComments?: Comments;
  }

  export interface Enum extends NamedNode {
    members: EnumMember[];
  }

  export interface FunctionType {
    parameters: PropertyLikeDeclaration[];
    returnValue: Type;
  }

  export interface TypeArgument {
    type: Type;
  }

  export interface Type {
    name: string;
    leadingComments?: Comments;
    function?: FunctionType;
    unions?: Type[];
    intersections?: Type[];
    typeReferenceIndex?: number;
    array?: Type;
    typeArguments?: TypeArgument[];
    isTypeParameter?: boolean;
  }

  export interface SuperTypeClause extends Node {
    isImplements: boolean;
    type: Type;
  }

  export interface PropertyLikeDeclaration extends NamedNode {
    isOptional: boolean;
    type: Type;
    leadingComments?: Comments;
  }

  export interface TypeParameterLikeDeclaration extends NamedNode {}

  export interface Interface extends NamedNode {
    members: PropertyLikeDeclaration[];
    typeParameters?: TypeParameterLikeDeclaration[];
    leadingComments?: Comments;
    supertypes?: SuperTypeClause[];
  }

  export interface Function extends NamedNode {
    type: FunctionType;
    leadingComments?: Comments;
  }

  export interface Variable extends NamedNode {
    type: Type;
    leadingComments?: Comments;
  }

  export interface TypeReference {
    name: string;
    fileName: string;
  }

  export interface TypeReferences {
    typeChecker: ts.TypeChecker;
    references: TypeReference[];
  }
}

function getIdentifierString(node: ts.Node): string {
  return node.getText();
}

function parseFunctionType(node: ts.SignatureDeclarationBase, references: AST.TypeReferences): AST.FunctionType {
  const parameters: AST.PropertyLikeDeclaration[] = [];

  if (node.parameters) {
    for (const parameter of node.parameters) {
      const parameterName = getIdentifierString(parameter.name);
      const parameterType = parseType(parameter.type, references);

      parameters.push({
        start: parameter.getStart(),
        end: parameter.getEnd(),
        isOptional: !!parameter.questionToken,
        name: parameterName,
        type: parameterType,
      });
    }
  }

  return {
    returnValue: parseType(node.type, references),
    parameters,
  };
}

interface ResolvedSymbol {
  name: string;
  sourceFile?: ts.SourceFile;
  isTypeParameter: boolean;
}

interface SymbolGroup {
  symbol?: ResolvedSymbol;
  unions?: SymbolGroup[];
  intersections?: SymbolGroup[];
}

function makeResolvedSymbol(symbol: ts.Symbol, isTypeParameter: boolean = false): SymbolGroup {
  return {
    symbol: {
      name: symbol.name,
      sourceFile: symbol.declarations?.[0]?.getSourceFile(),
      isTypeParameter,
    },
  };
}

function resolveSymbolFromTypeName(typeName: ts.Type, typeChecker: ts.TypeChecker): SymbolGroup | undefined {
  // Weird TypeScript bug, if typeName.isTypeParameter() is in the `if ` expression, then the compiler
  // seems to sometimes think this always evaluates to true, which is not correct.
  const isTypeParameter = typeName.isTypeParameter() as any;
  if (isTypeParameter) {
    const symbol = typeName.getSymbol();
    if (!symbol) {
      return undefined;
    }
    return makeResolvedSymbol(symbol, true);
  }

  const typeNameSymbol = typeName.getSymbol();
  if (typeNameSymbol && typeNameSymbol.valueDeclaration) {
    if (ts.isEnumDeclaration(typeNameSymbol.valueDeclaration)) {
      return makeResolvedSymbol(typeNameSymbol);
    }

    if (ts.isEnumMember(typeNameSymbol.valueDeclaration)) {
      // Hack to expose the enum values as enum types
      const enumDeclaration = typeNameSymbol.valueDeclaration.parent;

      return {
        symbol: {
          name: getIdentifierString(enumDeclaration.name),
          sourceFile: enumDeclaration.getSourceFile(),
          isTypeParameter: false,
        },
      };
    }
  }

  if (typeName.isUnion()) {
    const unions: SymbolGroup[] = [];
    for (const t of typeName.types) {
      const resolvedSymbol = resolveSymbolFromTypeName(t, typeChecker);
      if (!resolvedSymbol) {
        return undefined;
      }
      unions.push(resolvedSymbol);
    }
    return {
      unions,
    };
  }

  if (typeName.isIntersection()) {
    const intersections: SymbolGroup[] = [];
    for (const t of typeName.types) {
      const resolvedSymbol = resolveSymbolFromTypeName(t, typeChecker);
      if (!resolvedSymbol) {
        return undefined;
      }
      intersections.push(resolvedSymbol);
    }
    return {
      intersections,
    };
  }

  const resolvedType = typeChecker.getApparentType(typeName);
  const symbol = resolvedType.getSymbol();
  if (!symbol) {
    return undefined;
  }
  return makeResolvedSymbol(symbol);
}

function resolveSymbol(type: ts.TypeNode, typeChecker: ts.TypeChecker): SymbolGroup | undefined {
  const typeName = typeChecker.getTypeAtLocation(type);
  return resolveSymbolFromTypeName(typeName, typeChecker);
}

function resolveTypeReference(
  type: ts.NodeWithTypeArguments,
  resolvedSymbol: ResolvedSymbol,
  references: AST.TypeReferences,
): number {
  if (!resolvedSymbol.sourceFile) {
    throw new Error(`No declarations for resolve symbol ${getNodeDebugDescription(type)}`);
  }

  const symbolName = resolvedSymbol.name;
  const sourceFile = resolvedSymbol.sourceFile;
  const sourceFileLocation = sourceFile.fileName;

  const typeReferences = references.references;

  for (let i = 0; i < typeReferences.length; i++) {
    const existingReference = typeReferences[i];
    if (existingReference.name === symbolName && existingReference.fileName === sourceFileLocation) {
      return i;
    }
  }

  const index = typeReferences.length;
  typeReferences.push({ name: symbolName, fileName: sourceFileLocation });

  return index;
}

function parseTypeFromSymbolGroup(
  type: ts.NodeWithTypeArguments,
  symbolGroup: SymbolGroup,
  leadingComments: NodeComments | undefined,
  references: AST.TypeReferences,
): AST.Type {
  if (symbolGroup.symbol) {
    const resolvedSymbol = symbolGroup.symbol;
    if (resolvedSymbol.name === 'Array') {
      if (!type.typeArguments || type.typeArguments.length !== 1) {
        throw new Error(
          `Resolved a generic Array reference with invalid type arguments ${getNodeDebugDescription(type)}`,
        );
      }

      const elementTypeNode = type.typeArguments[0];
      const elementType = parseType(elementTypeNode, references);
      return { name: 'array', leadingComments, array: elementType };
    }

    const typeArguments = type.typeArguments?.map((typeArgTypeNode) => {
      const parsedType = parseType(typeArgTypeNode, references);
      return { type: parsedType };
    });

    const typeReferenceIndex = resolveTypeReference(type, resolvedSymbol, references);
    return {
      name: 'reference',
      leadingComments,
      typeReferenceIndex,
      typeArguments,
      isTypeParameter: resolvedSymbol.isTypeParameter,
    };
  }

  if (symbolGroup.intersections) {
    return {
      name: 'intersection',
      leadingComments,
      intersections: symbolGroup.intersections.map((u) => parseTypeFromSymbolGroup(type, u, undefined, references)),
    };
  }

  if (symbolGroup.unions) {
    return {
      name: 'union',
      leadingComments,
      unions: symbolGroup.unions.map((u) => parseTypeFromSymbolGroup(type, u, undefined, references)),
    };
  }

  throw new Error('Invalid symbolGroup');
}

function parseType(type: ts.TypeNode | undefined, references: AST.TypeReferences): AST.Type {
  if (!type) {
    return {
      name: 'void',
    };
  }

  const leadingComments = getNodeComments(type);

  if (ts.isFunctionTypeNode(type)) {
    return {
      name: 'function',
      leadingComments,
      function: parseFunctionType(type, references),
    };
  }

  if (ts.isUnionTypeNode(type)) {
    const unions: AST.Type[] = [];
    for (const unionType of type.types) {
      unions.push(parseType(unionType, references));
    }

    return {
      name: 'union',
      leadingComments,
      unions,
    };
  }

  if (ts.isIntersectionTypeNode(type)) {
    const intersections: AST.Type[] = [];
    for (const unionType of type.types) {
      intersections.push(parseType(unionType, references));
    }

    return {
      name: 'intersection',
      leadingComments,
      intersections,
    };
  }

  if (ts.isArrayTypeNode(type)) {
    return {
      name: 'array',
      leadingComments,
      array: parseType(type.elementType, references),
    };
  }

  if (ts.isTypeReferenceNode(type) || ts.isExpressionWithTypeArguments(type)) {
    const resolvedSymbol = resolveSymbol(type, references.typeChecker);
    if (!resolvedSymbol) {
      throw new Error(`Could not resolve symbol ${getNodeDebugDescription(type)}`);
    }

    return parseTypeFromSymbolGroup(type, resolvedSymbol, leadingComments, references);
  }

  if (ts.isParenthesizedTypeNode(type)) {
    return parseType(type.type, references);
  }

  if (ts.isTypeLiteralNode(type)) {
    // TODO(simon): Provide members. Not yet needed today
    return {
      name: 'literal',
    };
  }

  switch (type.kind) {
    case ts.SyntaxKind.StringKeyword:
      return { name: 'string' };
    case ts.SyntaxKind.NumberKeyword:
      return { name: 'number' };
    case ts.SyntaxKind.BooleanKeyword:
      return { name: 'boolean' };
    case ts.SyntaxKind.VoidKeyword:
      return { name: 'void' };
    case ts.SyntaxKind.UndefinedKeyword:
      return { name: 'undefined' };
    case ts.SyntaxKind.NullKeyword:
      return { name: 'null' };
    case ts.SyntaxKind.AnyKeyword:
      return { name: 'any' };
    case ts.SyntaxKind.ObjectKeyword:
      return { name: 'object' };
    case ts.SyntaxKind.LiteralType:
      if (type.getText() === 'null') {
        return { name: 'null' };
      }
    case ts.SyntaxKind.ThisType:
    case ts.SyntaxKind.UnknownKeyword:
      return { name: 'any' };
    default:
      throw new Error(`Could not resolve type ${getNodeDebugDescription(type)}`);
  }
}

function parsePropertyLike(
  node: ts.TypeElement | ts.ClassElement,
  references: AST.TypeReferences,
): AST.PropertyLikeDeclaration {
  const memberName = node.name && getIdentifierString(node.name);
  const isOptional = !!(node as ts.TypeElement)?.questionToken;

  let type: AST.Type;

  if (ts.isPropertySignature(node) || ts.isPropertyDeclaration(node)) {
    type = parseType(node.type, references);
  } else if (
    ts.isMethodSignature(node) ||
    ts.isMethodDeclaration(node) ||
    ts.isGetAccessor(node) ||
    ts.isGetAccessorDeclaration(node)
  ) {
    const functionType = parseFunctionType(node, references);
    type = {
      name: 'function',
      function: functionType,
    };
  } else {
    throw new Error(`Could not resolve kind ${getNodeDebugDescription(node)}`);
  }

  const result: AST.PropertyLikeDeclaration = {
    start: node.getStart(),
    end: node.getEnd(),
    isOptional,
    name: memberName ?? '',
    type,
    leadingComments: getNodeComments(node),
  };

  return result;
}

export interface DumpedRootNode {
  nodeType: 'enum' | 'function' | 'interface' | 'variable' | 'exportedTypeAlias';
  start: number;
  leadingComments: AST.Comments | undefined;
  text: string;
  kind: ts.SyntaxKind;
  modifiers: string[] | undefined;
  function?: AST.Function;
  enum?: AST.Enum;
  interface?: AST.Interface;
  variable?: AST.Variable;
  exportedTypeAlias?: string;
}

const GENERATE_OR_EXPORT_ANNOTATION_REGEX = /@Generate|@Export/;
const NATIVE_ANNOTATION_REGEX = /@Native/;
const EXPORT_MODULE_ANNOTATION_REGEX = /@ExportModule/;

export function hasGenerateOrExportAnnotation(comments: string): boolean {
  return GENERATE_OR_EXPORT_ANNOTATION_REGEX.test(comments);
}

export function hasNativeExportAnnotation(comments: string): boolean {
  return hasGenerateOrExportAnnotation(comments) || NATIVE_ANNOTATION_REGEX.test(comments);
}

export function hasExportModuleAnnotation(comments: string): boolean {
  return EXPORT_MODULE_ANNOTATION_REGEX.test(comments);
}

function isExportedSymbolOrHasAnnotation(nodeToDump: NodeToDump, shouldDumpAllExportedSymbols: boolean): boolean {
  if (shouldDumpAllExportedSymbols && isNodeExported(nodeToDump.node)) {
    return true;
  }

  // Otherwise, only interested in symbols annotated with Valdi annotations
  if (!nodeToDump.leadingComments) {
    return false;
  }
  const comments = nodeToDump.leadingComments.text;
  const hasMatch = hasNativeExportAnnotation(comments) || !!comments.match(/@Component|@ViewModel|@Context/g);

  return hasMatch;
}

function shouldDumpNodeMember(nodeToDump: NodeToDump, memberNode: ts.TypeElement | ts.ClassElement): boolean {
  const nodeComments = nodeToDump.leadingComments?.text;
  if (!nodeComments) {
    return false;
  }

  if (nodeComments.match(/@NativeTemplateElement/)) {
    return false;
  } else if (nodeComments.match(/@Component/)) {
    return !!(getNodeComments(memberNode)?.text ?? '').match(/@Action|@ConstructorOmitted/g);
  } else if (hasGenerateOrExportAnnotation(nodeComments)) {
    return true;
  } else {
    return false;
  }
}

export function getImportStringLiteralsFromSourceFile(sourceFile: ts.SourceFile): ts.StringLiteral[] {
  const output: ts.StringLiteral[] = [];
  for (const statement of sourceFile.statements) {
    if (ts.isImportDeclaration(statement)) {
      const importDeclaration: ts.ImportDeclaration = statement;
      if (ts.isStringLiteral(importDeclaration.moduleSpecifier)) {
        output.push(importDeclaration.moduleSpecifier);
      }
    }
  }
  return output;
}

function isGlobalScopeAugmentation(module: ts.ModuleDeclaration): boolean {
  return !!(module.flags & ts.NodeFlags.GlobalAugmentation);
}

function containsGlobalScopeAugmentation(sourceFile: ts.SourceFile): boolean {
  // Taken from the TS compiler source code
  const moduleAugmentations: (ts.StringLiteral | ts.Identifier)[] = (sourceFile as any).moduleAugmentations;
  if (!moduleAugmentations) {
    return false;
  }
  for (const augmentation of moduleAugmentations) {
    if (isGlobalScopeAugmentation(augmentation.parent as ts.ModuleDeclaration)) {
      return true;
    }
  }

  return false;
}

function isExternalOrCommonJsModule(file: ts.SourceFile): boolean {
  return ((file as any).externalModuleIndicator || (file as any).commonJsModuleIndicator) !== undefined;
}

function isJsonSourceFile(file: ts.SourceFile): boolean {
  return (file as any).scriptKind === ts.ScriptKind.JSON;
}

function isModuleWithStringLiteralName(node: ts.Node): node is ts.ModuleDeclaration {
  return ts.isModuleDeclaration(node) && node.name.kind === ts.SyntaxKind.StringLiteral;
}

function containsOnlyAmbientModules(sourceFile: ts.SourceFile): boolean {
  for (const statement of sourceFile.statements) {
    if (!isModuleWithStringLiteralName(statement)) {
      return false;
    }
  }
  return true;
}

function isFileAffectingGlobalScope(sourceFile: ts.SourceFile): boolean {
  return (
    containsGlobalScopeAugmentation(sourceFile) ||
    (!isExternalOrCommonJsModule(sourceFile) &&
      !isJsonSourceFile(sourceFile) &&
      !containsOnlyAmbientModules(sourceFile))
  );
}

export function isNonModuleWithAmbiantDeclarations(sourceFile: ts.SourceFile): boolean {
  const statements = sourceFile.statements;

  if (!statements.length) {
    return false;
  }

  let hasAmbiantModuleDeclarations = false;
  let hasImport = false;
  let hasExports = false;

  for (const statement of statements) {
    const modifiers = ts.getCombinedModifierFlags(statement as unknown as ts.Declaration);
    const isAmbiant = (modifiers & ts.ModifierFlags.Ambient) != 0;
    const isExports = (modifiers & ts.ModifierFlags.Export) !== 0;

    if (isAmbiant) {
      if (ts.isModuleDeclaration(statement) && ts.isStringLiteral(statement.name)) {
        hasAmbiantModuleDeclarations = true;
      }
    }
    if (isExports) {
      hasExports = true;
    }

    if (ts.isExportDeclaration(statement) || ts.isExportAssignment(statement)) {
      hasExports = true;
    }

    if (ts.isImportDeclaration(statement) || ts.isImportEqualsDeclaration(statement)) {
      hasImport = true;
    }
  }

  return hasAmbiantModuleDeclarations || (!hasImport && !hasExports);
}

export function dumpRootNodes(sourceFile: ts.SourceFile, astReferences: AST.TypeReferences): DumpedRootNode[] {
  const rootNodes = getRootNodesToDump(sourceFile, undefined);
  const nodesToDump: NodeToDump[] = [];

  if (rootNodes.length) {
    // Need to dump all exported symbols for .vue user scripts, or for modules
    // annotated with @ExportModule
    const shouldDumpAllExportedSymbols =
      !!sourceFile.fileName.match(/\.vue\.ts(x)?$/g) ||
      hasExportModuleAnnotation(rootNodes[0].leadingComments?.text ?? '');
    for (const rootNode of rootNodes) {
      if (isExportedSymbolOrHasAnnotation(rootNode, shouldDumpAllExportedSymbols)) {
        nodesToDump.push(rootNode);
      }
    }
  }

  const dumpedNodes: DumpedRootNode[] = [];

  for (const nodeToDump of nodesToDump) {
    const node = nodeToDump.node;
    const kind = nodeToDump.node.kind;
    const modifiers = ts.canHaveModifiers(nodeToDump.node)
      ? nodeToDump.node.modifiers?.map((modifier) => modifier.getText())
      : undefined;

    const common = {
      start: nodeToDump.node.getStart(),
      leadingComments: nodeToDump.leadingComments,
      kind,
      modifiers,
    };

    let result: DumpedRootNode;

    if (ts.isEnumDeclaration(node)) {
      const dumpedEnum = dumpEnum(node);
      result = {
        ...common,
        nodeType: 'enum',
        text: dumpedEnum.name,
        enum: dumpedEnum,
      };
    } else if (ts.isFunctionDeclaration(node)) {
      const dumpedFunction = dumpFunction(node, astReferences);
      result = {
        ...common,
        nodeType: 'function',
        text: dumpedFunction.name,
        function: dumpedFunction,
      };
    } else if (ts.isInterfaceDeclaration(node) || ts.isClassDeclaration(node)) {
      const coercedNode = nodeToDump.node as ts.InterfaceDeclaration | ts.ClassDeclaration;
      const dumpedInterface = dumpInterface(coercedNode, astReferences, (memberNode) =>
        shouldDumpNodeMember(nodeToDump, memberNode),
      );
      if (!dumpedInterface) {
        continue;
      }

      result = {
        ...common,
        nodeType: 'interface',
        text: dumpedInterface.name,
        interface: dumpedInterface,
      };
    } else if (ts.isVariableStatement(node)) {
      if (!modifiers?.includes('export')) {
        continue;
      }
      const exportedVariable = dumpVariable(node, astReferences);
      result = {
        ...common,
        nodeType: 'variable',
        text: exportedVariable.name,
        variable: exportedVariable,
      };
    } else if (ts.isTypeAliasDeclaration(node)) {
      if (!modifiers?.includes('export')) {
        continue;
      }
      const exportedVariableName = node.name.getText();
      result = {
        ...common,
        nodeType: 'exportedTypeAlias',
        text: exportedVariableName,
        exportedTypeAlias: exportedVariableName,
      };
    } else {
      continue;
    }

    dumpedNodes.push(result);
  }

  return dumpedNodes.filter((node) => {
    // should have either interface, function, or enum
    return (node.interface || node.function || node.enum || node.variable || node.exportedTypeAlias) && node.kind;
  });
}

export function dumpInterface(
  node: ts.InterfaceDeclaration | ts.ClassDeclaration,
  references: AST.TypeReferences,
  dumpMemberPredicate: (memberNode: ts.TypeElement | ts.ClassElement) => boolean = () => true,
): AST.Interface | undefined {
  if (!node.name) {
    return undefined;
  }

  const name = getIdentifierString(node.name);

  const members: AST.PropertyLikeDeclaration[] = [];

  const typeParameters = node.typeParameters?.map((tsTypeParam) => {
    return {
      start: node.getStart(),
      end: node.getEnd(),
      name: getIdentifierString(tsTypeParam.name),
    };
  });

  for (const member of node.members) {
    if (!member.name) {
      continue;
    }

    if (!dumpMemberPredicate(member)) {
      continue;
    }

    const propertyLike = parsePropertyLike(member, references);
    members.push(propertyLike);
  }

  const result: AST.Interface = {
    start: node.getStart(),
    end: node.getEnd(),
    name,
    members,
    typeParameters,
  };

  if (node.heritageClauses?.length) {
    const supertypes: AST.SuperTypeClause[] = [];
    for (const heritageClause of node.heritageClauses) {
      for (const type of heritageClause.types) {
        const outputType = parseType(type, references);

        supertypes.push({
          start: type.getStart(),
          end: type.getEnd(),
          isImplements: heritageClause.token === ts.SyntaxKind.ImplementsKeyword,
          type: outputType,
        });
      }
    }

    result.supertypes = supertypes;
  }

  const leadingComments = getNodeComments(node);
  if (leadingComments) {
    result.leadingComments = leadingComments;
  }

  return result;
}

export function dumpFunction(node: ts.FunctionDeclaration, references: AST.TypeReferences): AST.Function {
  const functionType = parseFunctionType(node, references);

  return {
    start: node.getStart(),
    end: node.getEnd(),
    name: (node.name && getIdentifierString(node.name)) ?? '',
    type: functionType,
    leadingComments: getNodeComments(node),
  };
}

export function dumpVariable(node: ts.VariableStatement, references: AST.TypeReferences): AST.Variable {
  if (node.declarationList.declarations.length !== 1) {
    throw new Error('Only declaration of single variables are currently supported');
  }

  const declaration = node.declarationList.declarations[0];
  return {
    start: node.getStart(),
    end: node.getEnd(),
    name: getIdentifierString(declaration.name),
    type: parseType(declaration.type, references),
    leadingComments: getNodeComments(node),
  };
}

export function dumpEnum(node: ts.EnumDeclaration): AST.Enum {
  const members: AST.EnumMember[] = [];

  for (const member of node.members) {
    const memberComments = getNodeComments(member);
    const enumMember: AST.EnumMember = {
      start: member.getStart(),
      end: member.getEnd(),
      name: getIdentifierString(member.name),
    };

    if (memberComments) {
      enumMember.leadingComments = memberComments;
    }

    if (member.initializer) {
      if (ts.isNumericLiteral(member.initializer)) {
        enumMember.numberValue = member.initializer.text;
      } else if (ts.isStringLiteralLike(member.initializer)) {
        enumMember.stringValue = member.initializer.text;
      }
    }

    members.push(enumMember);
  }

  return {
    start: node.getStart(),
    end: node.getEnd(),
    name: getIdentifierString(node.name),
    members: members,
  };
}
