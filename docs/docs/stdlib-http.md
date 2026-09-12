# HTTP Client (valdi_http)

The `valdi_http` module provides a promise-based HTTP client for making network requests from Valdi applications.

## Overview

The HTTP client supports all standard HTTP methods (GET, POST, PUT, DELETE, HEAD) and returns cancelable promises, allowing you to abort in-flight requests.

## Installation

Add the `valdi_http` module to your `BUILD.bazel` dependencies:

```python
valdi_module(
    name = "my_module",
    deps = [
        "@valdi//src/valdi_modules/src/valdi/valdi_http",
    ],
)
```

## Basic Usage

```typescript
import { HTTPClient } from 'valdi_http/src/HTTPClient';

const client = new HTTPClient('https://api.example.com');

// GET request
const response = await client.get('/users');
console.log(response.statusCode); // 200
console.log(response.body); // Uint8Array

// POST request with body
const postData = new TextEncoder().encode(JSON.stringify({ name: 'Alice' }));
const postResponse = await client.post('/users', postData, {
    'Content-Type': 'application/json'
});
```

## API Reference

### HTTPClient

The main HTTP client class.

#### Constructor

```typescript
constructor(baseURL?: string)
```

Creates a new HTTP client with an optional base URL. If provided, all requests will be relative to this base URL.

```typescript
// With base URL
const client = new HTTPClient('https://api.example.com');
await client.get('/users'); // Requests https://api.example.com/users

// Without base URL (must use full URLs)
const client = new HTTPClient();
await client.get('https://api.example.com/users');
```

#### Methods

##### `get(pathOrUrl: string, headers?: StringMap<string>): CancelablePromise<HTTPResponse>`

Performs an HTTP GET request.

```typescript
const response = await client.get('/api/data', {
    'Authorization': 'Bearer token123',
    'Accept': 'application/json'
});
```

##### `post(pathOrUrl: string, body?: ArrayBuffer | Uint8Array, headers?: StringMap<string>): CancelablePromise<HTTPResponse>`

Performs an HTTP POST request with optional body.

```typescript
const jsonData = JSON.stringify({ username: 'alice', email: 'alice@example.com' });
const body = new TextEncoder().encode(jsonData);

const response = await client.post('/api/users', body, {
    'Content-Type': 'application/json'
});
```

##### `put(pathOrUrl: string, body?: ArrayBuffer | Uint8Array, headers?: StringMap<string>): CancelablePromise<HTTPResponse>`

Performs an HTTP PUT request with optional body.

```typescript
const updateData = new TextEncoder().encode(JSON.stringify({ name: 'Alice Updated' }));
const response = await client.put('/api/users/123', updateData, {
    'Content-Type': 'application/json'
});
```

##### `delete(pathOrUrl: string, headers?: StringMap<string>): CancelablePromise<HTTPResponse>`

Performs an HTTP DELETE request.

```typescript
const response = await client.delete('/api/users/123', {
    'Authorization': 'Bearer token123'
});
```

##### `head(pathOrUrl: string, headers?: StringMap<string>): CancelablePromise<HTTPResponse>`

Performs an HTTP HEAD request (returns only headers, no body).

```typescript
const response = await client.head('/api/resource');
console.log(response.headers['Content-Length']);
```

## Types

### HTTPRequest

```typescript
interface HTTPRequest {
    url: string;
    method: HTTPMethod;
    headers?: StringMap<string>;
    body?: ArrayBuffer | Uint8Array;
    priority?: number;
}
```

### HTTPResponse

```typescript
interface HTTPResponse {
    headers: StringMap<string>;
    statusCode: number;
    body?: Uint8Array;
}
```

Response from an HTTP request.

**Properties:**
- `statusCode` - HTTP status code (200, 404, 500, etc.)
- `headers` - Response headers as a string map
- `body` - Response body as `Uint8Array` (optional)

```typescript
const response = await client.get('/api/data');

if (response.statusCode === 200) {
    // Decode response body
    const text = new TextDecoder().decode(response.body);
    const data = JSON.parse(text);
    console.log(data);
}
```

### HTTPMethod

```typescript
enum HTTPMethod {
    GET = 'GET',
    POST = 'POST',
    DELETE = 'DELETE',
    PUT = 'PUT',
    HEAD = 'HEAD',
}
```

## Canceling Requests

All HTTP methods return `CancelablePromise`, which allows you to cancel in-flight requests:

```typescript
const request = client.get('/api/large-file');

// Cancel the request after 5 seconds
setTimeout(() => {
    request.cancel();
}, 5000);

try {
    const response = await request;
    console.log('Request completed');
} catch (error) {
    console.log('Request was canceled or failed');
}
```

## Working with JSON

### Sending JSON

```typescript
function sendJSON(client: HTTPClient, path: string, data: any): Promise<HTTPResponse> {
    const jsonString = JSON.stringify(data);
    const body = new TextEncoder().encode(jsonString);
    
    return client.post(path, body, {
        'Content-Type': 'application/json'
    });
}

const response = await sendJSON(client, '/api/users', {
    name: 'Bob',
    email: 'bob@example.com'
});
```

