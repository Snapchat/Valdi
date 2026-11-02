import { makeSingleCallInterruptibleCallback } from 'valdi_core/src/utils/FunctionUtils';
import { ValdiProtobufModule, INativeMessageArena, INativeMessageIndex } from './ValdiProtobuf';
import { getProtobufModule } from './ValdiProtobufModule';
import { IArena, IMessage, IMessageConstructor, JSONPrintOptions } from './types';

/**
 * Specifies how the Arena should decode messages.
 */
export const enum DecodingMode {
  /**
   * Parsing will be eager, meaning that nested messages will be parsed immediately
   * when the decode() call is made. This will provide worse performance than lazy,
   * but will validate that the message could be decoded immediately.
   * Use this option if you are decoding a message from an untrusted source, like
   * a local disk cache. The arena will use the EAGER decoding mode when decoding
   * a message asynchronously, even if LAZY is set.
   *
   * This is the default option.
   */
  EAGER = 0,

  /**
   * Parsing will be lazy, meaning that nested messages
   * will be parsed when they are first accessed. This will provide
   * the best performance if the TypeScript code does not visit
   * every single sub messages that the decoded message contains.
   * If one of the nested messages is malformated, parse errors will
   * occur only when the nested message is accessed, as opposed to when
   * the decode() call is made. Use this option if you are decoding a message
   * from a trusted source, like a remote RPC call from a known endpoint.
   */
  LAZY = 1,
}

/**
 * Specifies how the Arena should encode messages.
 */
export const enum EncodingMode {
  /**
   * Ignore fields that have a zero value which will reduce
   * the encoded message size.
   *
   * This is the default option.
   */
  IGNORE_ZERO_FIELDS = 0,

  /**
   * Include all fields in the payload output, regardless if they
   * have a zero size.
   */
  INCLUDE_ALL_FIELDS = 1,
}

export class Arena implements IArena {
  private protobuf: ValdiProtobufModule;
  private $native: INativeMessageArena;

  constructor(decodingMode?: DecodingMode, encodingMode?: EncodingMode) {
    this.protobuf = getProtobufModule();
    const eagerDecoding = decodingMode !== DecodingMode.LAZY;
    const includeAllFields = encodingMode === EncodingMode.INCLUDE_ALL_FIELDS;
    this.$native = this.protobuf.createArena(eagerDecoding, includeAllFields);
  }

  createMessage(constructor: IMessageConstructor): IMessage {
    const messageIndex = this.protobuf.createMessage(
      this.$native,
      constructor.messageFactory,
      constructor.descriptorIndex,
    );
    return this.getMessageInstance(constructor, messageIndex);
  }

  decodeMessage(constructor: IMessageConstructor, data: Uint8Array): IMessage {
    const messageIndex = this.protobuf.decodeMessage(
      this.$native,
      constructor.messageFactory,
      constructor.descriptorIndex,
      data,
    );
    return this.getMessageInstance(constructor, messageIndex);
  }

  decodeMessageAsync(constructor: IMessageConstructor, data: Uint8Array): Promise<IMessage<any>> {
    return new Promise((resolve, reject) => {
      this.protobuf.decodeMessageAsync(
        this.$native,
        constructor.messageFactory,
        constructor.descriptorIndex,
        data,
        makeSingleCallInterruptibleCallback((data, error) => {
          if (data !== undefined) {
            resolve(this.getMessageInstance(constructor, data));
          } else {
            reject(new Error(error));
          }
        }),
      );
    });
  }

  decodeMessageDebugJSONAsync(constructor: IMessageConstructor, data: string): Promise<IMessage<any>> {
    return new Promise((resolve, reject) => {
      this.protobuf.decodeMessageDebugJSONAsync(
        this.$native,
        constructor.messageFactory,
        constructor.descriptorIndex,
        data,
        makeSingleCallInterruptibleCallback((data, error) => {
          if (data !== undefined) {
            resolve(this.getMessageInstance(constructor, data));
          } else {
            reject(new Error(error));
          }
        }),
      );
    });
  }

  encodeMessage(message: IMessage): Uint8Array {
    return this.protobuf.encodeMessage(this.$native, message.$index);
  }

  encodeMessageAsync(message: IMessage<any>): Promise<Uint8Array> {
    return new Promise((resolve, reject) => {
      this.protobuf.encodeMessageAsync(
        this.$native,
        message.$index,
        makeSingleCallInterruptibleCallback((data, error) => {
          if (data !== undefined) {
            resolve(data);
          } else {
            reject(new Error(error));
          }
        }),
      );
    });
  }

  batchEncodeMessageAsync(messages: readonly IMessage<any>[]): Promise<Uint8Array[]> {
    const messageIndexes = messages.map(fn => fn.$index);
    return new Promise((resolve, reject) => {
      this.protobuf.batchEncodeMessageAsync(
        this.$native,
        messageIndexes,
        makeSingleCallInterruptibleCallback((data, error) => {
          if (data !== undefined) {
            resolve(data);
          } else {
            reject(new Error(error));
          }
        }),
      );
    });
  }

  encodeMessageToJSON(message: IMessage<any>, printOptions: JSONPrintOptions | undefined): string {
    return this.protobuf.encodeMessageToJSON(this.$native, message.$index, printOptions ?? 0);
  }

  getMessageFields(message: IMessage): any[] {
    return this.protobuf.getMessageFields(this.$native, message.$index);
  }

  setMessageField(message: IMessage, fieldIndex: number, totalFieldsLength: number, fieldValue: any): void {
    this.protobuf.setMessageField(this.$native, message.$index, fieldIndex, fieldValue);
  }

  getMessageInstance(constructor: IMessageConstructor, messageIndex: INativeMessageIndex): IMessage {
    return new constructor(this, messageIndex);
  }

  copyMessage(message: IMessage): IMessage {
    const otherArena = message.$arena;
    if (!(otherArena instanceof Arena)) {
      throw Error('Can only copy messages from an Arena instance');
    }

    const messageIndex = this.protobuf.copyMessage(this.$native, message.$index, otherArena.$native);
    return this.getMessageInstance(message.constructor as IMessageConstructor, messageIndex);
  }
}
