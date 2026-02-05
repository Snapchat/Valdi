import { TSESTree, ESLintUtils, AST_NODE_TYPES } from '@typescript-eslint/utils';

const createRule = ESLintUtils.RuleCreator(
  name => `https://github.com/Snapchat/Valdi/blob/main/docs/docs/ai-tooling.md`,
);

const REACT_HOOKS = [
  'useState',
  'useEffect',
  'useContext',
  'useReducer',
  'useCallback',
  'useMemo',
  'useRef',
  'useImperativeHandle',
  'useLayoutEffect',
  'useDebugValue',
  'useDeferredValue',
  'useTransition',
  'useId',
  'useSyncExternalStore',
  'useInsertionEffect',
];

/**
 * Helper function to recursively check if an expression contains JSX.
 * Handles direct JSX, ternaries, logical expressions, and nested combinations.
 */
function containsJSX(node: TSESTree.Expression | TSESTree.SpreadElement | null | undefined): boolean {
  if (!node) return false;

  // Direct JSX
  if (node.type === AST_NODE_TYPES.JSXElement || node.type === AST_NODE_TYPES.JSXFragment) {
    return true;
  }

  // Ternary: condition ? <view /> : <text />
  if (node.type === AST_NODE_TYPES.ConditionalExpression) {
    return containsJSX(node.consequent) || containsJSX(node.alternate);
  }

  // Logical: condition && <view /> or condition || <view />
  if (node.type === AST_NODE_TYPES.LogicalExpression) {
    return containsJSX(node.left) || containsJSX(node.right);
  }

  // Parenthesized expressions: (<view />)
  if (node.type === AST_NODE_TYPES.TSAsExpression || node.type === AST_NODE_TYPES.TSNonNullExpression) {
    return containsJSX(node.expression);
  }

  return false;
}

const rule = createRule({
  name: 'no-react-patterns',
  meta: {
    type: 'problem',
    docs: {
      description: 'Prevents React patterns that do not exist in Valdi',
      recommended: 'error',
    },
    messages: {
      reactImport: 'Do not import from "react". Valdi is not React. Use "valdi_core" instead.',
      reactHook:
        'React hook "{{hookName}}" does not exist in Valdi. Use class properties and lifecycle methods instead. See https://github.com/Snapchat/Valdi/blob/main/docs/docs/ai-tooling.md',
      functionalComponent:
        'Functional components do not exist in Valdi. Use class-based components that extend Component. See https://github.com/Snapchat/Valdi/blob/main/docs/docs/ai-tooling.md',
      returnInRender:
        'onRender() should return void, not JSX. Write JSX as a statement, not a return value. See https://github.com/Snapchat/Valdi/blob/main/docs/docs/ai-tooling.md',
      reactComponent:
        'Do not extend React.Component. Extend Component from "valdi_core/src/Component" instead.',
    },
    schema: [], // no options
  },
  defaultOptions: [],
  create(context) {
    return {
      // Detect imports from 'react'
      ImportDeclaration(node: TSESTree.ImportDeclaration) {
        if (node.source.value === 'react') {
          context.report({
            node,
            messageId: 'reactImport',
          });
        }
      },

      // Detect React hooks being called
      CallExpression(node: TSESTree.CallExpression) {
        if (node.callee.type === AST_NODE_TYPES.Identifier && REACT_HOOKS.includes(node.callee.name)) {
          context.report({
            node,
            messageId: 'reactHook',
            data: {
              hookName: node.callee.name,
            },
          });
        }
      },

      // Detect functional components (arrow functions returning JSX)
      'VariableDeclarator > ArrowFunctionExpression'(node: TSESTree.ArrowFunctionExpression) {
        // Check if the arrow function returns JSX (including ternaries, logical expressions)
        if (containsJSX(node.body as TSESTree.Expression)) {
          context.report({
            node,
            messageId: 'functionalComponent',
          });
          return;
        }
        
        // Check if the arrow function has a block body that returns JSX
        if (node.body.type === AST_NODE_TYPES.BlockStatement) {
          const returnStatements = node.body.body.filter(
            stmt => stmt.type === AST_NODE_TYPES.ReturnStatement,
          ) as TSESTree.ReturnStatement[];
          
          for (const returnStmt of returnStatements) {
            if (containsJSX(returnStmt.argument)) {
              context.report({
                node,
                messageId: 'functionalComponent',
              });
              break;
            }
          }
        }
      },

      // Detect return statements in onRender() method
      'MethodDefinition[key.name="onRender"] ReturnStatement'(node: TSESTree.ReturnStatement) {
        // onRender() should return void, not JSX (including ternaries, logical expressions)
        if (containsJSX(node.argument)) {
          context.report({
            node,
            messageId: 'returnInRender',
          });
        }
      },

      // Detect extending React.Component
      ClassDeclaration(node: TSESTree.ClassDeclaration) {
        if (node.superClass?.type === AST_NODE_TYPES.MemberExpression) {
          const superClass = node.superClass;
          if (
            superClass.object.type === AST_NODE_TYPES.Identifier &&
            superClass.object.name === 'React' &&
            superClass.property.type === AST_NODE_TYPES.Identifier &&
            superClass.property.property.name === 'Component'
          ) {
            context.report({
              node: node.superClass,
              messageId: 'reactComponent',
            });
          }
        }
      },
    };
  },
});

export default rule;
