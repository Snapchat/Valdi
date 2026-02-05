# Working with AI Assistants

This guide helps developers use AI coding assistants effectively with Valdi.

## 🚨 Critical: Valdi is NOT React

The most important thing to know when using AI assistants with Valdi: **despite using TSX/JSX syntax, Valdi is fundamentally different from React**.

AI models are heavily trained on React and will often suggest React patterns that **don't exist in Valdi**. This guide will help you recognize and correct these suggestions.

## Common AI Hallucinations

### 1. useState Hook (Doesn't Exist)

**AI might suggest:**
```typescript
// ❌ WRONG - useState doesn't exist in Valdi
const [count, setCount] = useState(0);
```

**Correct Valdi pattern:**
```typescript
// ✅ CORRECT - Use StatefulComponent with setState()
import { StatefulComponent } from 'valdi_core/src/Component';

class Counter extends StatefulComponent<ViewModel, State> {
  state = { count: 0 };
  
  incrementCount() {
    this.setState({ count: this.state.count + 1 }); // setState auto re-renders
  }
  
  onRender() {
    <button 
      title={`Count: ${this.state.count}`}
      onPress={this.incrementCount}
    />;
  }
}
```

### 2. useEffect Hook (Doesn't Exist)

**AI might suggest:**
```typescript
// ❌ WRONG - useEffect doesn't exist in Valdi
useEffect(() => {
  fetchData();
}, []);
```

**Correct Valdi pattern:**
```typescript
// ✅ CORRECT - Use lifecycle methods
import { StatefulComponent } from 'valdi_core/src/Component';

class DataComponent extends StatefulComponent<ViewModel, State> {
  state = { data: null };
  
  onCreate() {
    this.fetchData();
  }
  
  onViewModelUpdate(prevViewModel: ViewModel) {
    if (this.viewModel.id !== prevViewModel.id) {
      this.fetchData();
    }
  }
  
  async fetchData() {
    const data = await fetch(...);
    this.setState({ data });
  }
}
```

### 3. Functional Components (Don't Exist)

**AI might suggest:**
```typescript
// ❌ WRONG - Functional components don't exist in Valdi
const Button = ({ title, onPress }) => {
  return <button title={title} onPress={onPress} />;
};
```

**Correct Valdi pattern:**
```typescript
// ✅ CORRECT - Use class-based components
import { Component } from 'valdi_core/src/Component';

interface ButtonViewModel {
  title: string;
  onPress: () => void;
}

class Button extends Component<ButtonViewModel> {
  onRender() {
    <button 
      title={this.viewModel.title} 
      onPress={this.viewModel.onPress} 
    />;
  }
}
```

### 4. Returning JSX from onRender()

**AI might suggest:**
```typescript
// ❌ WRONG - onRender returns void, not JSX
class MyComponent extends Component {
  onRender() {
    return <view />; // Compiler error!
  }
}
```

**Correct Valdi pattern:**
```typescript
// ✅ CORRECT - JSX is a statement, onRender returns void
class MyComponent extends Component {
  onRender() {
    <view />; // No return statement
  }
}
```

### 5. useContext Hook (Doesn't Exist)

**AI might suggest:**
```typescript
// ❌ WRONG - useContext doesn't exist in Valdi
const theme = useContext(ThemeContext);
```

**Correct Valdi pattern:**
```typescript
// ✅ CORRECT - Use Provider pattern with HOC
import { createProviderComponentWithKeyName } from 'valdi_core/src/provider/createProvider';
import { withProviders } from 'valdi_core/src/provider/withProviders';
import { ProvidersValuesViewModel } from 'valdi_core/src/provider/withProviders';
import { Component } from 'valdi_core/src/Component';

// Define theme service
class Theme {
  primary = '#FFFC00';
}

// Create provider
const ThemeProvider = createProviderComponentWithKeyName<Theme>('ThemeProvider');

// Provide value
class AppRoot extends Component {
  private theme = new Theme();
  
  onRender() {
    <ThemeProvider value={this.theme}>
      <ThemedComponentWithProvider />
    </ThemeProvider>;
  }
}

// Consume with HOC
interface ThemedViewModel extends ProvidersValuesViewModel<[Theme]> {}

class ThemedComponent extends Component<ThemedViewModel> {
  onRender() {
    const [theme] = this.viewModel.providersValues;
    <view backgroundColor={theme.primary} />;
  }
}

const ThemedComponentWithProvider = withProviders(ThemeProvider)(ThemedComponent);
```

