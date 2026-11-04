type OpaqueNativeObject<K> = {} & { __TYPE__: K };

export type INativeMessageFactory = {};
export type INativeMessageIndex = number;

export type INativeMessageArena = {};

export type NonRepeatedNativeFieldValues = number | boolean | Long | string | Uint8Array | INativeMessageIndex;
export type RepeatedNativeFieldValues = ReadonlyArray<NonRepeatedNativeFieldValues>;
export type NativeFieldValues = NonRepeatedNativeFieldValues | RepeatedNativeFieldValues;
export type NativeFieldValue<T extends NativeFieldValues = NativeFieldValues> = T extends RepeatedNativeFieldValues
  ? T
  : T extends INativeMessageIndex
  ? T | undefined
  : T;

export const enum FieldType {
  DOUBLE = 1,
  FLOAT = 2,
  INT64 = 3,
  UINT64 = 4,
  INT32 = 5,
  FIXED64 = 6,
  FIXED32 = 7,
  BOOL = 8,
  STRING = 9,
  GROUP = 10, // deprecated
  MESSAGE = 11,
  BYTES = 12,
  UINT32 = 13,
  ENUM = 14,
  SFIXED32 = 15,
  SFIXED64 = 16,
  SINT32 = 17,
  SINT64 = 18,
}

export const enum FieldModifier {
  NONE = 0,
  REPEATED = 1,
  MAP = 2,
}

export interface IField {
  name: string;
  index: number;
  type: FieldType;
  modifier: FieldModifier;
  otherDescriptorIndex?: number;
  oneOfFieldIndex?: number;
  enumValues?: number[];
}

export interface INativeDescriptor {
  fullName: string;
}

export type INativeNamespaceEntries = (string | number)[];

export interface ILoadMessageResult {
  factory: INativeMessageFactory;
  rootNamespaceEntries: INativeNamespaceEntries;
  descriptorNames: string[];
  namespaces?: any;
}

/**
 * Decoding mode handed to createArena: 0 lazy, 1 eager, 2 lets the runtime config
 * (VALDI_PROTOBUF_LAZY_DEFAULT) pick. 0 and 1 match the former boolean eagerDecoding parameter.
 */
export type NativeDecodingMode = 0 | 1 | 2;

export interface ValdiProtobufModule {
  createArena(decodingMode: NativeDecodingMode, ignoreAllFieldsDuringEncoding: boolean): INativeMessageArena;

  loadMessages(path: string): ILoadMessageResult;

  parseAndLoadMessages?(filename: string, protoFilesContent: string): ILoadMessageResult;

  getFieldsForMessageDescriptor(factory: INativeMessageFactory, messageDescriptorIndex: number): IField[];

  getNamespaceEntries(factory: INativeMessageFactory, namespaceId: number): INativeNamespaceEntries;

  createMessage(
    arena: INativeMessageArena,
    factory: INativeMessageFactory,
    messageDescriptorIndex: number,
  ): INativeMessageIndex;

  decodeMessage(
    arena: INativeMessageArena,
    factory: INativeMessageFactory,
    messageDescriptorIndex: number,
    data: Uint8Array,
  ): INativeMessageIndex;

  decodeMessageAsync(
    arena: INativeMessageArena,
    factory: INativeMessageFactory,
    messageDescriptorIndex: number,
    data: Uint8Array,
    callback: (messageIndex: INativeMessageIndex | undefined, error: string | undefined) => void,
  ): void;

  decodeMessageDebugJSONAsync(
    arena: INativeMessageArena,
    factory: INativeMessageFactory,
    messageDescriptorIndex: number,
    data: string,
    callback: (messageIndex: INativeMessageIndex | undefined, error: string | undefined) => void,
  ): void;

  encodeMessage(arena: INativeMessageArena, messageIndex: INativeMessageIndex): Uint8Array;

  encodeMessageAsync(
    arena: INativeMessageArena,
    messageIndex: INativeMessageIndex,
    callback: (data: Uint8Array | undefined, error: string | undefined) => void,
  ): void;

  batchEncodeMessageAsync(
    arena: INativeMessageArena,
    messageIndexes: INativeMessageIndex[],
    callback: (data: Uint8Array[] | undefined, error: string | undefined) => void,
  ): void;

  encodeMessageToJSON(arena: INativeMessageArena, messageIndex: INativeMessageIndex, options: number): string;

  setMessageField(
    arena: INativeMessageArena,
    messageIndex: INativeMessageIndex,
    fieldIndex: number,
    value: NativeFieldValues,
  ): void;

  getMessageFields(arena: INativeMessageArena, messageIndex: INativeMessageIndex): NativeFieldValues[];

  copyMessage(
    arena: INativeMessageArena,
    messageIndex: INativeMessageIndex,
    fromArena: INativeMessageArena,
  ): INativeMessageIndex;
}
