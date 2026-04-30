# valdi-migrate skill tests

Validates that code migrated to Valdi using the `valdi-migrate` skill is free of
anti-patterns (React hooks, Compose APIs, inline lambdas, `map()` in render loops, etc.)
and that the output actually compiles.

## Files

| File | Description |
|------|-------------|
| `BUILD.bazel` | Bazel build target — compiles `src/` and `output/` together |
| `tsconfig.json` | TypeScript config |
| `check_antipatterns.py` | Linter — exits 0 if clean, 1 if violations found |
| `flutter_example.dart` | Source Flutter code to migrate |
| `compose_example.kt` | Source Jetpack Compose code to migrate |
| `react_example.tsx` | Source React code to migrate |
| `src/expected_valdi.tsx` | Reference correct Valdi output (canonical ground truth) |
| `output/` | Skill-generated migrations — checked in after running integration tests |

## Integration tests

These tests verify the skill produces correct, compilable Valdi from each source framework.

### 1. Generate migrations

Ask Claude (with the `valdi-migrate` skill active) to migrate each source file:

```
Migrate flutter_example.dart to Valdi
Migrate compose_example.kt to Valdi
Migrate react_example.tsx to Valdi
```

Save each output to the `output/` directory:
- `output/flutter_migrated.tsx`
- `output/compose_migrated.tsx`
- `output/react_migrated.tsx`

### 2. Anti-pattern check

Verify none of the outputs contain framework-specific patterns:

```bash
cd client/src/open_source/ai-skills/skills/valdi-migrate/tests

python3 check_antipatterns.py output/flutter_migrated.tsx   # must pass
python3 check_antipatterns.py output/compose_migrated.tsx   # must pass
python3 check_antipatterns.py output/react_migrated.tsx     # must pass

# Sanity check — source files should still fail
python3 check_antipatterns.py react_example.tsx             # expected: fail
python3 check_antipatterns.py compose_example.kt            # expected: fail
```

### 3. Compile check

Verify all outputs (reference + migrations) build with the Valdi compiler:

```bash
cd client/src/open_source
bazel build //ai-skills/skills/valdi-migrate/tests:tests
```

Expected: `Build completed successfully`

## Updating the tests

- **Source examples** (`flutter_example.dart`, `compose_example.kt`, `react_example.tsx`): add new patterns you want the skill to handle correctly.
- **Reference output** (`src/expected_valdi.tsx`): update when Valdi APIs change (import paths, type signatures, etc.). Must always compile and pass the linter.
- **Skill** (`../skill.md`): update when adding new source frameworks or fixing incorrect migration guidance.
