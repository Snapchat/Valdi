import { TSESTree, ESLintUtils, AST_NODE_TYPES } from '@typescript-eslint/utils';
import { TypeFormatFlags } from 'typescript';

// Array methods that always return a new reference, causing unnecessary re-renders
// when called directly in a JSX attribute.
const ARRAY_METHODS_RETURNING_NEW = new Set([
  'map',
  'filter',
  'slice',
  'flatMap',
  'concat',
  'flat',
  'toSorted',
  'toReversed',
  'toSpliced',
]);

const createRule = ESLintUtils.RuleCreator(
  name =>
    `https://github.com/Snapchat/Valdi/blob/main/docs/docs/performance-optimization.md#using-callbacks-in-elements`,
);

const rule = createRule({
  name: 'jsx-no-lambda',
  meta: {
    type: 'problem',
    docs: {
      description: "Ensures that lambda functions and arrays aren't rendered directly in JSX",
      recommended: 'strict',
    },
    messages: {
      incorrectLambda: "Avoid assigning a lambda function directly to the '{{attributeName}}' JSX attribute.",
      incorrectArray:
        "Avoid assigning an array directly to the '{{attributeName}}' JSX attribute. Consider storing the array as a member of this component.",
      incorrectObject:
        "Avoid assigning an object literal directly to the '{{attributeName}}' JSX attribute. Consider storing the object as a member of this component to avoid re-renders caused by referential inequality.",
      incorrectArrayMethod:
        "Avoid calling .{{methodName}}() directly in the '{{attributeName}}' JSX attribute. It returns a new array reference every render, causing unnecessary child re-renders. Pre-compute in onViewModelUpdate instead.",
    },
    schema: [], // no options
  },
  defaultOptions: [],
  create(context) {
    return {
      JSXAttribute(node: TSESTree.JSXAttribute) {
        if (node.value?.type !== AST_NODE_TYPES.JSXExpressionContainer) {
          return;
        }
        switch (node.value.expression.type) {
          case AST_NODE_TYPES.ArrowFunctionExpression:
          case AST_NODE_TYPES.FunctionExpression:
            context.report({
              node: node.value,
              messageId: 'incorrectLambda',
              data: {
                attributeName: node.name.name,
              },
            });
            break;
          case AST_NODE_TYPES.ArrayExpression:
            context.report({
              node: node.value,
              messageId: 'incorrectArray',
              data: {
                attributeName: node.name.name,
              },
            });
            break;
          case AST_NODE_TYPES.ObjectExpression:
            context.report({
              node: node.value,
              messageId: 'incorrectObject',
              data: {
                attributeName: node.name.name,
              },
            });
            break;
          case AST_NODE_TYPES.CallExpression: {
            const callee = node.value.expression.callee;
            if (
              callee.type === AST_NODE_TYPES.MemberExpression &&
              !callee.computed &&
              callee.property.type === AST_NODE_TYPES.Identifier &&
              ARRAY_METHODS_RETURNING_NEW.has(callee.property.name)
            ) {
              context.report({
                node: node.value,
                messageId: 'incorrectArrayMethod',
                data: {
                  attributeName: node.name.name,
                  methodName: callee.property.name,
                },
              });
            }
            break;
          }
        }
      },
    };
  },
});

export default rule;
