import { RuleTester } from '@typescript-eslint/rule-tester';
import rule from '../../src/rules/jsx-no-lambda';

const ruleTester = new RuleTester({
  parser: '@typescript-eslint/parser',
  parserOptions: {
    ecmaFeatures: { jsx: true },
  },
});

ruleTester.run('jsx-no-lambda', rule, {
  valid: [
    // Primitive values are fine
    {
      code: `<MyComp count={42} />`,
    },
    {
      code: `<MyComp label="hello" />`,
    },
    {
      code: `<MyComp enabled={true} />`,
    },
    // Variables referencing objects/arrays/functions are fine (stable references)
    {
      code: `const handler = () => {}; const x = <MyComp onPress={handler} />;`,
    },
    {
      code: `const items = [1, 2, 3]; const x = <MyComp items={items} />;`,
    },
    {
      code: `const config = { theme: 'dark' }; const x = <MyComp config={config} />;`,
    },
    // Pre-computed .map() result stored in a variable is fine
    {
      code: `const mapped = items.map(fn); const x = <MyComp items={mapped} />;`,
    },
    // Non-array method calls are fine
    {
      code: `<MyComp items={this.getItems()} />`,
    },
    {
      code: `<MyComp items={createReusableCallback(() => doThing())} />`,
    },
    // Computed property access (bracket notation) should not be flagged
    {
      code: `<MyComp items={obj[map]()} />`,
    },
    // .sort() and .reverse() mutate in place and return the same reference
    {
      code: `<MyComp items={items.sort(compareFn)} />`,
    },
    {
      code: `<MyComp items={items.reverse()} />`,
    },
  ],
  invalid: [
    // Inline arrow function
    {
      code: `<MyComp onPress={() => doSomething()} />`,
      errors: [{ messageId: 'incorrectLambda', data: { attributeName: 'onPress' } }],
    },
    // Inline function expression
    {
      code: `<MyComp onPress={function() { doSomething(); }} />`,
      errors: [{ messageId: 'incorrectLambda', data: { attributeName: 'onPress' } }],
    },
    // Inline array literal
    {
      code: `<MyComp items={[1, 2, 3]} />`,
      errors: [{ messageId: 'incorrectArray', data: { attributeName: 'items' } }],
    },
    // Inline object literal
    {
      code: `<MyComp config={{ theme: 'dark' }} />`,
      errors: [{ messageId: 'incorrectObject', data: { attributeName: 'config' } }],
    },
    // Inline object literal with multiple properties
    {
      code: `<MyComp style={{ flex: 1, color: 'red' }} />`,
      errors: [{ messageId: 'incorrectObject', data: { attributeName: 'style' } }],
    },
    // Empty object literal
    {
      code: `<MyComp options={{}} />`,
      errors: [{ messageId: 'incorrectObject', data: { attributeName: 'options' } }],
    },
    // .map() directly in JSX attribute
    {
      code: `<MyComp items={this.viewModel.entities.map(mapFn)} />`,
      errors: [{ messageId: 'incorrectArrayMethod', data: { attributeName: 'items', methodName: 'map' } }],
    },
    // .filter() directly in JSX attribute
    {
      code: `<MyComp items={items.filter(x => x.active)} />`,
      errors: [{ messageId: 'incorrectArrayMethod', data: { attributeName: 'items', methodName: 'filter' } }],
    },
    // .slice() directly in JSX attribute
    {
      code: `<MyComp items={items.slice(0, 5)} />`,
      errors: [{ messageId: 'incorrectArrayMethod', data: { attributeName: 'items', methodName: 'slice' } }],
    },
    // .flatMap() directly in JSX attribute
    {
      code: `<MyComp items={sections.flatMap(s => s.entities)} />`,
      errors: [{ messageId: 'incorrectArrayMethod', data: { attributeName: 'items', methodName: 'flatMap' } }],
    },
    // .concat() directly in JSX attribute
    {
      code: `<MyComp items={listA.concat(listB)} />`,
      errors: [{ messageId: 'incorrectArrayMethod', data: { attributeName: 'items', methodName: 'concat' } }],
    },
    // .flat() directly in JSX attribute
    {
      code: `<MyComp items={items.flat()} />`,
      errors: [{ messageId: 'incorrectArrayMethod', data: { attributeName: 'items', methodName: 'flat' } }],
    },
  ],
});
