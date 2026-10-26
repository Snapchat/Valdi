---
title: Server-side rendering prototype
---

The `valdi_ssr` module runs the normal Valdi Web renderer against a server-side
DOM and serializes the resulting tree as HTML. It supports an initial response
and later streamed renders using Declarative Partial Updates.

This is a prototype for server-rendered output. Event listeners, layout
measurement, visibility, media playback, navigation, and browser-only custom
behavior are inert. Each streamed patch replaces the complete Valdi root, so
browser-local DOM state is not retained.

## Run the example

```sh
bazel run //apps/ssr_example:server
```

Open `http://127.0.0.1:8080/` for a static render or
`http://127.0.0.1:8080/stream` for five server-driven updates. Streaming uses
HTTP/1.1 chunked transfer encoding and the browser's Declarative Partial
Updates implementation.

## Add a route

Each route associates a component class with a builder that derives its view
model and component context from the incoming request:

```typescript
import { ValdiSSRRouter } from 'valdi_ssr/src/ValdiSSRRouter';

const router = new ValdiSSRRouter('My Valdi application');
router.add('/example', ExampleComponent, request => ({
  componentContext: {},
  viewModel: { requestTarget: request.target },
}));

await router.listen(8080);
```

A route may provide `startViewModelStream`. It receives the renderer callback
for later view models and returns a cleanup callback that runs when the client
disconnects.

Run the framework tests with:

```sh
bazel test //src/valdi_modules/src/valdi/valdi_ssr:test
```
