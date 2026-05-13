# Valdi Localization

Internationalization (i18n) and localized strings in Valdi modules.

## When to use

When adding user-visible text that should be translatable across languages.

## Key concepts

Valdi localization uses **JSON string files** (`strings-en.json`) compiled into type-safe TypeScript accessors. The compiler generates a `Strings.d.ts` file with typed functions for each key. On iOS, strings compile to `.strings` files; on Android, to XML string resources.

## Setup

### 1. Create the strings file

Create `strings/strings-en.json` in your module directory:

```json
{
  "hello_world": {
    "defaultMessage": "Hello, world!"
  },
  "welcome_user": {
    "defaultMessage": "Welcome, {name}!",
    "example": "Welcome, Alex!"
  },
  "items_count": {
    "defaultMessage": "{count%d} items remaining",
    "example": "5 items remaining"
  }
}
```

### 2. Add strings directory to module config

In your `module.yaml`:

```yaml
strings_dir: strings
```

### 3. Generate typed accessors

```bash
valdi projectsync
```

This generates `Strings.d.ts` in your module with typed functions for each key.

### 4. Use in TypeScript

```tsx
import Strings from 'mymodule/src/Strings';

onRender(): void {
  <view>
    <label value={Strings.helloWorld()} />
    <label value={Strings.welcomeUser('Alex')} />
    <label value={Strings.itemsCount(5)} />
  </view>;
}
```

## String keys and naming

Keys in `strings-en.json` are `snake_case`. The generated TypeScript accessors are `camelCase`:

| JSON key | TypeScript accessor |
|----------|-------------------|
| `hello_world` | `Strings.helloWorld()` |
| `items_count` | `Strings.itemsCount(n)` |
| `welcome_user` | `Strings.welcomeUser(name)` |

## Template strings with variables

Variables use the format `{NAME%FORMAT}`:

```json
{
  "greeting": {
    "defaultMessage": "Hi {name}, you have {count%d} new messages",
    "example": "Hi Alex, you have 3 new messages"
  }
}
```

Supported formats:
- `{name}` or `{name%s}` — string (default)
- `{count%d}` — integer

Multiple variables must have unique names. Argument order in the TypeScript function matches the order they appear in the English string.

```tsx
// ✅ Variables passed in order of appearance in defaultMessage
Strings.greeting('Alex', 3)  // "Hi Alex, you have 3 new messages"
```

## Using strings in Kotlin (Android)

Generated strings are accessible as standard Android resources:

```kotlin
val text = getResources().getString(
  com.snap.valdi.modules.my_module.R.string.my_module_hello_world
)
```

## Common mistakes

```tsx
// ❌ WRONG — Hardcoding user-visible strings
<label value="Hello, world!" />

// ✅ Correct — use localized strings
<label value={Strings.helloWorld()} />

// ❌ WRONG — Concatenating localized strings
const msg = Strings.hello() + " " + name;  // ❌ Word order varies by language

// ✅ Correct — use template strings with variables
const msg = Strings.helloUser(name);

// ❌ WRONG — Constructing plurals in code
const msg = count === 1 ? "1 item" : `${count} items`;  // ❌

// ✅ Correct — handle plurals in the string definition
// (Use a single template string; translation handles pluralization)

// ❌ WRONG — Importing from the wrong path
import Strings from 'Strings';  // ❌

// ✅ Correct — import from your module's generated path
import Strings from 'mymodule/src/Strings';
```

## Workflow

1. Add keys to `strings/strings-en.json`
2. Run `valdi projectsync` to regenerate `Strings.d.ts`
3. Import `Strings` from your module and use the generated functions
4. Valdi tooling handles iOS `.strings` and Android `.xml` generation during build
