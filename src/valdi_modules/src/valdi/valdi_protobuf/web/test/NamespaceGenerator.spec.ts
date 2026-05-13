// @ts-nocheck
import { IArena, IMessage, IMessageNamespace } from '../src_symlink/types';
import {
  generateMonkeyPatchedNamespace,
  ProtobufTsMessageWrapper,
} from '../headless/NamespaceGenerator';
import { Message as ValdiMessage } from '../Message';
import { getLoadedDescriptorDatabase } from './DescriptorDatabaseTestUtils';

// Import extracted types (interfaces + const enums) without native dependencies
// const enums are inlined at compile time, so no runtime module load
import { test } from './proto-types';

// Extended interface that includes the actual runtime methods
interface IRuntimeMessage extends IMessage {
  encode(): Uint8Array;
}

// Mock Arena for web tests - the namespace generator ignores it anyway
class MockArena implements IArena {
  createMessage(): never { throw new Error('Not implemented'); }
  decodeMessage(): never { throw new Error('Not implemented'); }
  decodeMessageAsync(): never { throw new Error('Not implemented'); }
  decodeMessageDebugJSONAsync(): never { throw new Error('Not implemented'); }
  encodeMessage(): never { throw new Error('Not implemented'); }
  encodeMessageAsync(): never { throw new Error('Not implemented'); }
  batchEncodeMessageAsync(): never { throw new Error('Not implemented'); }
  encodeMessageToJSON(): never { throw new Error('Not implemented'); }
  getMessageFields(): never { throw new Error('Not implemented'); }
  setMessageField(): never { throw new Error('Not implemented'); }
  getMessageInstance(): never { throw new Error('Not implemented'); }
  copyMessage(): never { throw new Error('Not implemented'); }
}

// Base64 encoding (ported from coreutils/src/Base64.ts)
const lookup = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
function tripletToBase64(num: number): string {
  return lookup[(num >> 18) & 0x3f] + lookup[(num >> 12) & 0x3f] + lookup[(num >> 6) & 0x3f] + lookup[num & 0x3f];
}
const Base64 = {
  fromByteArray(uint8: Uint8Array): string {
    const len = uint8.length;
    const extraBytes = len % 3;
    const parts: string[] = [];
    for (let i = 0, len2 = len - extraBytes; i < len2; i += 3) {
      const tmp = ((uint8[i] << 16) & 0xff0000) + ((uint8[i + 1] << 8) & 0xff00) + (uint8[i + 2] & 0xff);
      parts.push(tripletToBase64(tmp));
    }
    if (extraBytes === 1) {
      const tmp = uint8[len - 1];
      parts.push(lookup[tmp >> 2] + lookup[(tmp << 4) & 0x3f] + '==');
    } else if (extraBytes === 2) {
      const tmp = (uint8[len - 2] << 8) + uint8[len - 1];
      parts.push(lookup[tmp >> 10] + lookup[(tmp >> 4) & 0x3f] + lookup[(tmp << 2) & 0x3f] + '=');
    }
    return parts.join('');
  }
};

interface TypedMessageNamespace<I> {
  create: (arena: IArena, properties?: I) => I & IRuntimeMessage;
  decode: (arena: IArena, buffer: Uint8Array) => I & IRuntimeMessage;
}

function getTypedMessageNamespace<I>(getNamespace: (input: any) => any): TypedMessageNamespace<I> {
  const database = getLoadedDescriptorDatabase();
  const namespace = generateMonkeyPatchedNamespace(database, database.getAllMessageDescriptorTypeNames()) as any;

  const messageNamespace = getNamespace(namespace);
  if (!messageNamespace) {
    throw new Error('Could not resolve namespace');
  }
  return getNamespace(namespace) as unknown as TypedMessageNamespace<I>;
}

