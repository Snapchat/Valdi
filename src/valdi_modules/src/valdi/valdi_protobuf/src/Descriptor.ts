import { StringMap } from 'coreutils/src/StringMap';
import { makeMessageFields } from './FieldFactory';
import { Message, RESOLVE_ALL_FIELDS_KEY } from './Message';
import { IField, INativeMessageFactory } from './ValdiProtobuf';
import { IArena, IDescriptor, IDescriptorPool, IMessage, IMessageConstructor } from './types';

export default class Descriptor implements IDescriptor {
  private messageConstructor: IMessageConstructor | undefined;

  private _fields: readonly IField[] | undefined;

  get fields(): readonly IField[] {
    let fields = this._fields;
    if (!fields) {
      fields = this.descriptorPool.getFieldsForDescriptorIndex(this.index);
      this._fields = fields;
    }
    return fields;
  }

  constructor(
    readonly namespace: string,
    readonly messageName: string,
    readonly index: number,
    private readonly descriptorPool: IDescriptorPool,
  ) {}

  setProperties(arena: IArena, message: IMessage, properties: StringMap<any>) {
    for (const field of this.fields) {
      const value = properties[field.name];
      if (value) {
        (message as any)[field.name] = value;
      }
    }
  }

  getConstructor(): IMessageConstructor {
    let messageConstructor = this.messageConstructor;
    if (!messageConstructor) {
      const index = this.index;
      const messageFactory: INativeMessageFactory = this.descriptorPool.nativeMessageFactory;
      const fields = this.fields;

      class NewMessage extends Message<any> {
        static descriptorIndex: number = index;
        static messageFactory: INativeMessageFactory = messageFactory;
        static fields = fields;
      }

      Object.defineProperty(NewMessage, 'name', { value: this.messageName });

      const messageFields = makeMessageFields(this.descriptorPool, fields);

      Object.defineProperty(NewMessage.prototype, RESOLVE_ALL_FIELDS_KEY, { value: messageFields.resolveAll });

      let fieldIndex = 0;
      for (const messageField of messageFields.fields) {
        const fieldName = fields[fieldIndex].name;
        try {
          Object.defineProperty(NewMessage.prototype, fieldName, {
            enumerable: true,
            get: messageField.get,
            set: messageField.set,
          });
        } catch (err: any) {
          throw Error(
            `Failed to set field at index ${fieldIndex} named '${fieldName}' on message ${this.messageName}: ${err}`,
          );
        }

        fieldIndex += 1;
      }

      messageConstructor = NewMessage;
      this.messageConstructor = messageConstructor;
    }

    return messageConstructor;
  }
}
