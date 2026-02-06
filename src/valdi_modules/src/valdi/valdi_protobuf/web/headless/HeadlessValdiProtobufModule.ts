import {
  ValdiProtobufModule,
  IField,
  ILoadMessageResult,
  INativeMessageArena,
  INativeMessageFactory,
  INativeNamespaceEntries,
  NativeFieldValues,
} from '../ValdiProtobuf';
import { DescriptorDatabase } from './DescriptorDatabase';
import { generateMonkeyPatchedNamespace } from './NamespaceGenerator';

const DUMMY_ARENA: INativeMessageArena = {};

export class HeadlessValdiProtobufModule implements ValdiProtobufModule {
  constructor(private readonly loadProtoDeclFn: (path: string) => Uint8Array) {}

  createArena(eagerDecoding: boolean, ignoreAllFieldsDuringEncoding: boolean): INativeMessageArena {
    return DUMMY_ARENA;
  }

  loadMessages(path: string): ILoadMessageResult {
    const protoDeclContent = this.loadProtoDeclFn(path);
    const database = new DescriptorDatabase();
    database.addFileDescriptorSet(protoDeclContent);
    database.resolve();

    const allMessageNames = database.getAllMessageDescriptorTypeNames();

    const namespace = generateMonkeyPatchedNamespace(database, allMessageNames);

    return {
      factory: {},
      namespaces: namespace,
      rootNamespaceEntries: [],
      descriptorNames: allMessageNames.map(n => n.fullName),
    };
  }

  getFieldsForMessageDescriptor(factory: INativeMessageFactory, messageDescriptorIndex: number): IField[] {
    throw new Error('Method not implemented.');
  }

  getNamespaceEntries(factory: INativeMessageFactory, namespaceId: number): INativeNamespaceEntries {
    throw new Error('Method not implemented.');
  }

  createMessage(arena: INativeMessageArena, factory: INativeMessageFactory, messageDescriptorIndex: number): number {
    throw new Error('Method not implemented.');
  }

  decodeMessage(
    arena: INativeMessageArena,
    factory: INativeMessageFactory,
    messageDescriptorIndex: number,
    data: Uint8Array,
  ): number {
    throw new Error('Method not implemented.');
  }
  decodeMessageAsync(
    arena: INativeMessageArena,
    factory: INativeMessageFactory,
    messageDescriptorIndex: number,
    data: Uint8Array,
    callback: (messageIndex: number | undefined, error: string | undefined) => void,
  ): void {
    throw new Error('Method not implemented.');
  }
  decodeMessageDebugJSONAsync(
    arena: INativeMessageArena,
    factory: INativeMessageFactory,
    messageDescriptorIndex: number,
    data: string,
    callback: (messageIndex: number | undefined, error: string | undefined) => void,
  ): void {
    throw new Error('Method not implemented.');
  }
  encodeMessage(arena: INativeMessageArena, messageIndex: number): Uint8Array {
    throw new Error('Method not implemented.');
  }
  encodeMessageAsync(
    arena: INativeMessageArena,
    messageIndex: number,
    callback: (data: Uint8Array | undefined, error: string | undefined) => void,
  ): void {
    throw new Error('Method not implemented.');
  }
  batchEncodeMessageAsync(
    arena: INativeMessageArena,
    messageIndexes: number[],
    callback: (data: Uint8Array[] | undefined, error: string | undefined) => void,
  ): void {
    throw new Error('Method not implemented.');
  }
  encodeMessageToJSON(arena: INativeMessageArena, messageIndex: number, options: number): string {
    throw new Error('Method not implemented.');
  }
  setMessageField(
    arena: INativeMessageArena,
    messageIndex: number,
    fieldIndex: number,
    value: NativeFieldValues,
  ): void {
    throw new Error('Method not implemented.');
  }
  getMessageFields(arena: INativeMessageArena, messageIndex: number): NativeFieldValues[] {
    throw new Error('Method not implemented.');
  }
  copyMessage(arena: INativeMessageArena, messageIndex: number, fromArena: INativeMessageArena): number {
    throw new Error('Method not implemented.');
  }
}
