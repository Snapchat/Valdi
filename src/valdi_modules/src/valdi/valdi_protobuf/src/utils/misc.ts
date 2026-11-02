import { FieldModifier, FieldType, IField } from '../ValdiProtobuf';
import { FieldValue } from '../types';

export type DefaultValueFactory = () => FieldValue;

const defaultMap = new Map();
const defaultBytes = new Uint8Array([]);
const defaultArray: any[] = [];
const defaultString = '';
const defaultUnsigned64 = Long.fromNumber(0, true);
const defaultSigned64 = Long.fromNumber(0, false);

Object.freeze(defaultMap);
Object.freeze(defaultBytes);
Object.freeze(defaultArray);
Object.freeze(defaultUnsigned64);
Object.freeze(defaultSigned64);

export function defaultValueForField(field: IField): FieldValue {
  switch (field.modifier) {
    case FieldModifier.REPEATED:
      return defaultArray;
    case FieldModifier.MAP:
      return defaultMap;
  }
  switch (field.type) {
    case FieldType.BOOL:
      return false;
    case FieldType.STRING:
      return defaultString;
    case FieldType.GROUP:
    case FieldType.MESSAGE:
      return undefined;
    case FieldType.BYTES:
      return defaultBytes;
    case FieldType.UINT64:
    case FieldType.FIXED64:
      return defaultUnsigned64;
    case FieldType.INT64:
    case FieldType.SFIXED64:
    case FieldType.SINT64:
      return defaultSigned64;
    case FieldType.DOUBLE:
    case FieldType.FLOAT:
    case FieldType.INT32:
    case FieldType.FIXED32:
    case FieldType.UINT32:
    case FieldType.ENUM:
    case FieldType.SFIXED32:
    case FieldType.SINT32:
      return 0;
  }
}
