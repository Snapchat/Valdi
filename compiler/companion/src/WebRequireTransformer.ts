import * as ts from 'typescript';

/**
 * TypeScript transformer that annotates source-level require() calls with
 * sentinel comments consumed by PrependWebJsProcessor (web-only).
 *
 * In the TypeScript AST (before tsc's module transform):
 * - `import X from "Y"` → ImportDeclaration node (NOT a require call)
 * - `require("Y")` in source → CallExpression with Identifier("require")
 *
 * Three cases:
 * 1. Variable first-arg require — annotated with /* @valdi-dynamic *​/.
 *    PrependWebJsProcessor rewrites to globalThis.moduleLoader.load().
 * 2. String-literal first-arg with extra args — wrapped in
 *    /* @valdi-web-strip-start *​/ ... /* @valdi-web-strip-end *​/.
 *    PrependWebJsProcessor strips the extras so webpack can statically
 *    resolve `require("X")`. Native keeps the extras because the
 *    minifier drops the comments and the call survives intact —
 *    `instance.load("X", true, true)` (disableProxy/disableSyncDeps).
 * 3. Single-arg string-literal require — left untouched. Works on all
 *    platforms as-is.
 */

export function createWebRequireTransformer(): ts.TransformerFactory<ts.SourceFile> {
  return (context: ts.TransformationContext): ts.Transformer<ts.SourceFile> => {
    const factory = context.factory;

    return (sourceFile: ts.SourceFile): ts.SourceFile => {
      const visitor: ts.Visitor = (node: ts.Node): ts.VisitResult<ts.Node> => {
        const visitedNode = ts.visitEachChild(node, visitor, context);

        if (
          ts.isCallExpression(visitedNode) &&
          ts.isIdentifier(visitedNode.expression) &&
          visitedNode.expression.text === 'require' &&
          visitedNode.arguments.length >= 1
        ) {
          const firstArg = visitedNode.arguments[0];

          // String-literal first arg with extra args — wrap in sentinel
          // pair so the Swift PrependWebJsProcessor (web-only) can locate
          // the call and strip args without paren-counting. Native keeps
          // the extras (disableProxy/disableSyncDeps) because the minifier
          // drops the comments and the call survives intact.
          // Spread args to force a new node — see comment on variable-arg
          // branch below.
          if (ts.isStringLiteralLike(firstArg) && visitedNode.arguments.length > 1) {
            const freshNode = factory.updateCallExpression(
              visitedNode,
              visitedNode.expression,
              visitedNode.typeArguments,
              [...visitedNode.arguments],
            );
            ts.addSyntheticLeadingComment(
              freshNode,
              ts.SyntaxKind.MultiLineCommentTrivia,
              ' @valdi-web-strip-start ',
              /* hasTrailingNewLine */ false,
            );
            ts.addSyntheticTrailingComment(
              freshNode,
              ts.SyntaxKind.MultiLineCommentTrivia,
              ' @valdi-web-strip-end ',
              /* hasTrailingNewLine */ false,
            );
            return freshNode;
          }

          // Single-arg string literal (including no-substitution template
          // literals) — leave untouched for all platforms.
          if (ts.isStringLiteralLike(firstArg)) {
            return visitedNode;
          }

          // Variable first arg — annotate for web-only transform.
          // PrependWebJsProcessor converts to moduleLoader.load().
          // Native minifier strips the comment.
          // Spread args to force a new node — updateCallExpression returns the
          // original if children are identity-equal, and addSyntheticLeadingComment
          // mutates in place (would accumulate in watch mode).
          const freshNode = factory.updateCallExpression(
            visitedNode,
            visitedNode.expression,
            visitedNode.typeArguments,
            [...visitedNode.arguments],
          );
          ts.addSyntheticLeadingComment(
            freshNode,
            ts.SyntaxKind.MultiLineCommentTrivia,
            ' @valdi-dynamic ',
            /* hasTrailingNewLine */ false,
          );
          return freshNode;
        }

        return visitedNode;
      };

      return ts.visitNode(sourceFile, visitor) as ts.SourceFile;
    };
  };
}
