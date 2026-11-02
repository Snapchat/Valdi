import { lazyMap } from 'coreutils/src/ArrayUtils';
import { StringMap } from 'coreutils/src/StringMap';
import {
  FieldModifier,
  FieldType,
  IField,
  INativeMessageIndex,
  NativeFieldValues,
  NonRepeatedNativeFieldValues,
} from './ValdiProtobuf';
import {
  FieldValue,
  FieldValues,
  IArena,
  IDescriptorPool,
  IMessage,
  IMessageFactory,
  MapFieldMapKey,
  MapFieldMapValue,
  NonRepeatedFieldValues,
} from './types';
import { defaultValueForField } from './utils/misc';

// Use a value outside the bounds of a 32-bit integer to avoid
// any possible conflicts with enum values, since enum values can
// only be 32-bit integers.
const UNRECOGNIZED_ENUM_VALUE = 0xffffffffff;

type ValueConverter<T1, T2> = (arena: IArena, message: IMessage | undefined, value: T1) => T2;
type Preprocessor<T> = (arena: IArena, value: T) => T;
type Postprocessor<T> = (value: T) => T;

interface MapEntry {
  key: MapFieldMapKey;
  value: MapFieldMapValue;
}

interface ValueConverterPair<JSType extends FieldValues, NativeType extends NativeFieldValues> {
  readonly toNative: ValueConverter<JSType, NativeType>;
  readonly fromNative: ValueConverter<NativeType, JSType> | undefined;
  readonly preprocess: Preprocessor<JSType> | undefined;
  /**
   * Transforms only the in-memory JS value, used for ensuring the in-memory value for enums always
   * maps to an enum value when setting unkown values
   */
  readonly postprocess: Postprocessor<JSType> | undefined;
}

// Key where the array containing the cached JS fields will be set on the IMessage instance
const JS_FIELDS_KEY = Symbol('fields');

const NOOP: ValueConverterPair<any, any> = {
  toNative: (arena, message, value) => value,
  fromNative: (arena, message, value) => value,
  preprocess: undefined,
  postprocess: undefined,
};

function preprocessMessage(arena: IArena, messageFactory: IMessageFactory, message: IMessage): IMessage {
  const messageArena = message.$arena;

  if (messageArena !== arena) {
    if (messageArena === undefined) {
      const newMessage = arena.createMessage(messageFactory.getConstructor());
      messageFactory.setProperties(arena, newMessage, message as StringMap<any>);
      return newMessage;
    } else {
      // Message comes from a different arena, we need to copy it.
      return arena.copyMessage(message);
    }
  }

  return message;
}

function messageValueConverterPair(messageFactory: IMessageFactory): ValueConverterPair<IMessage, INativeMessageIndex> {
  return {
    toNative(arena, message, value) {
      return value.$index;
    },
    fromNative(arena, message, value) {
      return arena.getMessageInstance(messageFactory.getConstructor(), value);
    },
    preprocess(arena: IArena, value: IMessage) {
      return preprocessMessage(arena, messageFactory, value);
    },
    postprocess: undefined,
  };
}

function lazyEnumValueSet(field: IField): () => { [key: number]: boolean } {
  let values: { [key: number]: boolean } | undefined;
  return () => {
    if (!values) {
      values = {};
      for (const enumValue of field.enumValues ?? []) {
        values[enumValue] = true;
      }
    }
    return values;
  };
}

function enumValueConverterPair(field: IField): ValueConverterPair<number, number> {
  const enumValues = lazyEnumValueSet(field);
  return {
    toNative: NOOP.toNative,
    fromNative: (arena, message, value) => (enumValues()[value] ? value : UNRECOGNIZED_ENUM_VALUE),
    postprocess: value => (enumValues()[value] ? value : UNRECOGNIZED_ENUM_VALUE),
    preprocess: NOOP.preprocess,
  };
}

function preprocessRepeated<JSType extends NonRepeatedFieldValues>(
  arena: IArena,
  itemPreprocessor: Preprocessor<JSType>,
  items: JSType[],
): JSType[] {
  return lazyMap(items, item => itemPreprocessor(arena, item));
}

function repeatedValueConverterPair<
  JSType extends NonRepeatedFieldValues,
  NativeType extends NonRepeatedNativeFieldValues,
