#!/usr/bin/env ts-node
/**
 * Updates AGENTS.md templates in bootstrap directory with correct Valdi patterns
 * Ensures consistency with root AGENTS.md anti-hallucination guidelines
 */

import fs from 'fs';
import path from 'path';

const CLI_ROOT = path.join(__dirname, '..');
const OPEN_SOURCE_ROOT = path.join(CLI_ROOT, '../..');

// Common Valdi patterns section that should be consistent across all AGENTS.md files
const VALDI_PATTERNS_SECTION = `
## 🚨 CRITICAL: Valdi is NOT React!

**AI assistants often suggest React patterns that DON'T EXIST in Valdi.** This is because TSX/JSX syntax looks like React, but Valdi compiles to native code.

### ❌ FORBIDDEN Patterns (Don't exist in Valdi)

\`\`\`typescript
// ❌ WRONG - React hooks don't exist
const [count, setCount] = useState(0);
useEffect(() => { ... }, []);

// ❌ WRONG - Functional components don't exist
const MyComponent = () => <view />;

// ❌ WRONG - Common AI mistakes
this.markNeedsRender();  // Doesn't exist!
this.props.title;        // Should be: this.viewModel.title
onMount() { }            // Should be: onCreate()
onUpdate() { }           // Should be: onViewModelUpdate()
onUnmount() { }          // Should be: onDestroy()
this.context.get(Service);  // Wrong pattern, use withProviders()
\`\`\`

### ✅ CORRECT Valdi Patterns

\`\`\`typescript
// ✅ Stateful component with state
import { StatefulComponent } from 'valdi_core/src/Component';

class MyComponent extends StatefulComponent<ViewModel, State> {
  state = { count: 0 };
  
  // Lifecycle methods
  onCreate() { }                           // Component created
  onViewModelUpdate(prev: ViewModel) { }  // Props changed
  onDestroy() { }                          // Before removal
  
  // State updates
  handleClick() {
    this.setState({ count: this.state.count + 1 }); // Auto re-renders
  }
  
  // Render (returns void, JSX is statement)
  onRender() {
    <button 
      title={\`Count: \${this.state.count}\`}
      onPress={this.handleClick}
    />;
  }
}

// ✅ Stateless component (props only)
import { Component } from 'valdi_core/src/Component';

class SimpleComponent extends Component<ViewModel> {
  onRender() {
    <label value={this.viewModel.title} />;  // viewModel, not props!
  }
}
\`\`\`

### Key Differences from React

| Concept | React | Valdi |
|---------|-------|-------|
| **Components** | Functions or classes | Classes only (Component or StatefulComponent) |
| **State** | \`useState\` hook | \`state = { }\` + \`setState()\` |
| **Props** | \`this.props\` | \`this.viewModel\` |
| **Lifecycle** | \`useEffect\` | \`onCreate\`, \`onViewModelUpdate\`, \`onDestroy\` |
| **Re-render** | \`setCount(...)\` | \`this.setState(...)\` |
| **Return** | \`return <view />\` | \`<view />;\` (statement, returns void) |

For complete framework documentation, see: https://github.com/Snapchat/Valdi
`;

function updateAgentsTemplate(templatePath: string, _isCliApp: boolean) {
  console.log(`\nUpdating: ${path.relative(CLI_ROOT, templatePath)}`);
  
  let content = fs.readFileSync(templatePath, 'utf-8');
  
  // Fix incorrect lifecycle method comments
  content = content.replace(
    /\/\/ onMount\(\) - Called when component is first mounted/g,
    '// onCreate() - Called when component is first created'
  );
  content = content.replace(
    /\/\/ onUnmount\(\) - Called before component is removed/g,
    '// onDestroy() - Called before component is removed'
  );
  content = content.replace(
    /\/\/ onUpdate\(prevProps\) - Called when component updates/g,
    '// onViewModelUpdate(previousViewModel) - Called when viewModel updates'
  );
  
  // Find where to insert the patterns section (after Overview, before Project Structure or Development Workflow)
  const insertAfter = '## Overview';
  const insertIndex = content.indexOf(insertAfter);
  
  if (insertIndex === -1) {
    console.warn('  ⚠️  Could not find Overview section, skipping pattern insertion');
  } else {
    // Find the end of the Overview section (next ## heading)
    const nextHeadingMatch = content.substring(insertIndex + insertAfter.length).match(/\n## /);
    if (nextHeadingMatch) {
      const insertPosition = insertIndex + insertAfter.length + nextHeadingMatch.index!;
      
      // Check if patterns section already exists
      if (!content.includes('🚨 CRITICAL: Valdi is NOT React!')) {
        content = 
          content.substring(0, insertPosition) + 
          '\n' + 
          VALDI_PATTERNS_SECTION + 
          '\n' + 
          content.substring(insertPosition);
        console.log('  ✅ Added Valdi patterns section');
      } else {
        console.log('  ℹ️  Patterns section already exists, preserving custom content');
      }
    }
  }
  
  // Update Provider pattern example if it exists
  if (content.includes('import { Provider }')) {
    console.log('  ⚠️  Found old Provider pattern, consider updating to withProviders() HOC');
  }
  
  fs.writeFileSync(templatePath, content, 'utf-8');
  console.log('  ✅ Template updated');
}

function main() {
  console.log('Updating AGENTS.md bootstrap templates with correct Valdi patterns...\n');
  
  const uiTemplate = path.join(CLI_ROOT, '.bootstrap/apps/ui_application/AGENTS.md.template');
  const cliTemplate = path.join(CLI_ROOT, '.bootstrap/apps/cli_application/AGENTS.md.template');
  
  if (fs.existsSync(uiTemplate)) {
    updateAgentsTemplate(uiTemplate, false);
  } else {
    console.warn(`UI template not found: ${uiTemplate}`);
  }
  
  if (fs.existsSync(cliTemplate)) {
    updateAgentsTemplate(cliTemplate, true);
  } else {
    console.warn(`CLI template not found: ${cliTemplate}`);
  }
  
  console.log('\n✅ All templates updated!');
  console.log('\nNext steps:');
  console.log('  1. Review the changes in the templates');
  console.log('  2. Test with: valdi bootstrap (in a test directory)');
  console.log('  3. Commit the updated templates');
}

main();
