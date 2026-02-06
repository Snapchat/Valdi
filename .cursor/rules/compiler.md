# Valdi Compiler Rules

**Applies to**: Swift files in `/compiler/compiler/`

## Overview

The Valdi compiler consists of two parts:

1. **Compiler (Swift)**: Transforms TypeScript/TSX into `.valdimodule` files
2. **Companion (TypeScript/Node.js)**: Handles TypeScript compilation, type checking, and provides debugging support

## Key Conventions

### Code Style

- **4-space indentation** for Swift
- Follow Swift naming conventions (camelCase for methods, PascalCase for types)
- Use Swift type inference where appropriate
- Prefer `let` over `var` when possible

### Architecture

- Compiler is a multi-pass system
- AST transformation pipeline
- Type checking and validation
- Code generation for each platform

### Important Files

- `/compiler/compiler/` - Main Swift compiler implementation
- Output: `.valdimodule` files (binary format read by runtime)

## Common Patterns

### AST Traversal

```swift
// Follow existing visitor patterns
class MyASTVisitor: ASTVisitor {
    func visit(_ node: Node) -> Result {
        // Visit logic
    }
}
```

### Error Handling

```swift
// Use proper error types
enum CompilerError: Error {
    case invalidSyntax(String)
    case typeError(String)
}
```

## Testing

Tests are critical - add tests for new features and error cases. The `update_compiler.sh` script runs tests automatically.

## Build

### Using the update script (recommended):

```bash
cd compiler/compiler
./scripts/update_compiler.sh ../../bin/compiler
```

This script:
- Runs `swift test` automatically
- Builds the compiler for the correct architecture
- Copies the binary to `bin/compiler/macos/valdi_compiler` (or `linux/valdi_compiler`)
- Handles platform differences (macOS vs Linux)

### Alternative: Using Xcode

Open `compiler/compiler/Compiler/Package.swift` in Xcode, let it resolve dependencies, then build.

## Companion (TypeScript)

The companion is a TypeScript service that works alongside the Swift compiler. It handles TypeScript compilation, type checking, and provides debugging support.

### Build the companion:

```bash
cd compiler/companion
./scripts/update_companion.sh ../../bin/compiler_companion
```

This script:
- Runs `npm install`
- Runs `npm run test`
- Builds with `bzl build //compiler/companion:bundle`
- Copies output to `bin/compiler_companion`

**Note**: Once built, the companion is automatically invoked by the compiler during compilation. You don't need to run it manually - it's part of the compiler process.

## Important Notes

1. **Performance matters** - Compiler speed affects developer experience
2. **Error messages** - Make them helpful and actionable
3. **Backward compatibility** - Don't break existing .valdimodule files
4. **Cross-platform** - Consider iOS, Android, macOS targets

## More Information

- Compiler architecture: `/compiler/compiler/README.md`
- Framework docs: `/AGENTS.md`
