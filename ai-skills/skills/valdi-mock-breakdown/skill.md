# valdi-mock-breakdown

Generate a Valdi TSX component from a screenshot or design mock. Analyzes the visual layout, decomposes it into Valdi primitives and widgets, and produces a complete `.tsx` file.

## When to use

- User provides a screenshot, Figma export, or design mock and wants Valdi code
- You're the "generator" in a mock-to-module pipeline
- User says "build this", "make this screen", or "turn this into code" with an image

## Overview

1. Analyze the image — identify layout structure, components, spacing, colors, typography
2. Map to Valdi primitives — `<view>`, `<label>`, `<image>`, `<scroll>`, `<shape>`, widgets
3. Generate a complete `.tsx` file with correct Valdi patterns

---

## Step 1: Analyze the layout

Look at the image and identify, top-to-bottom:

- **Overall structure**: Is it a full screen? A card? A list? A form?
- **Scroll behavior**: Does the content exceed the viewport? Use `<scroll>`.
- **Sections**: Group visually distinct areas (header, body, footer, list items).
- **Components**: Buttons, toggles, text inputs, checkboxes, pickers, etc.
- **Typography**: Title vs body vs caption — map to font sizes.
- **Colors**: Background, text, accent, divider — use semantic colors when possible.
- **Spacing**: Estimate padding and margins in 4pt/8pt increments.
- **Icons/images**: Note placement and sizing.

**Report your analysis** before writing code. Example:

> I see a vertical layout with:
> - A top bar with a back button and title ("Settings")
> - A scrollable list of 5 toggle rows, each with a label and a switch
> - A footer button ("Save Changes")
> - White background, 16px side padding, 12px between rows

---

## Step 2: Choose Valdi elements and widgets

### Primitives (always available, no extra deps)

Start with primitives to get a bare-bones skeleton on screen fast:

| Element | Use for |
|---------|---------|
| `<view>` | Containers, layout groups, spacers |
| `<label>` | All text (titles, body, captions) |
| `<image>` | Images, icons (use `src` prop with Asset) |
| `<scroll>` | Scrollable content areas |
| `<shape>` | Rounded rects, circles, dividers, decorative shapes |
| `<button>` | Tappable areas (has `title` and `onPress`) |

### Then upgrade to widget library components (if available)

Once the skeleton is rendering, check the workspace for a shared widget or component library (e.g. in `valdi_modules/widgets/` or a similar path). Pre-built widget components encode design system conventions (correct spacing, fonts, colors, shadows) and produce more accurate results with less hand-tuning.

**How to explore**: Search for exported component classes in the workspace's widget directories. Read the component's `ViewModel` interface to understand available props.

**Use icon assets instead of emoji** for better fidelity — search the workspace for an icon library class.

### Widgets (require module dep)

Before using a widget, search the codebase to confirm it exists and read its ViewModel:

```bash
# Find available widgets
grep -r "export class" --include="*.tsx" valdi_modules/widgets/src/components/
```

Common widgets in `widgets/`:
- `Checkbox` — toggle with `on`/`onTap`
- `Toggle` — switch with `on`/`onChange`
- `Section` — card-style section with title
- `SectionSeparator` — divider between sections
- `DatePicker`, `TimePicker`, `IndexPicker` — picker controls
- `EmojiLabel` — label with emoji rendering support
- `WithInsets` — safe area inset wrapper

**If the design has a component you're not sure about**, use primitives to approximate it rather than guessing a widget name that may not exist.

### Mapping visual patterns to elements

| Visual pattern | Primitives approach |
|---------------|---------------------|
| List row with icon + text | `<view>` with flexDirection row |
| Card with shadow/border | `<view>` with borderRadius + boxShadow |
| Section header with action | `<label>` + `<label>` in a row |
| Horizontal row | `<view>` with `flexDirection: 'row'` |
| Vertical stack | `<view>` (default column direction) |
| Divider line | `<view>` with `height: 1`, `backgroundColor: '#E0E0E0'` |
| Circular icon background | `<view>` with `borderRadius: size/2`, wrapping `<image>` |
| Tappable row | `onTap` on `<view>` |
| Scrollable list | `<scroll>` wrapping children |
| Fixed header + scrolling body | Outer `<view>` with header + `<scroll flexGrow={1}>` body |
| Spacer between items | margin on children (no `gap` property) |

