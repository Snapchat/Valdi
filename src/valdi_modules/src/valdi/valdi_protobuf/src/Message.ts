import { ConsoleRepresentable } from 'valdi_core/src/ConsoleRepresentable';
import { INativeMessageIndex } from './ValdiProtobuf';
import { IArena, IMessage, IMessageConstructor, JSONPrintOptions } from './types';

function toPlainObjectImpl(value: any): any {
  if (value === undefined) {
    return undefined;
  }

  if (value.toPlainObject) {
    return value.toPlainObject();
  }

  if (Array.isArray(value)) {
    return value.map(v => toPlainObjectImpl(v));
  }

  if (value instanceof Map) {
    const map = new Map();
    value.forEach((mapValue, mapKey) => {
      map.set(mapKey, toPlainObjectImpl(mapValue));
    });
    return map;
  }

  return value;
}

/**
 * Prototype key under which generated message classes expose a function that fetches every
 * field of an instance in one native call.
 */
export const RESOLVE_ALL_FIELDS_KEY = '$resolveAllFields';

/**
 * Base class for all message types
 */
export class Message<MessageProps = {}> implements IMessage<MessageProps>, ConsoleRepresentable {
  /**
   * Creates a new Mesage instance using an existing native instance
   * inside the given arena.
   */
  constructor(readonly $arena: IArena, readonly $index: INativeMessageIndex) {}

  /**
   * Encode this message into buffer.
   */
  encode(): Uint8Array {
    return this.$arena.encodeMessage(this as IMessage<MessageProps>);
  }

  /**
   * Asynchronously encode this message into buffer.
   */
  encodeAsync(): Promise<Uint8Array> {
    return this.$arena.encodeMessageAsync(this as IMessage<MessageProps>);
  }

  /**
   * Creates a deep copy of this message into the given Arena.
   * If the arena is not provided, the copy will be hosted
   * into the same arena as this message.
   */
  clone<T extends IMessage<MessageProps>>(this: T, arena?: IArena): T {
    const targetArena = arena ?? this.$arena;
    return targetArena.copyMessage(this as IMessage<MessageProps>) as T;
  }

  /**
   * Eagerly recursively read the content of the message
   */
  toPlainObject(): MessageProps {
    (this as any)[RESOLVE_ALL_FIELDS_KEY]?.(this);
    const out: any = {};
    for (const field of (this.constructor as IMessageConstructor).fields) {
      const fieldName = field.name;
      const value = (this as any)[fieldName];

      if (value !== undefined) {
        out[fieldName] = toPlainObjectImpl(value);
      }
    }
    return out;
  }

  /**
   * Converts this message into its JSON equivalent.
   * Converting to JSON for anything other than debugging is discouraged, as such there is no exposed
   * function for converting JSON into message instances.
   * For more details on Protobuf JSON formatting:
   * https://protobuf.dev/programming-guides/proto3/#json
   */
  toDebugJSON(options?: JSONPrintOptions): string {
    return this.$arena.encodeMessageToJSON(this as IMessage<MessageProps>, options);
  }

  /**
   * Allow a full easy display in the console logs
   */
  toConsoleRepresentation() {
    return this.toPlainObject();
  }
}
