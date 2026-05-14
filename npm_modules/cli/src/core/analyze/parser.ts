import * as fs from 'fs';
import * as ts from 'typescript';
import type { ComponentFingerprint, FingerprintFlags, ScanConfig } from './fingerprint';
import { extractModuleName } from './scanner';

export function parseFile(filePath: string, config: ScanConfig): ComponentFingerprint | undefined {
  let content: string;
  try {
    content = fs.readFileSync(filePath, 'utf8');
  } catch {
    return undefined;
  }

  const sourceFile = ts.createSourceFile(filePath, content, ts.ScriptTarget.Latest, false, ts.ScriptKind.TSX);

  const imports = extractImports(sourceFile);
  const sharedLibImports = imports.filter(imp => isSharedLibImport(imp, config));
  const className = findComponentClassName(sourceFile);
  if (!className) return undefined;

  const viewModelProps = extractViewModelProps(sourceFile);
  const stateProps = extractStateProps(sourceFile);
  const renderElements = extractRenderElements(sourceFile);
  const flags = detectFlags(sourceFile, content, imports);

  return {
    filePath,
    moduleName: extractModuleName(filePath, config) ?? 'unknown',
    className,
    viewModelProps,
    stateProps,
    importPaths: imports,
    sharedLibImports,
    renderElements,
    flags,
  };
}

function extractImports(sourceFile: ts.SourceFile): string[] {
  const imports: string[] = [];
  for (const statement of sourceFile.statements) {
    if (
      ts.isImportDeclaration(statement) &&
      statement.moduleSpecifier &&
      ts.isStringLiteral(statement.moduleSpecifier)
    ) {
      imports.push(statement.moduleSpecifier.text);
    }
  }
  return imports;
}

function isSharedLibImport(importPath: string, config: ScanConfig): boolean {
  return config.sharedLibNames.some(name => importPath.startsWith(`${name}/`) || importPath === name);
}

function findComponentClassName(sourceFile: ts.SourceFile): string | undefined {
  for (const statement of sourceFile.statements) {
    if (!ts.isClassDeclaration(statement) || !statement.name) continue;

    if (statement.heritageClauses) {
      for (const clause of statement.heritageClauses) {
        if (clause.token !== ts.SyntaxKind.ExtendsKeyword) continue;
        for (const type of clause.types) {
          const text = type.expression.getText(sourceFile);
          if (text === 'Component' || text === 'StatefulComponent') {
            return statement.name.text;
          }
        }
      }
    }
  }
  return undefined;
}

function extractViewModelProps(sourceFile: ts.SourceFile): string[] {
  const props: string[] = [];

  for (const statement of sourceFile.statements) {
    if (!ts.isInterfaceDeclaration(statement)) continue;
    const name = statement.name.text;
    if (!name.endsWith('ViewModel') && !name.endsWith('VM') && !name.endsWith('Props')) continue;

    for (const member of statement.members) {
      if (ts.isPropertySignature(member) && member.name && ts.isIdentifier(member.name)) {
        props.push(member.name.text);
      }
    }
    if (props.length > 0) break;
  }

  return props;
}

function extractStateProps(sourceFile: ts.SourceFile): string[] {
  const props: string[] = [];

  for (const statement of sourceFile.statements) {
    if (!ts.isInterfaceDeclaration(statement)) continue;
    const name = statement.name.text;
    if (!name.endsWith('State') && !name.includes('State')) continue;
    if (name.endsWith('ViewModel') || name.endsWith('VM')) continue;

    for (const member of statement.members) {
      if (ts.isPropertySignature(member) && member.name && ts.isIdentifier(member.name)) {
        props.push(member.name.text);
      }
    }
    if (props.length > 0) break;
  }

  return props;
}

function extractRenderElements(sourceFile: ts.SourceFile): string[] {
  const elements: string[] = [];

  function visit(node: ts.Node): void {
    if (ts.isJsxOpeningElement(node) || ts.isJsxSelfClosingElement(node)) {
      const tagName = node.tagName.getText(sourceFile);
      elements.push(tagName);
    }
    ts.forEachChild(node, visit);
  }

  const onRender = findMethod(sourceFile, 'onRender');
  if (onRender) {
    ts.forEachChild(onRender, visit);
  }

  return elements;
}

function findMethod(sourceFile: ts.SourceFile, methodName: string): ts.Node | undefined {
  for (const statement of sourceFile.statements) {
    if (!ts.isClassDeclaration(statement)) continue;
    for (const member of statement.members) {
      if (
        ts.isMethodDeclaration(member) &&
        member.name &&
        ts.isIdentifier(member.name) &&
        member.name.text === methodName
      ) {
        return member.body;
      }
    }
  }
  return undefined;
}

function detectFlags(sourceFile: ts.SourceFile, content: string, imports: string[]): FingerprintFlags {
  const hasLoadingKeywords =
    /\bloading\b/i.test(content) &&
    (/\bshimmer\b/i.test(content) ||
      /\bspinner\b/i.test(content) ||
      /isLoading\b/.test(content) ||
      /\bLoadingState\b/.test(content));
  const hasErrorKeywords =
    /\berror\b/i.test(content) &&
    (/\bretry\b/i.test(content) || /\berrorState\b/.test(content) || /\bshowError\b/i.test(content));
  const hasLoadingEnum = /loading|loaded|error|empty/i.test(content) && /enum\s+\w*[Ss]tate/i.test(content);

  const usesPromise = imports.some(imp => imp.includes('CancelablePromise')) || /\bCancelablePromise\b/.test(content);
  const usesObservable =
    imports.some(imp => imp.includes('Observable') || imp.includes('observable')) || /\bObservable\b/.test(content);
  const usesAnimationShimmer =
    imports.some(imp => imp.includes('AnimationShimmer')) || /\bAnimationShimmer\b/.test(content);

  let isStateful = false;
  let extendsComponent = false;
  for (const statement of sourceFile.statements) {
    if (!ts.isClassDeclaration(statement) || !statement.heritageClauses) continue;
    for (const clause of statement.heritageClauses) {
      if (clause.token !== ts.SyntaxKind.ExtendsKeyword) continue;
      for (const type of clause.types) {
        const text = type.expression.getText(sourceFile);
        if (text === 'StatefulComponent') isStateful = true;
        if (text === 'Component' || text === 'StatefulComponent') extendsComponent = true;
      }
    }
  }

  return {
    hasLoadingState: hasLoadingKeywords || hasLoadingEnum,
    hasErrorState: hasErrorKeywords,
    usesPromise,
    usesObservable,
    usesAnimationShimmer,
    isStateful,
    extendsComponent,
  };
}