---

## Step 3: Generate the .tsx file

### Template

```typescript
import { StatefulComponent } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { View, Label } from 'valdi_tsx/src/NativeTemplateElements';
// Import widgets as needed:
// import { Checkbox } from 'widgets/src/components/button/Checkbox';

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

/**
 * @Context
 * @ExportModel
 */
export interface ComponentContext {}

interface ComponentState {
  // State for interactive elements
}

/**
 * @Component
 * @ExportModel
 */
export class GeneratedComponent extends StatefulComponent<ViewModel, ComponentState, ComponentContext> {
  state: ComponentState = {
    // Initial state values
  };

  onRender(): void {
    <view style={styles.container}>
      {/* Layout here */}
    </view>;
  }
}

const styles = {
  container: new Style<View>({
    backgroundColor: '#ffffff',
    width: '100%',
    height: '100%',
  }),
};
```

### Critical Valdi rules

These are **NOT React** — common mistakes that MUST be avoided:

```typescript
// WRONG: React hooks (don't exist in Valdi)
const [value, setValue] = useState('');  // WRONG

// WRONG: Functional components (don't exist)
const MyThing = () => <view />;  // WRONG

// WRONG: Return JSX from onRender
onRender() { return <view />; }  // WRONG — onRender returns void

// WRONG: this.props (doesn't exist)
this.props.title  // WRONG — use this.viewModel.title

// CORRECT: Class component with state
class MyComponent extends StatefulComponent<VM, State> {
  state = { value: '' };
  onRender(): void {
    <view />;  // Statement, not return value
  }
}
```

### Styling guidelines

- Use `new Style<View>({...})` for view styles, `new Style<Label>({...})` for label styles
- Font sizes via font string: `font: 'system 16'`, `font: 'system-bold 20'`
- Use 8pt grid for spacing: 4, 8, 12, 16, 24, 32, 48
- Prefer `padding` and `margin` shorthand: `padding: '16 20'` (vertical horizontal)
- Colors: use hex strings `'#RRGGBB'` or `'#RRGGBBAA'`
- Layout: Yoga flexbox — `flexDirection`, `justifyContent`, `alignItems`, `flexGrow`
- No `gap` property — use margin on children instead
- No `fontSize` — use `font` string on labels
- Overflow: only `'visible'` or `'scroll'` (NOT `'hidden'`)
- **`opacity` must be a number**, not a string: `opacity: 0.3` (NOT `'0.3'`)
- **`flexGrow` must be a number**, not a string: `flexGrow: 1` (NOT `'1'`)
- **Only two font weights**: `system` (regular) and `system-bold` (bold). `system-semibold`, `system-medium`, etc. silently fall back to regular at runtime.
- **ScrollView has no `flexDirection`** — `<scroll>` always scrolls vertically, do not set flexDirection on it
- `boxShadow` format: `'xOffset yOffset blur color'` e.g. `'0 2 8 rgba(0,0,0,0.12)'`

### Style pitfalls (common build-breaking mistakes)

**Read the valdi-tsx skill's "Common Mistakes to Avoid" section before writing styles.** These cause the most build failures:

- No `borderRight`, `borderRightWidth`, etc. — only `borderWidth`, `borderColor`, `borderRadius` exist. Use a thin `<view>` as a divider.
- No `paddingHorizontal` / `paddingVertical` — use `padding: '10 20'` shorthand.
- No `flex: 1` — use `flexGrow: 1` (number, not string).
- `flexGrow` must be a number, not a string.
- `Shape` is not exported — use `ShapeView` from NativeTemplateElements.
- `font: 'system-semibold'` doesn't work — use `system-bold` or `system`.
- `overflow: 'hidden'` doesn't exist — only `'visible'` or `'scroll'`.
- `flexDirection` on `<scroll>` doesn't exist — ScrollView always scrolls vertically.

### Font size calibration (IMPORTANT)

Font sizes are the **#1 source of visual mismatch**. Initial instinct is almost always 30-50% too small. Use these reference ranges and scale up from your first guess:

