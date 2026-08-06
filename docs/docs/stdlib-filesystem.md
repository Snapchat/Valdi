# File System (file_system)

The `file_system` module provides synchronous file system operations for Valdi applications.

> **⚠️ Internal Use Only**: This module is primarily for internal Valdi infrastructure usage. For most application needs, consider using [Persistent Storage](./stdlib-persistence.md) instead, which provides a higher-level, safer API for data persistence.

## Overview

The file system module exposes low-level synchronous file operations through a native C++ API. Operations are performed synchronously and may block the JavaScript thread, so use with caution.

## Installation

Add the `file_system` module to your `BUILD.bazel` dependencies:

```python
valdi_module(
    name = "my_module",
    deps = [
        "@valdi//src/valdi_modules/src/valdi/file_system",
    ],
)
```

## Basic Usage

```typescript
import { fs, VALDI_MODULES_ROOT } from 'file_system/src/FileSystem';

// Get current working directory
const cwd = fs.currentWorkingDirectory();
console.log('Working directory:', cwd);

// Read a file as string
const content = fs.readFileSync('/path/to/file.txt', { encoding: 'utf8' }) as string;
console.log(content);

// Read a file as binary
const buffer = fs.readFileSync('/path/to/file.dat') as ArrayBuffer;
console.log(new Uint8Array(buffer));

// Write a string to file
fs.writeFileSync('/path/to/output.txt', 'Hello, World!');

// Write binary data to file
const data = new Uint8Array([1, 2, 3, 4, 5]);
fs.writeFileSync('/path/to/output.dat', data.buffer);

// Create a directory
fs.createDirectorySync('/path/to/new/directory', true); // true = create intermediates

// Remove a file or directory
fs.removeSync('/path/to/file.txt');
```

## API Reference

### FileSystemModule

The main file system interface, accessed via the `fs` export.

#### `currentWorkingDirectory(): string`

Returns the current working directory path.

```typescript
const cwd = fs.currentWorkingDirectory();
console.log(cwd); // e.g., "/Users/username/project"
```

#### `readFileSync(path: string, options?: ReadFileOptions): string | ArrayBuffer`

Synchronously reads a file and returns its contents.

**Parameters:**
- `path` - Absolute or relative file path
- `options` - Optional encoding specification

**Returns:**
- If `encoding` is specified: returns `string`
- If `encoding` is omitted: returns `ArrayBuffer`

```typescript
// Read as UTF-8 string
const text = fs.readFileSync('config.json', { encoding: 'utf8' }) as string;
const config = JSON.parse(text);

// Read as UTF-16 string
const utf16Text = fs.readFileSync('data.txt', { encoding: 'utf16' }) as string;

// Read as binary
const binary = fs.readFileSync('image.png') as ArrayBuffer;
const bytes = new Uint8Array(binary);
```

#### `writeFileSync(path: string, data: ArrayBuffer | Uint8Array | string): void`

Synchronously writes data to a file, creating it if it doesn't exist or overwriting if it does.

**Parameters:**
- `path` - Absolute or relative file path
- `data` - String or binary data to write

```typescript
// Write string
fs.writeFileSync('output.txt', 'Hello, World!');

// Write binary
const imageData = new Uint8Array([...]); // image bytes
fs.writeFileSync('output.png', imageData);

// Write JSON
const config = { setting1: true, setting2: 'value' };
fs.writeFileSync('config.json', JSON.stringify(config, null, 2));
```

#### `createDirectorySync(path: string, createIntermediates: boolean): boolean`

Synchronously creates a directory.

**Parameters:**
- `path` - Directory path to create
- `createIntermediates` - If `true`, creates parent directories as needed (like `mkdir -p`)

**Returns:** `true` if successful, `false` otherwise

```typescript
// Create a single directory (fails if parent doesn't exist)
fs.createDirectorySync('/path/to/dir', false);

// Create directory with intermediates (like mkdir -p)
fs.createDirectorySync('/path/to/nested/dir', true);
```

#### `removeSync(path: string): boolean`

Synchronously removes a file or directory.

**Parameters:**
- `path` - File or directory path to remove

**Returns:** `true` if successful, `false` otherwise

**Warning:** Be careful with this operation - it's permanent!

```typescript
// Remove a file
fs.removeSync('/path/to/file.txt');

// Remove a directory
fs.removeSync('/path/to/directory');
```

### Types

#### `FileEncoding`

```typescript
type FileEncoding = 'utf8' | 'utf16';
```

Supported text encodings for reading files.

#### `ReadFileOptions`

```typescript
interface ReadFileOptions {
    encoding?: FileEncoding | undefined | null;
}
```

Options for reading files.

### Constants

#### `VALDI_MODULES_ROOT`

```typescript
const VALDI_MODULES_ROOT = './valdi_modules/src/valdi';
```

Path to the Valdi modules root directory.

## Use Cases

