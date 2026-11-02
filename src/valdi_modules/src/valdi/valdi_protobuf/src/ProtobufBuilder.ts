import { trace } from 'valdi_core/src/utils/Trace';
import { Arena } from './Arena';
import Descriptor from './Descriptor';
import { ValdiProtobufModule, IField, INativeMessageFactory, INativeNamespaceEntries } from './ValdiProtobuf';
import { getProtobufModule } from './ValdiProtobufModule';
import { IArena, IDescriptor, IDescriptorPool, IMessage, IMessageNamespace } from './types';

function createMessageNamespace(descriptor: IDescriptor): IMessageNamespace {
  function createMessage(arena: IArena | undefined, value?: any): IMessage {
    if (!arena) {
      throw Error('Must provide an Arena instance');
    }

    const message = arena.createMessage(descriptor.getConstructor());
    if (value) {
      descriptor.setProperties(arena, message, value);
    }

    return message;
  }
  return {
    create: createMessage,
    decode: (arena: IArena | undefined, data: Uint8Array): IMessage => {
      if (!arena) {
        throw Error('Must provide an Arena instance');
      }

      return arena.decodeMessage(descriptor.getConstructor(), data);
    },
    decodeAsync: (arena: IArena | undefined, data: Uint8Array): Promise<IMessage> => {
      if (!arena) {
        throw Error('Must provide an Arena instance');
      }

      return arena.decodeMessageAsync(descriptor.getConstructor(), data);
    },

    decodeDebugJSONAsync: (arena: IArena | undefined, json: string): Promise<IMessage> => {
      if (!arena) {
        throw Error('Must provide an Arena instance');
      }

      return arena.decodeMessageDebugJSONAsync(descriptor.getConstructor(), json);
    },

    encode: (value: IMessage | any): Uint8Array => {
      if (value.encode) {
        return value.encode();
      }
      const arena = new Arena();
      const instance = createMessage(arena, value);
      return instance.encode();
    },

    encodeAsync: (value: IMessage | any): Promise<Uint8Array> => {
      if (value.encodeAsync) {
        return value.encodeAsync();
      }
      const arena = new Arena();
      const instance = createMessage(arena, value);
      return instance.encodeAsync();
    },
  };
}

class DescriptorPool implements IDescriptorPool {
  private protobuf: ValdiProtobufModule;
  readonly namespaces: Record<string, any>;

  private descriptors: (IDescriptor | undefined)[] = [];

  get descriptorsLength(): number {
    return this.descriptorNames.length;
  }

  private packageNamespaceById: { [key: number]: Record<string, any> } = {};
  private messageNamespaceById: { [key: number]: IMessageNamespace } = {};

  constructor(
    readonly nativeMessageFactory: INativeMessageFactory,
    readonly descriptorNames: string[],
    rootNamespaceEntries: INativeNamespaceEntries,
  ) {
    this.protobuf = getProtobufModule();
    if (!rootNamespaceEntries) {
      const errorMessage = `Your client branch is more recent than your iOS/android checkout. Please update your iOS or android checkout and rebuild the app.`;
      console.error(errorMessage);
      throw new Error(errorMessage);
    }
    this.namespaces = this.createPackageNamespace(rootNamespaceEntries);
  }

  private createPackageNamespace(entries: INativeNamespaceEntries): Record<string, any> {
    const output: Record<string, any> = {};
    let i = 0;
    const length = entries.length;
    while (i < length) {
      const typeEntry = entries[i++] as number;
      const name = entries[i++] as string;

      const id = typeEntry >> 1;
      const isMessage = !!(typeEntry & 1);

      if (isMessage) {
        this.insertMessageNamespace(name, id, output);
      } else {
        this.insertPackageNamespace(name, id, output);
      }
    }

    return output;
  }