| Visual role | Typical range | Example |
|------------|---------------|---------|
| Hero / display number | 64–80pt bold | Temperature "75°", score display |
| Section title | 20–24pt bold | "Settings", screen headers |
| Primary body text | 20–22pt regular | Descriptions, location names |
| Secondary label | 17–19pt regular/semibold | Tab labels, row titles |
| Tertiary / caption | 15–17pt regular | Timestamps, footnotes, axis labels |
| Small metadata | 12–14pt regular | Badges, counters |

**Rule of thumb**: If your first estimate is 16pt, it's probably 20-22pt. If your first estimate is 12pt, it's probably 16-17pt.

### Text color calibration

Avoid pure `#000000` for most text — real designs use lighter values:

| Text role | Color range |
|----------|-------------|
| Primary / headings | `#000000` – `#111111` |
| Body / descriptions | `#333333` – `#555555` |
| Secondary / metadata | `#888888` – `#AAAAAA` |
| Tertiary / hints | `#999999` – `#BBBBBB` |

### Spacing calibration

Vertical spacing between major sections (header → content → footer) is **consistently underestimated**. Use generous gaps:

- Between major sections: 28–44pt
- Between sub-items within a section: 12–20pt
- Card padding: 28–36pt vertical, 28–32pt horizontal
- Card border radius: 16–20pt for modern rounded appearance

### Charts and data visualization with `<shape>`

For line charts, area charts, or custom shapes, use `GeometricPathBuilder`:

```typescript
import { GeometricPathBuilder } from 'valdi_core/src/utils/GeometricPathBuilder';

// Create a builder with coordinate space dimensions
const b = new GeometricPathBuilder(width, height);
b.moveTo(x, y);
b.lineTo(x, y);
b.cubicTo(cp1x, cp1y, cp2x, cp2y, x, y);
b.close();
const path = b.build();

// Use the path in a <shape> element
<shape path={path} strokeColor="#E87A2E" strokeWidth={5}
       strokeCap="round" strokeJoin="round"
       width={width} height={height} />;
```

**Key patterns for smooth data curves:**

1. **Use monotone cubic Hermite interpolation** — prevents overshoot at local extrema. Simple midpoint control points cause curve artifacts.
2. **Stroke line**: 4–5pt width for prominent lines, `strokeCap="round"` for smooth endpoints.
3. **Data dots**: 10–12pt diameter circles using `<view>` with `borderRadius`.
4. **Fill path construction**: `moveTo(firstX, bottomY)` → `lineTo(firstX, firstY)` → trace curve → `lineTo(lastX, bottomY)` → `close()`.
5. **Fill color**: Use alpha for subtlety, e.g., `"#F5D0A088"`. Don't try layered fills to simulate gradients — they create visible horizontal bands.
6. **Data labels**: Position above dots with `position="absolute"`, offset `top: y - 30`, centered with fixed-width container.

### Naming

- Class name: `PascalCase`, descriptive of the screen/component (e.g., `SettingsScreen`, `ProfileCard`)
- Component file name: match the class name (e.g., `SettingsScreen.tsx`)
- Styles object: `const styles = { ... }` at bottom of file
- State interface: descriptive fields (e.g., `toggleOn`, `selectedIndex`, not `s1`, `v`)

---

## Step 4: Wire interactive state

For any interactive elements (toggles, checkboxes, pickers, text inputs):

1. Add state fields in the `ComponentState` interface
2. Set initial values in `state = { ... }`
3. Wire callbacks with `this.setState()`

```typescript
interface ComponentState {
  notificationsOn: boolean;
  selectedFruit: number;
}

// Define handlers as class properties (NOT inline arrows in onRender):
private toggleNotifications = (val: boolean) => this.setState({ notificationsOn: val });
private selectFruit = (i: number) => this.setState({ selectedFruit: i });

// In onRender:
<Checkbox
  on={this.state.notificationsOn}
  onTap={this.toggleNotifications}
/>

<IndexPicker
  labels={fruitLabels}
  index={this.state.selectedFruit}
  onChange={this.selectFruit}
/>
```

---

## Output format

When generating code for the mock-to-module pipeline, output:

1. **Analysis** — your visual breakdown (2-5 sentences)
2. **Component name** — what to call the generated class
3. **Full .tsx file** — complete, compilable, with all imports and styles
4. **Required deps** — list of module deps needed (e.g., `widgets`)

When generating code for a preview harness specifically, the component should be a `PreviewRoot` with empty ViewModel/ComponentContext, since it's a standalone root.
