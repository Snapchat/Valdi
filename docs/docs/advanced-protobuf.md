## Protobuf

Protobuf is a message serialization library written by Google and is widely used at Snap. Valdi has a complete integration of Protobuf provided as a built-in module. It leverages the Protobuf C++ library to perform most operations, and exposes type safe TypeScript APIs which are generated from a Protobuf definition file (`.proto`).

> [!Note]
> The Valdi runtime provides built-in support for Protobuf serialization. This allows TypeScript components to efficiently serialize and deserialize structured data for network requests, disk storage, and cross-language communication.

### Protobuf config file

The Valdi Protobuf generation script can take a `proto_config.yaml` and generate API definitions for the messages which the config file includes. This file needs to be added inside the root of a Valdi module directory, same location where the `module.yaml` lives.

The proto config file lists all the proto files which should be included from one of the repositories declared in the `repositories` section, or from files that are in the `proto` directory.

### Generating message APIs
Once the `proto_config.yaml` was setup in the module, we can generate the TypeScript bindings by running the `scripts/generate_protos.py` script (Pls. run  `tools/build_pre.sh` once from the client root directory to fetch all necessary prerequirements for `generate_protos` script). We provide it the Valdi module name where the `proto_config.yaml` is stored.
```bash
# Typical Valdi development path
cd ~/Path/to/repo/src/valdi_modules
# Generate protos
./scripts/generate_protos.py --module my_module
```
This will parse the `proto_config.yaml` file, resolve all the protobuf messages from the repositories declared in the `proto_config.yaml` files as well as files within `<module>/proto` and generate a TypeScript definition file as well as a binary which will be used to allow the Protobuf C++ library to parse the new message.

If you have not installed Protobuf on your machine, you can use Homebrew to install it:
```
brew install protobuf
```
or in Linux
```
apt install -y protobuf-compiler
```

After running `generate_protos.py`, the file structure from `my_module` will then look like this:
```
module.yaml
proto_config.yaml
src/proto.d.ts
src/proto.js
src/proto.protodecl
```

- `proto.d.ts` contains the TypeScript definitions for tall the messages.
- `proto.js` contains instructions to lazily load the messages implementation whenever `proto` is imported.
- `proto.protodecl` is a binary file containing the message definitions that the Protobuf C++ will be able to read.

Messages are exposed in namespaces to avoid conflicts. The namespace is based off the package name in protobuf.

## Using generated messages in TypeScript

### Setting up valdi_protobuf dependency

The generated messages will try to import utility classes from the `valdi_protobuf` module. We need to make sure we add a dependency to it in our `module.yaml`:
```
- dependencies:
  - valdi_core
  - valdi_protobuf
```

### Decoding a message

Messages can be decoded from a `Uint8Array` instance. We can decode a message by importing the Protobuf package from our `proto.d.ts` and calling `decode()` on the Message type we want to deserialize
```ts
import { snapchat } from './proto';
import { Arena } from 'valdi_protobuf/src/Arena';

const bytes: UInt8Array = getBytesFromSomewhere();
const story = snapchat.Story.decode(new Arena(), bytes);
// We can now use our decoded Story object!
```

### Encoding a message

Messages can be encoded in two ways: either by calling `MessageType.encode()` and passing a JavaScript object representing the message properties, or by calling `encode()` on a message instance which was previously created or decoded.

```ts
import { snapchat } from './proto';
import { Arena } from 'valdi_protobuf/src/Arena';

// Option 1
const storyBytes = snapchat.Story.encode({storyId: '42', user: {userId: '84', displayName: 'Bob'}});

// Option 2
const story = getStoryMessageObjectFromSomewhere();
const storyBytes = story.encode();
```

You can therefore easily decode an object, modify it and re-encode it:
```ts
const bytes: UInt8Array = getBytesFromSomewhere();
// Decode our Story message
const story = snapchat.Story.decode(new Arena(), bytes);
// Modify one of the field
story.storyId = '84';
// Encode the result
const newBytes = story.encode();
```

You can also create a Message instance from a JavaScript object if you absolutely need the Protobuf message representation before going into bytes:
```ts
const story = snapchat.Story.create(new Arena(), {storyId: '42', user: {userId: '84', displayName: 'Bob'}});
```

### Arena

Some of the message APIs, like `decode()` and `create()` take an `Arena` instance as their first parameter. The `Arena` is what holds all the native allocations for the Protobuf messages. Whenever setting a protobuf message instance into another protobuf message instance, the runtime will have to make a deep copy if both messages are not from the same `Arena` which can be expensive.
For instance:
```ts
const story = snapchat.Story.create(new Arena(), {storyId: '42'});
const user = snapchat.User.create(new Arena(), {userId: '84', displayName: 'Bob'});
// This will force the runtime to copy the User message inside Story, since the User message is from a different Arena than Story
story.user = user;
```

We can easily fix this as follow:
```ts
const arena = new Arena();
const story = snapchat.Story.create(arena, {storyId: '42'});
const user = snapchat.User.create(arena, {userId: '84', displayName: 'Bob'});
// No copies will happen, the User message is from the same Arena as Story.
story.user = user;
```
Note that this can also be written as follow:
```ts
const story = snapchat.Story.create(new Arena(), {storyId: '42', user: {userId: '84', displayName: 'Bob'}});
// We created a Story object with a User object inside in this single call. The objects will be from the same Arena.
```

Ideally you should use a single `Arena` instance when creating a complete Message tree. For instance, when serializing a request to a server, you'd use a dedicated Arena instance for representing the whole request.
`Arena` objects are designed to be used an thrown away. Don't re-use them as they will keep growing in memory.