### Reading Configuration Files

```typescript
function loadConfig<T>(filename: string): T {
    try {
        const content = fs.readFileSync(filename, { encoding: 'utf8' }) as string;
        return JSON.parse(content);
    } catch (error) {
        console.error(`Failed to load config from ${filename}:`, error);
        throw error;
    }
}

const config = loadConfig<{ apiUrl: string; timeout: number }>('config.json');
```

### Writing Log Files

```typescript
function appendLog(message: string) {
    const timestamp = new Date().toISOString();
    const logEntry = `[${timestamp}] ${message}\n`;
    
    const logPath = `${fs.currentWorkingDirectory()}/app.log`;
    
    try {
        // Read existing log
        let existingLog = '';
        try {
            existingLog = fs.readFileSync(logPath, { encoding: 'utf8' }) as string;
        } catch {
            // File doesn't exist yet
        }
        
        // Append new entry
        fs.writeFileSync(logPath, existingLog + logEntry);
    } catch (error) {
        console.error('Failed to write log:', error);
    }
}
```

### Creating Build Artifacts

```typescript
function saveBuildArtifact(data: Uint8Array, filename: string) {
    const buildDir = `${fs.currentWorkingDirectory()}/build`;
    
    // Create build directory if it doesn't exist
    fs.createDirectorySync(buildDir, true);
    
    // Write artifact
    fs.writeFileSync(`${buildDir}/${filename}`, data.buffer);
}
```

## Important Warnings

### ⚠️ Synchronous Operations

All operations in this module are **synchronous** and **will block** the JavaScript thread. This can cause UI freezes if used with large files or slow storage.

```typescript
// ❌ BAD: Blocks UI thread
const largeFile = fs.readFileSync('large_video.mp4') as ArrayBuffer;

// ✅ BETTER: Use PersistentStore or async APIs for large data
```

### ⚠️ Platform Limitations

File system access may be restricted on iOS and Android:
- iOS: Limited to app sandbox
- Android: Limited to app private storage and external storage (with permissions)
- Web: Not available (use IndexedDB or similar instead)

### ⚠️ Security Considerations

Direct file system access can be dangerous:
- Be careful with user-provided paths (path traversal attacks)
- Don't store sensitive data without encryption
- Be aware of file permissions on different platforms

## Best Practices

1. **Prefer PersistentStore**: For most application data needs, use [PersistentStore](./stdlib-persistence.md) instead of direct file I/O.

2. **Use for build tools only**: This module is best suited for build scripts, code generation, and development tools.

3. **Handle errors**: Wrap all file operations in try-catch blocks.

4. **Check file existence**: Before reading, consider checking if files exist (though there's no direct exists() method).

5. **Use absolute paths**: When possible, use absolute paths to avoid ambiguity.

6. **Avoid in production UI**: Don't use synchronous file I/O in production UI code - it will freeze the interface.

## Error Handling

All methods can throw exceptions on failure. Always wrap in try-catch:

```typescript
function safeReadFile(path: string): string | null {
    try {
        return fs.readFileSync(path, { encoding: 'utf8' }) as string;
    } catch (error) {
        console.error(`Failed to read ${path}:`, error);
        return null;
    }
}

function safeWriteFile(path: string, data: string): boolean {
    try {
        fs.writeFileSync(path, data);
        return true;
    } catch (error) {
        console.error(`Failed to write ${path}:`, error);
        return false;
    }
}
```

## Alternative: Persistent Storage

For most application needs, **use PersistentStore instead**:

```typescript
// ❌ Using file_system for app data
import { fs } from 'file_system/src/FileSystem';
fs.writeFileSync('user_prefs.json', JSON.stringify(prefs));

// ✅ Better: Use PersistentStore
import { PersistentStore } from 'persistence/src/PersistentStore';
const store = new PersistentStore('user_prefs');
await store.storeString('preferences', JSON.stringify(prefs));
```

**Advantages of PersistentStore:**
- ✅ Asynchronous (doesn't block UI)
- ✅ Automatic encryption support
- ✅ TTL and LRU caching
- ✅ Batch writes for performance
- ✅ Cross-platform (including web)

## See Also

- [Persistent Storage](./stdlib-persistence.md) - **Recommended** higher-level storage API
- [Core Utilities](./stdlib-coreutils.md) - Additional utility functions
- [Advanced Worker Service](./advanced-worker-service.md) - For running file I/O in background threads

## Platform Support

- ✅ iOS (limited to app sandbox)
- ✅ Android (limited to app storage)
- ❌ Web (not available)

## Testing

When using file_system in tests, you may need to mock the native module:

```typescript
// In test setup
const mockFs = {
    readFileSync: jest.fn(),
    writeFileSync: jest.fn(),
    createDirectorySync: jest.fn(),
    removeSync: jest.fn(),
    currentWorkingDirectory: jest.fn(() => '/mock/path')
};
```