### Receiving JSON

```typescript
async function getJSON<T>(client: HTTPClient, path: string): Promise<T> {
    const response = await client.get(path, {
        'Accept': 'application/json'
    });
    
    if (response.statusCode !== 200) {
        throw new Error(`HTTP ${response.statusCode}`);
    }
    
    const text = new TextDecoder().decode(response.body);
    return JSON.parse(text);
}

interface User {
    id: number;
    name: string;
    email: string;
}

const user = await getJSON<User>(client, '/api/users/123');
console.log(user.name);
```

## Error Handling

```typescript
async function fetchData() {
    try {
        const response = await client.get('/api/data');
        
        if (response.statusCode >= 200 && response.statusCode < 300) {
            // Success
            const text = new TextDecoder().decode(response.body);
            return JSON.parse(text);
        } else {
            // HTTP error
            throw new Error(`HTTP error: ${response.statusCode}`);
        }
    } catch (error) {
        // Network error or request canceled
        console.error('Request failed:', error);
        throw error;
    }
}
```

## Advanced Usage

### Custom Headers

```typescript
const client = new HTTPClient('https://api.example.com');

const commonHeaders = {
    'Authorization': 'Bearer my-token',
    'X-App-Version': '1.0.0',
    'User-Agent': 'MyApp/1.0'
};

const response = await client.get('/api/data', commonHeaders);
```

### Uploading Binary Data

```typescript
async function uploadFile(client: HTTPClient, file: Uint8Array) {
    const response = await client.post('/api/upload', file, {
        'Content-Type': 'application/octet-stream'
    });
    
    return response;
}
```

### Request Priority

Some implementations support request prioritization:

```typescript
const request: HTTPRequest = {
    url: '/api/critical-data',
    method: HTTPMethod.GET,
    headers: { 'Authorization': 'Bearer token' },
    priority: 10 // Higher priority
};
```

## Best Practices

1. **Reuse HTTP client instances**: Create one client per base URL and reuse it across your application.

2. **Cancel unnecessary requests**: Use the cancelable promise to abort requests when components unmount or data is no longer needed.

3. **Handle errors gracefully**: Always check status codes and handle network errors.

4. **Use appropriate timeouts**: Cancel long-running requests that exceed expected timeouts.

5. **Decode response bodies**: Remember that `body` is always `Uint8Array` - decode appropriately for text/JSON.

## Integration with Components

```typescript
import { Component } from 'valdi_core/src/Component';
import { HTTPClient } from 'valdi_http/src/HTTPClient';

export class UserListComponent extends Component {
    private httpClient = new HTTPClient('https://api.example.com');
    private currentRequest?: CancelablePromise<HTTPResponse>;
    
    onCreate() {
        this.loadUsers();
    }
    
    private async loadUsers() {
        try {
            this.currentRequest = this.httpClient.get('/api/users');
            const response = await this.currentRequest;
            
            const text = new TextDecoder().decode(response.body);
            const users = JSON.parse(text);
            
            this.setState({ users });
        } catch (error) {
            console.error('Failed to load users:', error);
        }
    }
    
    onDestroy() {
        // Cancel any in-flight requests when component is destroyed
        if (this.currentRequest) {
            this.currentRequest.cancel();
        }
    }
    
    onRender() {
        // Render users...
    }
}
```

## See Also

- [Protobuf](./advanced-protobuf.md) - For structured binary protocols
- [RxJS](./client-libraries-rxjs.md) - For reactive HTTP patterns
- [Native Bindings](./native-bindings.md) - Type conversion details
- [Cancelable Promise](./stdlib-coreutils.md) - Promise cancellation patterns

## Platform Support

The `valdi_http` module works on:
- ✅ iOS
- ✅ Android
- ✅ Web (via polyfill)
- ✅ CLI (requires `enable_http = True`, see [Building a CLI application](./workflow-cli-application.md#making-network-requests))

Network requests are always performed asynchronously and will not block the JavaScript thread.

### CLI differences

The CLI's HTTP client is libcurl rather than a platform user agent, so a few things differ from iOS
and Android:

- **No response compression.** Responses arrive uncompressed, and a response that carries a
  `Content-Encoding` this build cannot decode fails rather than handing back unreadable bytes.
- **`Accept-Encoding` and `Content-Length` request headers are ignored**, as `fetch()` ignores them
  on the web. The transport sets both itself.
- **A non-standard method survives a 303 redirect** instead of becoming `GET`. Standard methods are
  rewritten per RFC 9110; its body is dropped either way.
- **Certificates verify against the system trust store**, found via `SSL_CERT_FILE`, `SSL_CERT_DIR`,
  `CURL_CA_BUNDLE` or `CURL_CA_PATH`, or else the usual distribution paths. If none is found, every
  HTTPS request fails rather than succeeding unverified.
