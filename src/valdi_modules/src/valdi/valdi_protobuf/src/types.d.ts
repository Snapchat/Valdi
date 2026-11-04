import { StringMap } from 'coreutils/src/StringMap';
import { IField, INativeMessageFactory, INativeMessageIndex } from './ValdiProtobuf';

export type MapFieldMapKey = string | number;
export type MapFieldMapValue = NonRepeatedFieldValues;
export type MapFieldValue = ReadonlyMap<MapFieldMapKey, MapFieldMapValue>;
export type NonRepeatedFieldValues = boolean | string | Uint8Array | number | Long | IMessage;
export type RepeatedFieldValues = ReadonlyArray<NonRepeatedFieldValues>;
export type FieldValues = NonRepeatedFieldValues | RepeatedFieldValues | MapFieldValue;
export type FieldValue<T extends FieldValues = FieldValues> = T extends RepeatedFieldValues
  ? T
  : T extends IMessage
  ? T | undefined
  : T;

export const enum JSONPrintOptions {
  /**
  Whether to add spaces, line breaks and indentation to make the JSON output
  easy to read.
  */
  PRETTY = 1 << 0,
  /**
   * Whether to always print primitive fields. By default proto3 primitive
   * fields with default values will be omitted in JSON output. For example, an
   * int32 field set to 0 will be omitted. Set this flag to true will override
   * the default behavior and print primitive fields regardless of their values.
   */
  ALWAYS_PRINT_PRIMITIVE_FIELDS = 1 << 1,

  /**
   * Whether to always print enums as ints. By default they are rendered as
   * strings.
   */
  ALWAYS_PRINT_ENUMS_AS_INTS = 1 << 2,
}

export interface IMessage<MessageProps = any> {
  readonly $arena: IArena;
  readonly $index: INativeMessageIndex;

  /**
   * Encode the message into a buffer
   */
  encode(): Uint8Array;

  /**
   * Asynchronously encode the message into a buffer
   */
  encodeAsync(): Promise<Uint8Array>;

  /**
   * Creates a deep copy of this message into the given Arena.
   * If the arena is not provided, the copy will be hosted
   * into the same arena as this message.
   */
  clone<T extends IMessage>(this: T, arena?: IArena): T;

  /**
   * Converts this Protobuf Message into a plain JS object
   * containing all properties from this message.
   * This is a recursive call and can be expensive, use
   * only if you truly need it.
   */
  toPlainObject(): MessageProps;

  /**
   * Converts this message into its JSON equivalent.
   * Converting to JSON for anything other than debugging is discouraged, as such there is no exposed
   * function for converting JSON into message instances.
   * For more details on Protobuf JSON formatting:
   * https://protobuf.dev/programming-guides/proto3/#json
   */
  toDebugJSON(options?: JSONPrintOptions): string;
}

export interface IArena {
  createMessage(constructor: IMessageConstructor): IMessage;
  decodeMessage(constructor: IMessageConstructor, data: Uint8Array): IMessage;
  decodeMessageAsync(constructor: IMessageConstructor, data: Uint8Array): Promise<IMessage>;
  encodeMessage(message: IMessage): Uint8Array;
  encodeMessageAsync(message: IMessage): Promise<Uint8Array>;
  decodeMessageDebugJSONAsync(constructor: IMessageConstructor, data: string): Promise<IMessage>;
  /**
   * Asynchronously encode a list of messages into a single batch.
   * This can be more efficient than asynchronously encoding each message
   * individually, especially if each message is fairly small.
   */
  batchEncodeMessageAsync(messages: readonly IMessage[]): Promise<Uint8Array[]>;
  encodeMessageToJSON(message: IMessage, printOptions: JSONPrintOptions | undefined): string;
  getMessageFields(message: IMessage): any[];
  /**
   * When true, message getters fetch one field at a time through getMessageField instead of
   * materializing every field through getMessageFields on first access.
   */
  readonly fieldReadsEnabled?: boolean;
  getMessageField?(message: IMessage, fieldIndex: number): any;
  setMessageField(message: IMessage, fieldIndex: number, totalFieldsLength: number, fieldValue: any): void;
  getMessageInstance(constructor: IMessageConstructor, messageIndex: INativeMessageIndex): IMessage;
  copyMessage(message: IMessage): IMessage;
}

export type IMessageConstructor = {
  new (arena: IArena, messageIndex: INativeMessageIndex): IMessage;

  descriptorIndex: number;
  messageFactory: INativeMessageFactory;
  fields: readonly IField[];
};

export interface IMessageFactory {
  getConstructor(): IMessageConstructor;
  setProperties(arena: IArena, message: IMessage, properties: StringMap<FieldValues>): void;
}

export interface IDescriptor extends IMessageFactory {
  readonly namespace: string;
  readonly messageName: string;
  readonly index: number;
  readonly fields: readonly IField[];
}

export interface IDescriptorPool {
  /**
   * All the message constructors keyed by the namespace.
   * If a message is on "com.snapchat.MyMessage", the "MyMessage"
   * class will be nested on the namespace object for key "com",
   * then another namespace object for the key "snapchat", and then
   * at key "MyMessage".
   */
  readonly namespaces: Record<string, any>;
  readonly nativeMessageFactory: INativeMessageFactory;
  readonly descriptorsLength: number;

  getFieldsForDescriptorIndex(descriptorIndex: number): readonly IField[];

  getDescriptorForIndex(index: number): IDescriptor;

  /**
   * Retrieve the message namespace instance from its message path.
   * If a message is called MyMessage in a package my_package, it
   * can be retrieved by passing my_package.MyMessage
   */
  getMessageNamespace(messagePath: string): IMessageNamespace | undefined;
}

/**
 * Represents the namespace exposed for each generated Protobuf Message type.
 * Exposes methods allowing to interact with the message.
 */
export interface IMessageNamespace {
  create: (arena: IArena, properties?: StringMap<FieldValues>) => IMessage;
  decode: (arena: IArena, buffer: Uint8Array) => IMessage;
  decodeAsync: (arena: IArena, buffer: Uint8Array) => Promise<IMessage>;
  encode: (value: IMessage | StringMap<FieldValues>) => Uint8Array;
  encodeAsync: (value: StringMap<FieldValues>) => Promise<Uint8Array>;
  decodeDebugJSONAsync(arena: IArena, json: string): Promise<IMessage>;
}
