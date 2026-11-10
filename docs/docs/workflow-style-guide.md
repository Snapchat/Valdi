# Valdi Style Guide

### Overview

[ESLint]: https://eslint.org
[Prettier]: https://prettier.io
[valdi-eslint-rules]: ../../npm_modules/eslint-plugin-valdi/src/rules
[eslint-rc]: ../../tools/lint/.eslintrc.js

In this section we would like to share the details about why and how we can use `Valdi Style Guide`.
`Valdi Style Guide` helps to achieve the following:

- Improve code quality, making code more consistent, avoiding bugs, errors, and suspicious constructs
- Improve TypeScript learning curve
- API depreciation

`Valdi Style Guide` is a set of two main tools - language linter and formatting tool.
We use for that the most popular and industry approved libraries:

- for code linter we use [ESLint][]
- for formatting we use [Prettier][]

[Prettier][] allows us to have standard, simple, and opinionated code formatting
rules. All code can be formatted via IDE or CLI so you can easily integrate this step into your development workflow.

[ESLint][] allows us to use the community-recomended set of JavaScript and TypeScript rules.
of rules. Also, we introduced more specific rules for our codebase that can be helpful for us. The up-to-date list of rules you can find
[here][valdi-eslint-rules].

We have less strict rules for existing codebase and for new modules we use more strict rules.
Eventually we have a plan to help and migrate all Valdi modules to one strict set of rules.

[Prettier][] and [ESLint][] work perfectly with VSCode or WebStorm.

If you need some modifications you can read more about vscode settings [here](https://marketplace.visualstudio.com/items?itemName=esbenp.prettier-vscode)
and [here](https://marketplace.visualstudio.com/items?itemName=dbaeumer.vscode-eslint), and about WebStorm settings
[here](https://www.jetbrains.com/help/webstorm/eslint.html) and [here](https://www.jetbrains.com/help/webstorm/prettier.html)

#### Valdi Style Guide CLI commands

If you don't like use the style guide configuration, the Valdi CLI allows you to run all helpful style guide commands. Below are a set of available commands commands that provides a quick way of checking/fixing your code according to our defined standards:

```bash
# Uses eslint and prettier to check the list of files for errors.
valdi lint check [files...]

# Same as check but also attempts to automatically fix errors.
valdi lint format [files...]
```

Under the hood, the CLI tool uses `prettier` and `eslint` to check and format files. Most if not all formatting rules can be fixed automatically via prettier, whereas
[ESLint][] will have more cases that will have to be resolved manually.

#### How to fix your linter error

If you encounter a linter error that can not be fixed automatically. The next step is to google this error or go to the [ESLint][] or [ESLint TypeScript](https://typescript-eslint.io) and follow the steps to resolve it.

For example, you can observe error `explicit-function-return-type`, then you can go to [website](https://typescript-eslint.io/rules/explicit-function-return-type) to see the examples of usage,
details about the error, and how to fix it.

#### Add specific rules per module

It is possible to define a specific rules on a per module basis. The official documentation for defining custom rules can be found at [ESLint documentation](https://eslint.org/docs/user-guide/configuring/configuration-files#how-do-overrides-work).

This [`.eslintrc.js`][eslint-rc] is an existing example of using a custom configuration with ESLint.