>(
  itemConverter: ValueConverterPair<JSType, NativeType> | undefined,
): ValueConverterPair<ReadonlyArray<JSType>, ReadonlyArray<NativeType>> | undefined {
  let preprocess: Preprocessor<ReadonlyArray<JSType>> | undefined;
  let postprocess: Postprocessor<ReadonlyArray<JSType>> | undefined;

  const itemPreprocessor = itemConverter?.preprocess;
  if (itemPreprocessor) {
    preprocess = (message, values) => preprocessRepeated(message, itemPreprocessor, values as JSType[]);
  }

  const itemPostProcessor = itemConverter?.postprocess;
  if (itemPostProcessor) {
    postprocess = values => values.map(itemPostProcessor);
  }

  if (itemConverter) {
    let fromNative: ValueConverter<ReadonlyArray<NativeType>, ReadonlyArray<JSType>> | undefined;
    const itemConverterFromNative = itemConverter.fromNative;

    if (itemConverterFromNative) {
      fromNative = (arena, message, values) => values.map(item => itemConverterFromNative(arena, message, item));
    }

    return {
      toNative(arena, message, values) {
        return values.map(item => itemConverter.toNative(arena, message, item));
      },

      fromNative,
      preprocess,
      postprocess,
    };
  } else {
    if (preprocess || postprocess) {
      return {
        toNative: NOOP.toNative,
        fromNative: undefined,
        preprocess,
        postprocess,
      };
    } else {
      return undefined;
    }
  }
}

function mapValueConverterPair<JSType extends NonRepeatedFieldValues, NativeType extends NonRepeatedNativeFieldValues>(
  itemConverter: ValueConverterPair<JSType, NativeType> | undefined,
): ValueConverterPair<ReadonlyMap<MapFieldMapKey, JSType>, ReadonlyArray<NativeType>> {
  const itemPreprocessor = itemConverter?.preprocess;
  if (!itemPreprocessor) {
    throw Error('Map should always have a preprocessor');
  }
  const itemFromNative = itemConverter.fromNative;
  if (!itemFromNative) {
    throw Error('Map should always have a fromNative');
  }

  const cacheKey = Symbol();

  const itemPostProcessor = itemConverter.postprocess;
  return {
    toNative(arena, message, value) {
      const previousEntries: MapEntry[] | undefined = message && ((message as any)[cacheKey] as MapEntry[]);
      if (previousEntries) {
        // Clear out our values, so we can make the messages orphan
        for (const entry of previousEntries) {
          entry.value = undefined!;
        }
      }

      let newEntries: MapEntry[] | undefined;
      const entries: NativeType[] = [];

      for (const [mapKey, mapValue] of Array.from(value)) {
        if (mapValue === undefined) {
          continue;
        }

        const convertedValue = itemPreprocessor(arena, (<MapEntry>{ key: mapKey, value: mapValue }) as any) as any;
        entries.push(itemConverter.toNative(arena, message, convertedValue));

        if (message) {
          const convertedMapValue = convertedValue.value;
          if (convertedMapValue.$arena) {
            // This is a Message, keep track of it so we can mark it orphan next time we set it
            if (!newEntries) {
              newEntries = [];
            }
            newEntries.push(convertedValue);
          }
        }
      }

      if (message && newEntries !== previousEntries) {
        (message as any)[cacheKey] = newEntries;
      }

      return entries;
    },

    fromNative(arena, message, values) {
      const out = new Map();
      if (!values) {
        return out;
      }

      let newEntries: MapEntry[] | undefined;

      for (const value of values) {
        if (value === undefined) {
          continue;
        }

        const item = itemFromNative(arena, message, value) as any as MapEntry;

        const mapKey = item.key;
        const mapValue = item.value;
        out.set(mapKey, mapValue);

        if ((mapValue as IMessage).$arena) {
          if (!newEntries) {
            newEntries = [];
          }
          newEntries.push(item);
        }
      }

      (message as any)[cacheKey] = newEntries;

      return out;
    },
    preprocess: undefined,
    postprocess: itemPostProcessor
      ? value =>
          new Map(
            Array.from(value.entries()).map(([key, value]) => {
              return [key, itemPostProcessor(value)];
            }),
          )
      : undefined,
  };
}

