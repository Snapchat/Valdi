# Cursor Rules for Valdi

This directory contains context-specific rules for AI coding assistants (like Cursor) working on different parts of the Valdi codebase.

## How Cursor Rules Work

Cursor automatically loads rules from this directory based on the files you're working on. This means:

- Working in `/compiler/` → `compiler.md` rules apply
- Working in `/apps/` or `/src/valdi_modules/` → `typescript-tsx.md` rules apply
- Working in `/valdi/`, `/valdi_core/`, `/snap_drawing/` → `cpp-runtime.md` rules apply
- Working in `/bzl/` → `bazel.md` rules apply

## Available Rules

| File | Applies To | Description |
|------|-----------|-------------|
| `typescript-tsx.md` | `**/*.ts`, `**/*.tsx` in src/valdi_modules/, apps/ | Valdi component patterns, anti-React hallucination warnings |
| `compiler.md` | `/compiler/**/*.swift` | Swift compiler implementation guidelines |
| `cpp-runtime.md` | `/valdi/**/*.{cpp,hpp}`, `/valdi_core/**/*.{cpp,hpp}` | C++ runtime and layout engine conventions |
| `android.md` | `/valdi/**/android/**/*.{kt,java}` | Kotlin/Java Android runtime patterns |
| `ios.md` | `/valdi/**/ios/**/*.{m,mm,h}` | Objective-C/C++ iOS runtime patterns |
| `bazel.md` | `**/BUILD.bazel`, `/bzl/**/*.bzl` | Bazel build system conventions |
| `testing.md` | `**/test/**/*.ts`, `**/*.spec.ts` | Testing framework and patterns |

## Rule Priority

1. **Most specific rule wins** - File in `/valdi/src/valdi/ios/` gets iOS rules
2. **Multiple rules can apply** - TypeScript files get both `typescript-tsx.md` and `testing.md` if in test directory
3. **Root `.cursorrules`** - General warnings that apply everywhere

## Adding New Rules

1. Create a new `.md` file in this directory
2. Add frontmatter to specify when it applies (optional, based on directory)
3. Document the specific patterns/conventions for that area
4. Update this README

## Important

All rules should reference AGENTS.md for comprehensive framework documentation. Rules here are quick reminders, not exhaustive documentation.

## Open Source Project

⚠️ This is an open source project. Never include secrets, API keys, or proprietary information.
