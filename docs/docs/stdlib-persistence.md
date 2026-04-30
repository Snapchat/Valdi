# Persistent Storage (persistence)

The `persistence` module provides a simple key-value store that persists data across app sessions, with optional encryption and LRU caching.

> [!Note]
> The Valdi runtime provides a built-in **Disk Store** mechanism. This allows TypeScript components to persist data locally on the device, ensuring that user preferences, session tokens, and cached assets are available even after the application is restarted.

## Overview

`PersistentStore` allows you to store binary data or strings that survive app restarts. It supports:
- User-scoped or device-global storage
- Optional encryption for sensitive data
- TTL (time-to-live) for automatic expiration
- LRU cache behavior with maximum weight limits
- Batch writes for performance

## Installation

Add the `persistence` module to your `BUILD.bazel` dependencies:

```python
valdi_module(
    name = "my_module",
    deps = [
        "@valdi//src/valdi_modules/src/valdi/persistence",
    ],
)
```

## Basic Usage

```typescript
import { PersistentStore } from 'persistence/src/PersistentStore';

// Create a persistent store
const store = new PersistentStore('my_app_data');

// Store a string
await store.storeString('username', 'alice');

// Fetch a string
const username = await store.fetchString('username');
console.log(username); // 'alice'

// Store binary data
const binaryData = new Uint8Array([1, 2, 3, 4, 5]);
await store.store('binary_key', binaryData.buffer);

// Fetch binary data
const data = await store.fetch('binary_key');
console.log(new Uint8Array(data)); // [1, 2, 3, 4, 5]

// Remove data
await store.remove('username');
```

## API Reference

### PersistentStore

#### Constructor

```typescript
constructor(name: string, options?: PersistentStoreOptions)
```

Creates a new persistent store with the given name.

**Parameters:**
- `name` - Unique identifier for this store
- `options` - Optional configuration (see PersistentStoreOptions below)

```typescript
const store = new PersistentStore('user_preferences', {
    enableEncryption: true,
    maxWeight: 1024 * 1024 // 1MB cache
});
```

#### Methods

##### `store(key: string, value: ArrayBuffer, ttlSeconds?: number, weight?: number): Promise<void>`

Store binary data with the given key.

**Parameters:**
- `key` - Unique key for this data
- `value` - Binary data to store
- `ttlSeconds` - Optional time-to-live in seconds (data auto-expires)
- `weight` - Optional weight for LRU eviction (when maxWeight is set)

```typescript
const data = new Uint8Array([1, 2, 3, 4]);
await store.store('my_data', data.buffer);

// With TTL (expires in 1 hour)
await store.store('temporary', data.buffer, 3600);

// With weight for LRU cache
await store.store('cached_image', imageBuffer, undefined, 512 * 1024); // 512KB
```

##### `storeString(key: string, value: string, ttlSeconds?: number, weight?: number): Promise<void>`

Store a string with the given key.

```typescript
await store.storeString('user_name', 'Alice');

// With TTL (expires in 24 hours)
await store.storeString('session_token', 'abc123', 86400);
```

##### `fetch(key: string): Promise<ArrayBuffer>`

Fetch binary data for the given key. Throws if key doesn't exist.

```typescript
try {
    const data = await store.fetch('my_data');
    console.log(new Uint8Array(data));
} catch (error) {
    console.log('Key not found or expired');
}
```

##### `fetchString(key: string): Promise<string>`

Fetch a string for the given key. Throws if key doesn't exist.

```typescript
try {
    const username = await store.fetchString('user_name');
    console.log(username);
} catch (error) {
    console.log('Key not found or expired');
}
```

##### `exists(key: string): Promise<boolean>`