function valueConverterPairForField(
  field: IField,
  descriptorPool: IDescriptorPool,
): ValueConverterPair<any, any> | undefined {
  let itemConverter: ValueConverterPair<any, any> | undefined;

  if (field.type === FieldType.MESSAGE) {
    if (field.otherDescriptorIndex === undefined) {
      throw new Error('Message field must have an other descriptor index');
    }

    const messageFactory = descriptorPool.getDescriptorForIndex(field.otherDescriptorIndex);
    itemConverter = messageValueConverterPair(messageFactory);

    // If this message field is a map, check to see if its value type is an enum
    // If so, pass through the enum value converter's postprocess through so the
    // in-memory Map value can have its values updated to unknown on set if needed
    if (field.modifier === FieldModifier.MAP) {
      const valueField: IField = messageFactory.fields[1];
      if (valueField?.type === FieldType.ENUM) {
        const enumValueConverter = enumValueConverterPair(valueField);
        itemConverter = { ...itemConverter, postprocess: enumValueConverter.postprocess };
      }
    }
  } else if (field.type === FieldType.ENUM) {
    itemConverter = enumValueConverterPair(field);
  }

  switch (field.modifier) {
    case FieldModifier.REPEATED:
      return repeatedValueConverterPair(itemConverter);
    case FieldModifier.MAP:
      return mapValueConverterPair(itemConverter);
    case FieldModifier.NONE:
      return itemConverter;
  }
}

export interface IMessageField {
  get(this: IMessage): any;
  set(this: IMessage, value: any | undefined): void;
  toNativeDirect(arena: IArena, value: any): any;
}

function resolveFields(
  message: IMessage,
  converters: readonly (ValueConverterPair<FieldValues, NativeFieldValues> | undefined)[],
): FieldValue[] {
  let jsFields = (message as any)[JS_FIELDS_KEY] as any[];
  if (!jsFields) {
    const arena = message.$arena;
    jsFields = arena.getMessageFields(message);
    (message as any)[JS_FIELDS_KEY] = jsFields;

    let index = 0;
    for (const converter of converters) {
      const fromNative = converter?.fromNative;
      if (fromNative) {
        const unconvertedValue = jsFields[index];
        if (unconvertedValue !== undefined) {
          jsFields[index] = fromNative(arena, message, unconvertedValue);
        }
      }

      index++;
    }
  }

  return jsFields;
}

function clearOneOfFields(fields: readonly IField[], jsFields: FieldValue[], currentField: IField) {
  for (let i = 0; i < fields.length; i++) {
    const field = fields[i];
    if (field === currentField || field.oneOfFieldIndex !== currentField.oneOfFieldIndex) {
      continue;
    }

    jsFields[i] = undefined;
  }
}

function makeMessageField(
  fields: readonly IField[],
  converters: readonly (ValueConverterPair<FieldValues, NativeFieldValues> | undefined)[],
  index: number,
): IMessageField {
  const field = fields[index];
  const converter = converters[index];
  const defaultValue = defaultValueForField(fields[index]);

  function get(this: IMessage): any {
    const value = resolveFields(this, converters)[index];
    if (value !== undefined) {
      return value;
    }
    return defaultValue;
  }

  if (converter) {
    // Field using a converter
    return {
      get,
      set(this: IMessage, value: any) {
        if (converter.preprocess && value !== undefined) {
          value = converter.preprocess(this.$arena, value);
        }

        const jsFields = resolveFields(this, converters);

        const currentValue = jsFields[index];

        if (currentValue !== value) {
          jsFields[index] = converter.postprocess ? converter.postprocess(value) : value;

          let nativeValue: NativeFieldValues | undefined;
          if (value !== undefined) {
            nativeValue = converter.toNative(this.$arena, this, value);
          }

          this.$arena.setMessageField(this, field.index, converters.length, nativeValue);

          if (field.oneOfFieldIndex !== undefined) {
            clearOneOfFields(fields, jsFields, field);
          }
        }
      },
      toNativeDirect(arena: IArena, value: any) {
        if (converter.preprocess) {
          value = converter.preprocess(arena, value);
        }
        return converter.toNative(arena, undefined, value);
      },
    };
  } else {
    // Field without a converter, can use faster branch
    return {
      get,
      set(this: IMessage, value: any) {
        const jsFields = resolveFields(this, converters);

        const currentValue = jsFields[index];

        if (currentValue !== value) {
          jsFields[index] = value;

          this.$arena.setMessageField(this, field.index, converters.length, value);

          if (field.oneOfFieldIndex !== undefined) {
            clearOneOfFields(fields, jsFields, field);
          }
        }
      },
      toNativeDirect(arena: IArena, value: any) {
        return value;
      },
    };
  }
}

export function makeMessageFields(descriptorPool: IDescriptorPool, fields: readonly IField[]): IMessageField[] {
  const converters = fields.map(field => valueConverterPairForField(field, descriptorPool));

  const messageFields: IMessageField[] = [];
  for (let i = 0; i < fields.length; i++) {
    messageFields.push(makeMessageField(fields, converters, i));
  }

  return messageFields;
}
