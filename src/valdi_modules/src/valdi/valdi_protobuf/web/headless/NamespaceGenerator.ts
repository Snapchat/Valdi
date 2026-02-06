import { Message as ValdiMessage } from '../Message';
import { IArena, IMessage, IMessageNamespace, JSONPrintOptions } from '../src_symlink/types';
import { FullyQualifiedName } from './FullyQualifiedName';
import { IMessageType, PbLong, PbULong } from '@protobuf-ts/runtime';
import { DescriptorDatabase } from './DescriptorDatabase';
import { INativeMessageIndex } from '../src_symlink/ValdiProtobuf';

// Monkey-patch PbLong to accept Long objects from long.js
// @protobuf-ts uses PbLong.from() to convert values to their internal long representation
// We extend it to handle Long objects by converting them to string/number first
const originalPbLongFrom = PbLong.from;
const originalPbULongFrom = PbULong.from;

PbLong.from = function(value: any): any {
  // If it's a Long object, convert it to a primitive
  if (value && typeof value === 'object' && 'low' in value && 'high' in value && 'unsigned' in value) {
    const num = value.toNumber?.();
    if (num !== undefined && Number.isSafeInteger(num)) {
      return originalPbLongFrom.call(this, num);
    }
    return originalPbLongFrom.call(this, value.toString?.() ?? value);
  }
  return originalPbLongFrom.call(this, value);
};

PbULong.from = function(value: any): any {
  // If it's a Long object, convert it to a primitive
  if (value && typeof value === 'object' && 'low' in value && 'high' in value && 'unsigned' in value) {
    const num = value.toNumber?.();
    if (num !== undefined && Number.isSafeInteger(num) && num >= 0) {
      return originalPbULongFrom.call(this, num);
    }
    return originalPbULongFrom.call(this, value.toString?.() ?? value);
  }
  return originalPbULongFrom.call(this, value);
};

export interface IMonkeyPatchedProtobufJsNamespace {
  [key: string]: IMessageNamespace | IMonkeyPatchedProtobufJsNamespace;
}

// Dummy arena/index for web messages that don't use native arena
const DUMMY_ARENA = {} as unknown as IArena;
const DUMMY_INDEX: INativeMessageIndex = -1 as INativeMessageIndex;

/** Extract underlying data from a value (handles both wrapper and plain objects) */
function unwrapData(value: any): any {
  return value instanceof ProtobufTsMessageWrapper ? value._data : value;
}

/**
 * Wrapper class that makes @protobuf-ts messages compatible with Valdi's message interface.
 */
export class ProtobufTsMessageWrapper<MessageProps = {}> extends ValdiMessage<MessageProps> {
  readonly _messageType: IMessageType<any>;
  readonly _data: any;

  constructor(messageType: IMessageType<any>, data: any) {
    super(DUMMY_ARENA, DUMMY_INDEX);
    this._messageType = messageType;
    this._data = data;
    Object.assign(this, data);
  }

  override encode(): Uint8Array {
    return this._messageType.toBinary(this._data);
  }

  override encodeAsync(): Promise<Uint8Array> {
    return Promise.resolve(this.encode());
  }

  override clone<T extends IMessage<MessageProps>>(this: T, arena?: IArena): T {
    const wrapper = this as unknown as ProtobufTsMessageWrapper<MessageProps>;
    const clonedData = wrapper._messageType.clone(wrapper._data);
    return new ProtobufTsMessageWrapper(wrapper._messageType, clonedData) as unknown as T;
  }

  override toPlainObject(): MessageProps {
    return this._data as MessageProps;
  }

  override toDebugJSON(options?: JSONPrintOptions): string {
    const obj = this.toPlainObject();
    return options && (options & JSONPrintOptions.PRETTY) !== 0
      ? JSON.stringify(obj, null, 2)
      : JSON.stringify(obj);
  }
}

function resolveTargetNamespace(
  current: IMonkeyPatchedProtobufJsNamespace,
  fqn: FullyQualifiedName
): IMonkeyPatchedProtobufJsNamespace {
  if (fqn.parent) {
    const parent = resolveTargetNamespace(current, fqn.parent);
    if (!parent[fqn.symbolName]) {
      parent[fqn.symbolName] = {};
    }
    return parent[fqn.symbolName] as IMonkeyPatchedProtobufJsNamespace;
  }
  if (!current[fqn.symbolName]) {
    current[fqn.symbolName] = {};
  }
  return current[fqn.symbolName] as IMonkeyPatchedProtobufJsNamespace;
}

function populateNamespaceFromMessageType(
  messageType: IMessageType<any>,
  namespace: IMessageNamespace
): void {
  namespace.create = (_arena: IArena | undefined, value?: any): IMessage => {
    const data = messageType.create(value);
    return new ProtobufTsMessageWrapper(messageType, data) as unknown as IMessage;
  };

  namespace.decode = (_arena: IArena | undefined, data: Uint8Array): IMessage => {
    const decoded = messageType.fromBinary(data);
    return new ProtobufTsMessageWrapper(messageType, decoded) as unknown as IMessage;
  };

  namespace.decodeAsync = (_arena: IArena | undefined, data: Uint8Array): Promise<IMessage> => {
    const decoded = messageType.fromBinary(data);
    return Promise.resolve(new ProtobufTsMessageWrapper(messageType, decoded) as unknown as IMessage);
  };

  namespace.decodeDebugJSONAsync = (_arena: IArena | undefined, json: string): Promise<IMessage> => {
    const parsed = JSON.parse(json);
    const decoded = messageType.fromJson(parsed);
    return Promise.resolve(new ProtobufTsMessageWrapper(messageType, decoded) as unknown as IMessage);
  };

  namespace.encode = (value: IMessage | any): Uint8Array => {
    const unwrapped = unwrapData(value);
    const normalized = messageType.create(unwrapped);
    return messageType.toBinary(normalized);
  };

  namespace.encodeAsync = (value: IMessage | any): Promise<Uint8Array> => {
    return Promise.resolve(namespace.encode(value));
  };
}

export function generateMonkeyPatchedNamespace(
  db: DescriptorDatabase,
  types: readonly FullyQualifiedName[],
): IMonkeyPatchedProtobufJsNamespace {
  const out: IMonkeyPatchedProtobufJsNamespace = {};

  for (const type of types) {
    const messageType = db.getMessageType(type.fullName);
    const namespace = resolveTargetNamespace(out, type);
    populateNamespaceFromMessageType(messageType, namespace as unknown as IMessageNamespace);
  }

  return out;
}