Test whether data for the given key exists (and hasn't expired).

```typescript
if (await store.exists('user_name')) {
    const username = await store.fetchString('user_name');
    console.log(username);
}
```

##### `remove(key: string): Promise<void>`

Remove data for the given key.

```typescript
await store.remove('user_name');
```

##### `removeAll(): Promise<void>`

Remove all data from this persistent store.

```typescript
// Clear all stored data
await store.removeAll();
```

##### `fetchAll(): Promise<PropertyList>`

Fetch all items as a PropertyList (map of key-value pairs).

```typescript
const allData = await store.fetchAll();
console.log(allData);
```

### PersistentStoreOptions

Configuration options for creating a persistent store.

```typescript
interface PersistentStoreOptions {
    disableBatchWrites?: boolean;
    deviceGlobal?: boolean;
    maxWeight?: number;
    enableEncryption?: boolean;
}
```

#### `disableBatchWrites?: boolean`

By default, save operations are batched to minimize disk I/O. Set to `true` to write immediately on every `store()` or `remove()` call.

```typescript
const store = new PersistentStore('immediate_writes', {
    disableBatchWrites: true // Writes happen immediately
});
```

**Default:** `false` (batching enabled for better performance)

#### `deviceGlobal?: boolean`

Whether the store should be available globally across all user sessions, instead of being scoped to the current user. Data will not be encrypted when this flag is set.

```typescript
// Device-global store (shared across all users)
const deviceStore = new PersistentStore('device_settings', {
    deviceGlobal: true
});

// User-scoped store (default)
const userStore = new PersistentStore('user_preferences');
```

**Default:** `false` (user-scoped)

#### `maxWeight?: number`

If set, the store acts like an LRU cache where items are removed as needed to keep total weight below this value. When using this, provide weight when calling `store()`.

```typescript
const imageCache = new PersistentStore('image_cache', {
    maxWeight: 10 * 1024 * 1024 // 10MB max
});

// Store with weight
const imageData = await loadImage();
await imageCache.store('image_123', imageData, undefined, 2 * 1024 * 1024); // 2MB
```

**Default:** `0` (no weight limit)

#### `enableEncryption?: boolean`

Set to `true` when storing sensitive data (credit cards, secret keys, authentication cookies). 

**Performance Note:** Encryption has some performance overhead. Use `false` for non-sensitive data.

```typescript
const secureStore = new PersistentStore('credentials', {
    enableEncryption: true
});

await secureStore.storeString('api_key', 'secret_key_123');
```

**Default:** `false` (no encryption)

## Common Use Cases

### User Preferences

```typescript
class UserPreferences {
    private store = new PersistentStore('user_prefs');
    
    async saveTheme(theme: 'light' | 'dark') {
        await this.store.storeString('theme', theme);
    }
    
    async getTheme(): Promise<'light' | 'dark'> {
        try {
            return await this.store.fetchString('theme') as 'light' | 'dark';
        } catch {
            return 'light'; // Default
        }
    }
}
```

### Session Management

```typescript
class SessionManager {
    private store = new PersistentStore('sessions', {
        enableEncryption: true
    });
    
    async saveSession(token: string) {
        // Expire after 7 days
        const ttl = 7 * 24 * 60 * 60;
        await this.store.storeString('auth_token', token, ttl);
    }
    
    async getSession(): Promise<string | null> {
        if (await this.store.exists('auth_token')) {
            return await this.store.fetchString('auth_token');
        }
        return null;
    }
    
    async clearSession() {
        await this.store.remove('auth_token');
    }
}
```

### Image Cache

```typescript
class ImageCache {
    private store = new PersistentStore('images', {
        maxWeight: 50 * 1024 * 1024 // 50MB cache
    });
    
    async cacheImage(url: string, imageData: ArrayBuffer) {
        const weight = imageData.byteLength;
        // Cache for 24 hours
        await this.store.store(url, imageData, 86400, weight);
    }
    
    async getImage(url: string): Promise<ArrayBuffer | null> {
        if (await this.store.exists(url)) {
            return await this.store.fetch(url);
        }
        return null;
    }
}
```

### Storing JSON

```typescript
async function storeJSON(store: PersistentStore, key: string, data: any) {
    const jsonString = JSON.stringify(data);
    await store.storeString(key, jsonString);
}

async function fetchJSON<T>(store: PersistentStore, key: string): Promise<T | null> {
    try {
        const jsonString = await store.fetchString(key);
        return JSON.parse(jsonString);
    } catch {
        return null;
    }
}

// Usage
const store = new PersistentStore('app_data');
await storeJSON(store, 'user', { name: 'Alice', age: 30 });
const user = await fetchJSON<{ name: string; age: number }>(store, 'user');
```

## Best Practices

1. **Use encryption for sensitive data**: Always set `enableEncryption: true` for passwords, tokens, and personal information.

2. **Choose appropriate TTL**: Set reasonable expiration times to avoid stale data and save storage space.

3. **User-scoped by default**: Unless you explicitly need device-global storage, keep the default user-scoped behavior.

4. **Batch writes for performance**: Keep the default batch write behavior unless you need guaranteed immediate persistence.

5. **Handle missing keys**: Always wrap `fetch()` and `fetchString()` in try-catch blocks or check with `exists()` first.

6. **Use weights for caches**: When using `maxWeight`, always provide meaningful weights to `store()` for proper LRU behavior.

7. **Store name uniqueness**: Use unique store names to avoid conflicts between different parts of your application.

## Error Handling

```typescript
async function safeGetString(store: PersistentStore, key: string, defaultValue: string): Promise<string> {
    try {
        return await store.fetchString(key);
    } catch (error) {
        console.warn(`Failed to fetch ${key}:`, error);
        return defaultValue;
    }
}

async function safeStore(store: PersistentStore, key: string, value: string): Promise<boolean> {
    try {
        await store.storeString(key, value);
        return true;
    } catch (error) {
        console.error(`Failed to store ${key}:`, error);
        return false;
    }
}
```

## Component Integration

```typescript
import { Component } from 'valdi_core/src/Component';
import { PersistentStore } from 'persistence/src/PersistentStore';

export class SettingsComponent extends Component {
    private store = new PersistentStore('settings');
    
    async onCreate() {
        // Load saved settings
        const theme = await this.loadTheme();
        this.setState({ theme });
    }
    
    private async loadTheme(): Promise<string> {
        try {
            return await this.store.fetchString('theme');
        } catch {
            return 'light'; // Default
        }
    }
    
    private async saveTheme(theme: string) {
        await this.store.storeString('theme', theme);
        this.setState({ theme });
    }
    
    onRender() {
        // Render settings UI...
    }
}
```

## Platform Support

The `persistence` module works on:
- ✅ iOS (uses file system storage)
- ✅ Android (uses file system storage)
- ⚠️ Web (requires polyfill or alternative implementation)

## See Also

- [Core Utilities](./stdlib-coreutils.md) - LRU Cache and other utilities
- [File System](./stdlib-filesystem.md) - Lower-level file operations
- [Component Lifecycle](./core-component.md) - Loading data in components
- [RxJS](./client-libraries-rxjs.md) - Reactive data patterns

## Performance Considerations

- **Batch writes are enabled by default** for better performance
- **Encryption has overhead** - only use for sensitive data
- **LRU caching helps** - use maxWeight to limit storage usage
- **TTL prevents bloat** - set reasonable expiration times

