import * as ts from 'typescript';

/**
 * Creates a TypeScript transformer that wraps console.log/warn/error/info/debug
 * call expressions in `if (__valdiRuntime.isLoggingEnabled) { ... }` guards.
 *
 * This eliminates the cost of console logging in production builds:
 * - Argument evaluation at the call site is skipped
 * - debugStringify in Console.ts is skipped
 * - The JS-to-C++ bridge call is skipped
 *
 * The transform targets:
 * - `ExpressionStatement` nodes whose expression is a direct `console.X(...)` call
 * - expression-bodied arrow functions whose body is a direct `console.X(...)` call
 *
 * This means:
 *
 * - `console.log(a, b);` -> wrapped
 * - `.catch(console.error)` -> NOT wrapped (function reference, not a call)
 * - `.catch(err => console.error(err))` -> wrapped
 * - `const x = console.log(...)` -> NOT wrapped (not an ExpressionStatement)
 *
 * Known caveats:
 * - Arguments with side effects (e.g., `console.log(counter++)`) will have
 *   those side effects suppressed when logging is disabled. No instances of
 *   this pattern exist in the codebase today.
 * - `console.*` used in expression position is not transformed.
 * - Expression-bodied arrows with non-direct console usage are not transformed
 *   (e.g., `x => doSomething(console.log(x))`).
 */

const CONSOLE_METHODS = new Set(['log', 'warn', 'error', 'info', 'debug']);
const RUNTIME_PROVIDER_MODULE = 'valdi_core/src/ValdiRuntimeProvider';
const RUNTIME_BINDING = '__valdiRuntime';

/**
 * Returns true if the given CallExpression is a `console.X(...)` call
 * where X is one of log, warn, error, info, debug.
 */
function isConsoleCall(node: ts.CallExpression): boolean {
  const expr = node.expression;
  if (!ts.isPropertyAccessExpression(expr)) {
    return false;
  }
  if (!ts.isIdentifier(expr.expression) || expr.expression.text !== 'console') {
    return false;
  }
  return CONSOLE_METHODS.has(expr.name.text);
}

/**
 * Creates the guard expression: `__valdiRuntime.isLoggingEnabled`
 */
function createGuardExpression(factory: ts.NodeFactory): ts.Expression {
  return factory.createPropertyAccessExpression(
    factory.createIdentifier(RUNTIME_BINDING),
    factory.createIdentifier('isLoggingEnabled'),
  );
}

function createRuntimeProviderStatements(factory: ts.NodeFactory): ts.Statement[] {
  const runtimeBinding = factory.createVariableStatement(
    undefined,
    factory.createVariableDeclarationList(
      [
        factory.createVariableDeclaration(
          factory.createIdentifier(RUNTIME_BINDING),
          undefined,
          undefined,
          factory.createCallExpression(
            factory.createPropertyAccessExpression(
              factory.createCallExpression(factory.createIdentifier('require'), undefined, [
                factory.createStringLiteral(RUNTIME_PROVIDER_MODULE),
              ]),
              factory.createIdentifier('getValdiRuntime'),
            ),
            undefined,
            [],
          ),
        ),
      ],
      ts.NodeFlags.Const,
    ),
  );
  return [runtimeBinding];
}

function unwrapParenthesizedExpression(node: ts.Expression): ts.Expression {
  let expr = node;
  while (ts.isParenthesizedExpression(expr)) {
    expr = expr.expression;
  }
  return expr;
}

/**
 * Creates a TypeScript transformer factory for wrapping console calls.
 *
 */
export function createConsoleLogTransformer(): ts.TransformerFactory<ts.SourceFile> {
  return (context: ts.TransformationContext): ts.Transformer<ts.SourceFile> => {
    const factory = context.factory;

    return (sourceFile: ts.SourceFile): ts.SourceFile => {
      let addedLoggingGuard = false;
      const visitor: ts.Visitor = (node: ts.Node): ts.Node => {
        const visitedNode = ts.visitEachChild(node, visitor, context);

        // Match ExpressionStatement nodes with direct console calls.
        if (
          ts.isExpressionStatement(visitedNode) &&
          ts.isCallExpression(visitedNode.expression) &&
          isConsoleCall(visitedNode.expression)
        ) {
          addedLoggingGuard = true;
          // Wrap in: if (__valdiRuntime.isLoggingEnabled) { <original statement> }
          return factory.createIfStatement(
            createGuardExpression(factory),
            factory.createBlock([visitedNode], /* multiLine */ true),
          );
        }

        // Match expression-bodied arrow functions with direct console calls.
        if (ts.isArrowFunction(visitedNode) && !ts.isBlock(visitedNode.body)) {
          const bodyExpression = unwrapParenthesizedExpression(visitedNode.body);
          if (ts.isCallExpression(bodyExpression) && isConsoleCall(bodyExpression)) {
            addedLoggingGuard = true;
            const guardedCall = factory.createIfStatement(
              createGuardExpression(factory),
              factory.createBlock([factory.createExpressionStatement(bodyExpression)], /* multiLine */ true),
            );
            return factory.updateArrowFunction(
              visitedNode,
              visitedNode.modifiers,
              visitedNode.typeParameters,
              visitedNode.parameters,
              visitedNode.type,
              visitedNode.equalsGreaterThanToken,
              factory.createBlock([guardedCall], /* multiLine */ true),
            );
          }
        }

        return visitedNode;
      };

      const updatedSourceFile = ts.visitNode(sourceFile, visitor) as ts.SourceFile;
      if (!addedLoggingGuard) {
        return updatedSourceFile;
      }
      return factory.updateSourceFile(updatedSourceFile, [
        ...createRuntimeProviderStatements(factory),
        ...updatedSourceFile.statements,
      ]);
    };
  };
}