## How to Prompt AI Assistants

### Good Prompts

When asking AI for help, be explicit that you're using Valdi:

✅ "In Valdi (not React), how do I add state to a component?"  
✅ "Using Valdi's class-based component model, how do I fetch data on mount?"  
✅ "How do I trigger a re-render in Valdi after updating state?"

### Prompts to Avoid

❌ "How do I add a counter?" (AI will assume React)  
❌ "Create a functional component" (Doesn't exist in Valdi)  
❌ "Use hooks to manage state" (Hooks don't exist in Valdi)

## Quick Reference: React vs Valdi

| Concept | React Pattern | Valdi Pattern |
|---------|---------------|---------------|
| **Component** | `const C = () => {}` | `class C extends StatefulComponent {}` |
| **State** | `useState(0)` | `state = { count: 0 }` |
| **Update State** | `setCount(1)` | `this.setState({ count: 1 })` |
| **Props** | `props.title` | `this.viewModel.title` |
| **Mount effect** | `useEffect(() => {}, [])` | `onCreate() {}` |
| **Update effect** | `useEffect(() => {}, [dep])` | `onViewModelUpdate(prev) {}` |
| **Unmount effect** | `useEffect(() => () => {}, [])` | `onDestroy() {}` |
| **Context** | `useContext(Ctx)` | `withProviders(Provider)(Component) + this.viewModel.providersValues` |
| **Render** | `return <view />` | `<view />; // statement, returns void` |

## Setting Up AI Tools

### Cursor

Cursor will automatically use the `.cursorrules` file in the repository root, which includes Valdi-specific guidelines.

### GitHub Copilot

Add this to your workspace settings:

```json
{
  "github.copilot.chat.codeGeneration.instructions": [
    {
      "text": "This is a Valdi project, not React. Use class-based components extending StatefulComponent (with state) or Component (stateless). State is managed via StatefulComponent with this.setState(). Lifecycle methods are onCreate(), onViewModelUpdate(previousViewModel), onDestroy(). Props are accessed via this.viewModel. The onRender() method returns void."
    }
  ]
}
```

### Claude / ChatGPT

When starting a conversation, include:

> "I'm working with Valdi, a cross-platform UI framework that uses TSX syntax but is NOT React. Valdi uses class-based components (StatefulComponent with setState() for state, or Component for stateless), not functional components or hooks. Props are accessed via this.viewModel. Lifecycle methods are onCreate(), onViewModelUpdate(), and onDestroy(). The onRender() method returns void, with JSX written as statements."

## Reviewing AI Suggestions

Always check AI-generated code for these red flags:

🚩 **Imports from 'react'** - Valdi imports from 'valdi_core'  
🚩 **Hooks (useState, useEffect, etc.)** - Don't exist in Valdi  
🚩 **Functional components** - Must be class-based  
🚩 **Return statements in onRender()** - Should return void  
🚩 **Using `this.props`** - Should be `this.viewModel`  
🚩 **Using `markNeedsRender()`** - Doesn't exist, use `setState()` instead  
🚩 **Lifecycle methods like `onMount/onUnmount`** - Should be `onCreate/onDestroy`  
🚩 **`this.context.get()`** - Doesn't exist, use `withProviders()` HOC pattern

## Resources

- **[AGENTS.md](../../AGENTS.md)** - Comprehensive guide for AI assistants
- **[Getting Started](./getting-started.md)** - Learn Valdi basics
- **[API Reference](./api/)** - Complete API documentation

## Getting Help

If AI tools are consistently giving incorrect suggestions:

1. **Check configuration**: Ensure `.cursorrules` is in your project root
2. **Be explicit**: Mention "Valdi (not React)" in your prompts
3. **Report issues**: [Open an issue](https://github.com/Snapchat/Valdi/issues) if patterns are consistently wrong
4. **Join Discord**: [Ask the community](https://discord.gg/uJyNEeYX2U) for help

---

Remember: AI assistants are trained primarily on React code. When working with Valdi, you're the domain expert guiding the AI, not the other way around!