  private doDefineGetter(name: string, output: Record<string, any>, getter: () => any) {
    const descriptor = Object.getOwnPropertyDescriptor(output, name);
    if (!descriptor || !descriptor.get) {
      // New property, we can assign it directly
      Object.defineProperty(output, name, {
        enumerable: true,
        configurable: true,
        get: getter,
      });
    } else {
      // Property already exist. We need to merge it
      const key = Symbol();
      const previousGetter = descriptor.get;
      Object.defineProperty(output, name, {
        enumerable: true,
        configurable: true,
        get: () => {
          let value = (output as any)[key];
          if (!value) {
            value = { ...previousGetter(), ...getter() };
            (output as any)[key] = value;
          }

          return value;
        },
      });
    }
  }

  private insertPackageNamespace(name: string, id: number, output: Record<string, any>) {
    this.doDefineGetter(name, output, () => {
      let resolvedNamespace = this.packageNamespaceById[id];
      if (!resolvedNamespace) {
        const namespaceEntries = this.protobuf.getNamespaceEntries(this.nativeMessageFactory, id);
        resolvedNamespace = this.createPackageNamespace(namespaceEntries);
        this.packageNamespaceById[id] = resolvedNamespace;
      }
      return resolvedNamespace;
    });
  }

  private insertMessageNamespace(name: string, id: number, output: Record<string, any>) {
    this.doDefineGetter(name, output, () => {
      let resolvedNamespace = this.messageNamespaceById[id];
      if (!resolvedNamespace) {
        resolvedNamespace = createMessageNamespace(this.getDescriptorForIndex(id));
        this.messageNamespaceById[id] = resolvedNamespace;
      }
      return resolvedNamespace;
    });
  }

  getDescriptorForIndex(index: number): IDescriptor {
    let descriptor = this.descriptors[index];

    if (!descriptor) {
      const nameSplit = this.descriptorNames[index].split('.');
      const name = nameSplit[nameSplit.length - 1];
      const namespaceKeyEntries = nameSplit.slice(0, nameSplit.length - 1);
      const namespaceKey = namespaceKeyEntries.join('.');

      descriptor = new Descriptor(namespaceKey, name, index, this);
      this.descriptors[index] = descriptor;
    }

    return descriptor;
  }

  getFieldsForDescriptorIndex(descriptorIndex: number): readonly IField[] {
    return this.protobuf.getFieldsForMessageDescriptor(this.nativeMessageFactory, descriptorIndex);
  }

  getMessageNamespace(messagePath: string): IMessageNamespace | undefined {
    const components = messagePath.split('.');
    let current = this.namespaces;

    for (const component of components) {
      current = current[component];
      if (!current) {
        return undefined;
      }
    }

    if (current && current.decodeAsync) {
      // Found our namespace
      return current as IMessageNamespace;
    }

    return undefined;
  }
}

/**
 * Load and return a Protobuf DescriptorPool from the content of proto files.
 * This will parse the given proto file as a string and return a descriptor
 * that allows to interact with the Protobuf messages.
 * The "filename" is used to identify the file internally and does not need
 * to match a file on disk.
 */
export function loadDescriptorPoolFromProtoFiles(filename: string, protoFilesContent: string): IDescriptorPool {
  const protobuf = getProtobufModule();
  if (!protobuf.parseAndLoadMessages) {
    throw new Error('Parsing Proto files is not available in this build type');
  }

  return trace(`Protobuf.loadFromProtos`, () => {
    const result = protobuf.parseAndLoadMessages!(filename, protoFilesContent);
    return new DescriptorPool(result.factory, result.descriptorNames, result.rootNamespaceEntries);
  });
}

/**
 * Load and return the Protobuf message namespaces from a path.
 */
export function load(path: string): any {
  return trace(`Protobuf.load.${path}`, () => {
    const result = getProtobufModule().loadMessages(path);
    if (result.namespaces) {
      return result.namespaces;
    } else {
      return new DescriptorPool(result.factory, result.descriptorNames, result.rootNamespaceEntries).namespaces;
    }
  });
}
