import { RuleTester } from '@typescript-eslint/rule-tester';
import rule from '../../src/rules/no-react-patterns';

const ruleTester = new RuleTester({
  parser: '@typescript-eslint/parser',
  parserOptions: {
    ecmaFeatures: { jsx: true },
  },
});

ruleTester.run('no-react-patterns', rule, {
  valid: [
    // Valdi-style class component is fine
    {
      code: `
        import { Component } from 'valdi_core/src/Component';
        class MyComp extends Component {
          onRender() {
            <view />;
          }
        }
      `,
    },
    // Importing from valdi_core is fine
    {
      code: `import { Component } from 'valdi_core/src/Component';`,
    },
    // Arrow functions that don't return JSX are fine
    {
      code: `const handler = () => 42;`,
    },
    {
      code: `const getText = () => 'hello';`,
    },
    // onRender() returning void (no return statement) is fine
    {
      code: `
        class MyComp {
          onRender() {
            <view />;
          }
        }
      `,
    },
    // Non-hook function calls are fine
    {
      code: `const x = myCustomHook();`,
    },
  ],
  invalid: [
    // Import from 'react'
    {
      code: `import React from 'react';`,
      errors: [{ messageId: 'reactImport' }],
    },
    {
      code: `import { useState } from 'react';`,
      errors: [{ messageId: 'reactImport' }],
    },
    // React hooks
    {
      code: `const [count, setCount] = useState(0);`,
      errors: [{ messageId: 'reactHook', data: { hookName: 'useState' } }],
    },
    {
      code: `useEffect(() => {}, []);`,
      errors: [{ messageId: 'reactHook', data: { hookName: 'useEffect' } }],
    },
    {
      code: `const cb = useCallback(() => {}, []);`,
      errors: [{ messageId: 'reactHook', data: { hookName: 'useCallback' } }],
    },
    {
      code: `const value = useMemo(() => compute(), []);`,
      errors: [{ messageId: 'reactHook', data: { hookName: 'useMemo' } }],
    },
    // Functional component — arrow function returning JSX directly
    {
      code: `const MyComp = () => <view />;`,
      errors: [{ messageId: 'functionalComponent' }],
    },
    // Functional component — arrow function returning JSX via block body
    {
      code: `const MyComp = () => { return <view />; };`,
      errors: [{ messageId: 'functionalComponent' }],
    },
    // Functional component — arrow returning JSX in ternary
    {
      code: `const MyComp = (x: boolean) => x ? <view /> : <text />;`,
      errors: [{ messageId: 'functionalComponent' }],
    },
    // Functional component — arrow returning JSX in logical expression
    {
      code: `const MyComp = (x: boolean) => x && <view />;`,
      errors: [{ messageId: 'functionalComponent' }],
    },
    // onRender() returning JSX
    {
      code: `
        class MyComp {
          onRender() {
            return <view />;
          }
        }
      `,
      errors: [{ messageId: 'returnInRender' }],
    },
    // onRender() returning JSX in ternary
    {
      code: `
        class MyComp {
          onRender() {
            return this.show ? <view /> : null;
          }
        }
      `,
      errors: [{ messageId: 'returnInRender' }],
    },
    // Extending React.Component
    {
      code: `class MyComp extends React.Component {}`,
      errors: [{ messageId: 'reactComponent' }],
    },
  ],
});
