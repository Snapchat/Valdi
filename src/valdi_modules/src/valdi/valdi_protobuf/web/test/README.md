# Valdi Protobuf Web Tests

## Type Definitions

The web tests need to reference proto message types without pulling in native Valdi dependencies (like `IArena`, `ProtobufMessage`, etc.) that don't work in a web environment.

### File Structure

- **`proto-types.d.ts`** - Auto-generated TypeScript type definitions (interfaces and enums only)
  - Extracted from `../../test/proto.d.ts`
  - Contains ONLY pure type definitions (interfaces and enums)
  - All `class` declarations and native imports are removed
  - **DO NOT EDIT MANUALLY** - regenerate using `extract_types.py`

- **`extract_types.py`** - Python script to extract pure types from `test/proto.d.ts`
  - Automatically filters out class declarations and native imports
  - Preserves all interfaces, enums, and namespaces
  - Auto-adds `Long` import if needed

### Regenerating Types

When the proto definitions in `../../proto/*.proto` change and `../../test/proto.d.ts` is regenerated:

```bash
cd web/test
python3 extract_types.py > proto-types.d.ts
```

### Why Not Use test/proto.d.ts Directly?

The file `../../test/proto.d.ts` is generated for native Valdi and includes:
- `import { IArena } from "valdi_protobuf/src/types"`  (native only)
- `import { Message as ProtobufMessage } from "valdi_protobuf/src/Message"` (native only)
- `export class Message extends ProtobufMessage<IMessage>` (native runtime classes)

**Why not use `import type { test } from '../../test/proto'`?**

TypeScript's `import type` only works for pure type annotations. We also use **enum values** at runtime (e.g., `test.ParentMessage.ChildEnum.VALUE_1`), which `import type` doesn't allow.

**How does proto-types.d.ts avoid runtime module loads?**

The extracted types use `const enum` instead of regular `enum`. TypeScript inlines `const enum` values at compile time:

```typescript
// Source:
childEnum: test.ParentMessage.ChildEnum.VALUE_1

// Compiled JS:
childEnum: 1  // <-- inlined literal, no module load!
```

So we get:
- ✅ Type safety for interfaces
- ✅ Runtime enum values (inlined at compile time)  
- ✅ No runtime dependencies on native modules
