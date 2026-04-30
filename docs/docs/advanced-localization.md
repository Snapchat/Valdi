# Localization

## How to introduce new localizable strings?

### 1. Create a strings file
First, you need to declare your strings by creating a `strings-en.json` file in your module. For example:
```json
{
  "hello_world": {
    "defaultMessage": "Hello, world!"
  },
  "here_missed_call": {
    "defaultMessage": "{person} tried to call you",
    "example": "Bob tried to call you"
  },
  "minutes_ago": {
    "defaultMessage": "{minutes%d} minutes ago",
    "example": "42 minutes ago"
  },
  "person_gave_a_gift_to_person": {
    "defaultMessage": "{friend1} gave {friend2} a gift. How nice!",
    "example": "Alice gave Bob a gift. How nice!"
  },
}
```

Each entry within the `strings-en.json` file has a unique key and must be lower camelCased (or snake_cased, though we're likely to migrate from that for easier greppability).
* `defaultMessage`: Required. The base language (en) value for the key.
* `example`: Optional. An example string used to help translate the message.

Messages can contain uniquely named placeholders. By default, a placeholder is of type `string` but you may add `sprintf` style formatting such as `%s` and `%d` as a suffix to your named placeholder.

### 2. Add the strings directory to your module.yaml
```yaml
# Where to find your strings directory
strings_dir: strings
```

### 3. Generate `Strings.d.ts`

Run `valdi projectsync` to generate/update `Strings.d.ts` file for your module.

### 4. Use strings from TypeScript code:

Generated strings can be used directly in TypeScript code, e.g.:

```tsx
import Strings from 'mymodule/src/Strings';

// ...

onRender() {
  <view>
    <label value={ Strings.helloWorld() }/>
  </view>
}

// ...
```
Note that the `Strings` module exported property names are camelCased based on the snake_case localizable key provided in `strings.yaml`. E.g. in the above example the snake_case `hello_world` string is referred to using camelCase `helloWorld`.

### 5. Use strings from Kotlin code:

Generated strings can be used directly in Kotlin code as normal android resouces:

```kt
val helloWorld = getResources().getString(com.snap.valdi.modules.valdi_example.R.string.valdi_example_hello_world)
```

### 6. That's it!

Valdi tooling processes the input strings .json files for both the Base (English) localization and the translations. The tooling generates .strings files for iOS and strings .xml files for Android, which are included in the published Valdi modules artifacts.

> [!Note]
> The Valdi compiler automatically generates a TypeScript file exposing individual localizable strings, similar to the Asset Catalog. This ensures that using localized strings in TypeScript is type-safe and enables autocompletion in your IDE.


## Template Strings

In many instances, developers may require the utilization of strings that incorporate variables or placeholders - such as `"[5 unread notifications]"`. Normally in English, we could easily implement this via `numNotifications + " unread notifications"`. However, localization challenges arise with variations in languages, and spacing, ordering, and punctuation can all be different. For [5 unread notifications], the Korean translation is [읽지 않은 알림 5개]; simply `numNotifications + {localizedString}` would not suffice.

As such, we strongly recommend that no localized strings are constructed in code. Using longer strings allow our translations have the full context for each sentence, and we should not be worried about duplicate words and phrases as they are easily compressible. Instead, we can use variable interpolation into variables within template strings, so that they can be natually localized.

The variables have the following format: `{NAME%FORMAT}`.

These can be used as follows:
```tsx
// strings-en.json:
"notification_count": {
  "defaultMessage": "{num%d} unread notifications",
  "example": "5 unread notifications",
}

// calling code
const notificationCount = 5;
const localizedString = Strings.unreadNotifications(notificationCount);

// result - "5 unread notifications"
```

Formats that are currently accepted are:
- `%s` - the default format, the raw string is used
- `%d` - integer format

When multiple variables are used, they must be uniquely named. The ordering in the English (default) locale will be used to align passed in variables to variable names, and they will be matched to the correct position in localized strings. 