describe('NamespaceGenerator', () => {
  it('can generate monkey patched js namespaces', () => {
    const database = getLoadedDescriptorDatabase();
    const namespace = generateMonkeyPatchedNamespace(database, database.getAllMessageDescriptorTypeNames()) as any;
    const Message = namespace.test?.Message as IMessageNamespace;

    expect(Message).toBeTruthy();
    expect(Message.create).toBeTruthy();
    expect(Message.decodeAsync).toBeTruthy();
    expect(Message.encode).toBeTruthy();
    expect(Message.encodeAsync).toBeTruthy();
    expect(Message.decodeDebugJSONAsync).toBeTruthy();

    const message = Message.create(new MockArena(), {
      int32: 42,
      string: 'Hello World',
    });

    expect((message as test.IMessage).int32).toEqual(42);
    expect((message as test.IMessage).string).toEqual('Hello World');

    const encoded = message.encode();
    expect(Base64.fromByteArray(encoded)).toBe('CCpyC0hlbGxvIFdvcmxk');

    const decodedMessage = Message.decode(message.$arena, encoded);
    expect((decodedMessage as test.IMessage).int32).toEqual(42);
    expect((decodedMessage as test.IMessage).string).toEqual('Hello World');
  });

  it('ensures messages inherit Valdi messages', () => {
    const database = getLoadedDescriptorDatabase();
    const namespace = generateMonkeyPatchedNamespace(database, database.getAllMessageDescriptorTypeNames()) as any;
    const Message = namespace.test?.Message as IMessageNamespace;

    expect(Message).toBeTruthy();

    const message = Message.create(new MockArena(), {});
    expect(message instanceof ProtobufTsMessageWrapper).toBe(true);
    expect(message instanceof ValdiMessage).toBe(true);
  });

  it('can encode and decode message from monkey patched js namespace', () => {
    const Message = getTypedMessageNamespace<test.IMessage>(namespace => namespace.test?.Message);

    const message = Message.create(new MockArena(), {
      int32: 42,
      string: 'Hello World',
    });

    expect(message.int32).toEqual(42);
    expect(message.string).toEqual('Hello World');

    const encoded = message.encode();
    expect(Base64.fromByteArray(encoded)).toBe('CCpyC0hlbGxvIFdvcmxk');

    const decodedMessage = Message.decode(new MockArena(), encoded);
    expect((decodedMessage as test.IMessage).int32).toEqual(42);
    expect((decodedMessage as test.IMessage).string).toEqual('Hello World');
  });

  it('supports nested messages in monkey patched js namespace', () => {
    const ParentMessage = getTypedMessageNamespace<test.IParentMessage>(
      namespace => namespace.test?.ParentMessage,
    );

    const parentMessage = ParentMessage.create(new MockArena(), {
      childMessage: {
        value: 'Hello World',
      },
      childEnum: test.ParentMessage.ChildEnum.VALUE_1,
    });

    expect(parentMessage.childEnum).toBe(test.ParentMessage.ChildEnum.VALUE_1);
    expect(parentMessage.childMessage?.value).toBe('Hello World');
  });

  // Does not yet work
  xit('supports nested map messages in monkey patched js namespace', () => {
    const MapMessage = getTypedMessageNamespace<test.IMapMessage>(
      namespace => namespace.test?.MapMessage,
    );

    const message = MapMessage.create(new MockArena(), {
      stringToDouble: new Map([
        ['key1', 0],
        ['key2', 1],
      ]),
      stringToMessage: new Map([
        [
          'message',
          {
            value: 'Hello World',
          },
        ],
      ]),
    });

    expect(message.stringToDouble instanceof Map).toBe(true);
    expect(message.stringToMessage instanceof Map).toBe(true);

    expect(message.stringToDouble?.get('key1')).toBe(0);
    expect(message.stringToDouble?.get('key2')).toBe(1);

    const nestedMessage = message.stringToMessage?.get('message');
    expect(nestedMessage).toBeDefined();
    expect(nestedMessage instanceof ValdiMessage).toBeTruthy();
    expect((nestedMessage as any)?.value).toBe('Hello World');

    const encoded = message.encode();
    expect(Base64.fromByteArray(encoded)).toBe('');
  });
});
