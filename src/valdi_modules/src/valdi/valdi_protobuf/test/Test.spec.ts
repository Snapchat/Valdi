import { Arena, DecodingMode } from 'valdi_protobuf/src/Arena';
import { loadDescriptorPoolFromProtoFiles } from 'valdi_protobuf/src/ProtobufBuilder';
import { IArena, IMessage } from 'valdi_protobuf/src/types';
import 'jasmine/src/jasmine';
import { Base64 } from 'coreutils/src/Base64';
import { package_with_underscores, test } from './proto';

interface MessageFactory<InterfaceType, MessageType> {
  create(arena: IArena, properties: InterfaceType): MessageType;
  decode(arena: IArena, buffer: Uint8Array): MessageType;
}

function testMessage<InterfaceType, MessageType extends IMessage & InterfaceType>(
  factory: MessageFactory<InterfaceType, MessageType>,
  message: InterfaceType,
  validator: (message: MessageType) => void,
  updater?: (message: MessageType, arena: IArena) => void,
): void {
  try {
    const updateAndValidate = (arena: Arena, instance: MessageType) => {
      updater?.(instance, arena);
      validator(instance);
      // Test after a encode/encode
      validator(factory.decode(arena, instance.encode()));
      // Test after a clone
      validator(instance.clone());
    };

    // Encode first to capture the original state
    const arena = new Arena();
    const encoded = factory.create(arena, message).encode();

    // Test using a message that was created through "create"
    updateAndValidate(arena, factory.create(arena, message));
    // // Test using a message that was created through "decode"
    updateAndValidate(arena, factory.decode(arena, encoded));
    // Test using an arena with lazy decode mode
    const eagerArena = new Arena(DecodingMode.LAZY);
    updateAndValidate(eagerArena, factory.decode(eagerArena, encoded));
  } catch (err: any) {
    console.error('Got error:', err);
    throw err;
  }
}

const INT32_MIN = -2147483648;
const INT32_MAX = 0x7fffffff;
const UINT32_MIN = 0;
const UINT32_MAX = 0xffffffff;
const INT64_MIN = Long.MIN_VALUE;
const INT64_MAX = Long.MAX_VALUE;
const UINT64_MAX = Long.MAX_UNSIGNED_VALUE;
const UINT64_MIN = Long.fromNumber(0, true);

describe('Valdi Proto Runtime', () => {
  describe('signed 32-bit fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.int32).toEqual(0);
        expect(x.sint32).toEqual(0);
        expect(x.sfixed32).toEqual(0);
      });
    });
    it('positive values', () => {
      testMessage(
        test.Message,
        {
          int32: 1,
          sint32: 2,
          sfixed32: 3,
        },
        x => {
          expect(x.int32).toEqual(1);
          expect(x.sint32).toEqual(2);
          expect(x.sfixed32).toEqual(3);
        },
      );
    });
    it('negative values', () => {
      testMessage(
        test.Message,
        {
          int32: -1,
          sint32: -2,
          sfixed32: -3,
        },
        x => {
          expect(x.int32).toEqual(-1);
          expect(x.sint32).toEqual(-2);
          expect(x.sfixed32).toEqual(-3);
        },
      );
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.int32).toEqual([]);
        expect(x.sint32).toEqual([]);
        expect(x.sfixed32).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          int32: [1, -3],
          sint32: [3, -4],
          sfixed32: [INT32_MAX, INT32_MIN],
        },
        x => {
          expect(x.int32).toEqual([1, -3]);
          expect(x.sint32).toEqual([3, -4]);
          expect(x.sfixed32).toEqual([INT32_MAX, INT32_MIN]);
        },
      );
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        {
          int32: 1,
          sint32: 2,
          sfixed32: 3,
        },
        x => {
          expect(x.int32).toEqual(2);
          expect(x.sint32).toEqual(3);
          expect(x.sfixed32).toEqual(4);
        },
        x => {
          x.int32 += 1;
          x.sint32 += 1;
          x.sfixed32 += 1;
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        {
          int32: 1,
          sint32: 2,
          sfixed32: 3,
        },
        x => {
          expect(x.int32).toEqual(0);
          expect(x.sint32).toEqual(0);
          expect(x.sfixed32).toEqual(0);
        },
        x => {
          x.int32 = undefined as any;
          x.sint32 = undefined as any;
          x.sfixed32 = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          int32: [1],
          sint32: [2],
          sfixed32: [3],
        },
        x => {
          expect(x.int32).toEqual([1, 101]);
          expect(x.sint32).toEqual([2, 102]);
          expect(x.sfixed32).toEqual([3, INT32_MAX, INT32_MIN]);
        },
        x => {
          x.int32 = [...x.int32, 101];
          x.sint32 = [...x.sint32, 102];
          x.sfixed32 = [...x.sfixed32, INT32_MAX, INT32_MIN];
        },
      );
    });
  });

  describe('unsigned 32-bit fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.uint32).toEqual(0);
        expect(x.fixed32).toEqual(0);
      });
    });
    it('normal values', () => {
      testMessage(
        test.Message,
        {
          uint32: 1,
          fixed32: 2,
        },
        x => {
          expect(x.uint32).toEqual(1);
          expect(x.fixed32).toEqual(2);
        },
      );
    });
    it('large values', () => {
      testMessage(
        test.Message,
        {
          uint32: UINT32_MAX,
          fixed32: UINT32_MAX,
        },
        x => {
          expect(x.uint32).toEqual(UINT32_MAX);
          expect(x.fixed32).toEqual(UINT32_MAX);
        },
      );
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.uint32).toEqual([]);
        expect(x.fixed32).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          uint32: [1, 2],
          fixed32: [UINT32_MAX, UINT32_MIN],
        },
        x => {
          expect(x.uint32).toEqual([1, 2]);
          expect(x.fixed32).toEqual([UINT32_MAX, UINT32_MIN]);
        },
      );
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        {
          uint32: 1,
          fixed32: 2,
        },
        x => {
          expect(x.uint32).toEqual(2);
          expect(x.fixed32).toEqual(3);
        },
        x => {
          x.uint32 += 1;
          x.fixed32 += 1;
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        {
          uint32: 1,
          fixed32: 2,
        },
        x => {
          expect(x.uint32).toEqual(0);
          expect(x.fixed32).toEqual(0);
        },
        x => {
          x.uint32 = undefined as any;
          x.fixed32 = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          uint32: [1],
          fixed32: [2],
        },
        x => {
          expect(x.uint32).toEqual([1, 101]);
          expect(x.fixed32).toEqual([2, UINT32_MAX]);
        },
        x => {
          x.uint32 = [...x.uint32, 101];
          x.fixed32 = [...x.fixed32, UINT32_MAX];
        },
      );
    });
  });

  describe('long limits', () => {
    it('supports min unsigned 64bits', () => {
      testMessage(
        test.Message,
        { uint64: Long.fromNumber(1) },
        x => {
          expect(x.uint64).toEqual(UINT64_MIN);
        },
        x => {
          x.uint64 = UINT64_MIN;
        },
      );
    });
    it('supports max unsigned 64bits', () => {
      testMessage(
        test.Message,
        { uint64: Long.fromNumber(1) },
        x => {
          expect(x.uint64).toEqual(UINT64_MAX);
        },
        x => {
          x.uint64 = UINT64_MAX;
        },
      );
    });
    it('supports min signed 64 bits', () => {
      testMessage(
        test.Message,
        { int64: Long.fromNumber(1), sint64: Long.fromNumber(1) },
        x => {
          expect(x.int64).toEqual(INT64_MIN);
          expect(x.sint64).toEqual(INT64_MIN);
        },
        x => {
          x.int64 = INT64_MIN;
          x.sint64 = INT64_MIN;
        },
      );
    });
    it('supports max signed 64 bits', () => {
      testMessage(
        test.Message,
        { int64: Long.fromNumber(1), sint64: Long.fromNumber(1) },
        x => {
          expect(x.int64).toEqual(INT64_MAX);
          expect(x.sint64).toEqual(INT64_MAX);
        },
        x => {
          x.int64 = INT64_MAX;
          x.sint64 = INT64_MAX;
        },
      );
    });
  });

  describe('signed 64-bit fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.int64).toEqual(Long.fromNumber(0, false));
        expect(x.sint64).toEqual(Long.fromNumber(0, false));
        expect(x.sfixed64).toEqual(Long.fromNumber(0, false));
      });
    });
    it('positive values', () => {
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(1, false),
          sint64: Long.fromNumber(2, false),
          sfixed64: Long.fromNumber(3, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(1, false));
          expect(x.sint64).toEqual(Long.fromNumber(2, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(3, false));
        },
      );
    });
    it('negative values', () => {
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(-1, false),
          sint64: Long.fromNumber(-2, false),
          sfixed64: Long.fromNumber(-3, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(-1, false));
          expect(x.sint64).toEqual(Long.fromNumber(-2, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(-3, false));
        },
      );
    });
    it('large values', () => {
      testMessage(
        test.Message,
        {
          int64: INT64_MAX,
          sfixed64: INT64_MIN,
        },
        x => {
          expect(x.int64).toEqual(INT64_MAX);
          expect(x.sfixed64.toString()).toEqual(INT64_MIN.toString());
        },
      );
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(UINT32_MAX, false),
          sfixed64: Long.fromNumber(UINT32_MIN, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(UINT32_MAX, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(UINT32_MIN, false));
        },
      );
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(INT32_MAX, false),
          sfixed64: Long.fromNumber(INT32_MIN, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(INT32_MAX, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(INT32_MIN, false));
        },
      );
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(UINT32_MAX + 1, false),
          sfixed64: Long.fromNumber(UINT32_MIN - 1, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(UINT32_MAX + 1, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(UINT32_MIN - 1, false));
        },
      );
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(INT32_MAX + 1, false),
          sfixed64: Long.fromNumber(INT32_MIN - 1, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(INT32_MAX + 1, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(INT32_MIN - 1, false));
        },
      );
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.int64).toEqual([]);
        expect(x.sint64).toEqual([]);
        expect(x.sfixed64).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          int64: [Long.fromNumber(1, false), Long.fromNumber(2, false)],
          sint64: [Long.fromNumber(3, false), Long.fromNumber(4, false)],
          sfixed64: [Long.fromNumber(5, false), Long.fromNumber(6, false)],
        },
        x => {
          expect(x.int64).toEqual([Long.fromNumber(1, false), Long.fromNumber(2, false)]);
          expect(x.sint64).toEqual([Long.fromNumber(3, false), Long.fromNumber(4, false)]);
          expect(x.sfixed64).toEqual([Long.fromNumber(5, false), Long.fromNumber(6, false)]);
        },
      );
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(1, false),
          sint64: Long.fromNumber(2, false),
          sfixed64: Long.fromNumber(3, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(2, false));
          expect(x.sint64).toEqual(Long.fromNumber(3, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(4, false));
        },
        x => {
          x.int64 = Long.fromNumber(2, false);
          x.sint64 = Long.fromNumber(3, false);
          x.sfixed64 = Long.fromNumber(4, false);
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        {
          int64: Long.fromNumber(1, false),
          sint64: Long.fromNumber(2, false),
          sfixed64: Long.fromNumber(3, false),
        },
        x => {
          expect(x.int64).toEqual(Long.fromNumber(0, false));
          expect(x.sint64).toEqual(Long.fromNumber(0, false));
          expect(x.sfixed64).toEqual(Long.fromNumber(0, false));
        },
        x => {
          x.int64 = undefined as any;
          x.sint64 = undefined as any;
          x.sfixed64 = undefined as any;
        },
      );
    });

    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          int64: [Long.fromNumber(1, false)],
          sint64: [Long.fromNumber(2, false)],
          sfixed64: [Long.fromNumber(3, false)],
        },
        x => {
          expect(x.int64).toEqual([Long.fromNumber(1, false), Long.fromNumber(101, false)]);
          expect(x.sint64).toEqual([Long.fromNumber(2, false), Long.fromNumber(102, false)]);
          expect(x.sfixed64).toEqual([Long.fromNumber(3, false), INT64_MIN, INT64_MAX]);
        },
        x => {
          x.int64 = [...x.int64, Long.fromNumber(101, false)];
          x.sint64 = [...x.sint64, Long.fromNumber(102, false)];
          x.sfixed64 = [...x.sfixed64, INT64_MIN, INT64_MAX];
        },
      );
    });
  });

  describe('unsigned 64-bit fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.uint64).toEqual(Long.fromNumber(0, true));
        expect(x.fixed64).toEqual(Long.fromNumber(0, true));
      });
    });
    it('normal values', () => {
      testMessage(
        test.Message,
        {
          uint64: Long.fromNumber(1, true),
          fixed64: Long.fromNumber(2, true),
        },
        x => {
          expect(x.uint64).toEqual(Long.fromNumber(1, true));
          expect(x.fixed64).toEqual(Long.fromNumber(2, true));
        },
      );
    });
    it('large values', () => {
      testMessage(
        test.Message,
        {
          uint64: Long.fromBits(UINT32_MAX, UINT32_MAX, true),
          fixed64: Long.fromBits(UINT32_MAX, UINT32_MAX, true),
        },
        x => {
          expect(x.uint64).toEqual(Long.fromBits(UINT32_MAX, UINT32_MAX, true));
          expect(x.fixed64).toEqual(Long.fromBits(UINT32_MAX, UINT32_MAX, true));
        },
      );
      testMessage(
        test.Message,
        {
          uint64: Long.fromNumber(UINT32_MAX + 1, true),
          fixed64: Long.fromNumber(UINT32_MAX + 1, true),
        },
        x => {
          expect(x.uint64).toEqual(Long.fromNumber(UINT32_MAX + 1, true));
          expect(x.fixed64).toEqual(Long.fromNumber(UINT32_MAX + 1, true));
        },
      );
      testMessage(
        test.Message,
        {
          uint64: Long.fromNumber(INT32_MAX + 1, true),
          fixed64: Long.fromNumber(INT32_MAX + 1, true),
        },
        x => {
          expect(x.uint64).toEqual(Long.fromNumber(INT32_MAX + 1, true));
          expect(x.fixed64).toEqual(Long.fromNumber(INT32_MAX + 1, true));
        },
      );
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.uint64).toEqual([]);
        expect(x.fixed64).toEqual([]);
      });
    });
    it('normal repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          uint64: [Long.fromNumber(1, true), Long.fromNumber(2, true)],
          fixed64: [Long.fromNumber(3, true), Long.fromNumber(4, true)],
        },
        x => {
          expect(x.uint64).toEqual([Long.fromNumber(1, true), Long.fromNumber(2, true)]);
          expect(x.fixed64).toEqual([Long.fromNumber(3, true), Long.fromNumber(4, true)]);
        },
      );
    });
    it('large repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          uint64: [Long.fromBits(UINT32_MAX, UINT32_MAX, true)],
          fixed64: [Long.fromBits(UINT32_MAX, UINT32_MAX, true)],
        },
        x => {
          expect(x.uint64).toEqual([Long.fromBits(UINT32_MAX, UINT32_MAX, true)]);
          expect(x.fixed64).toEqual([Long.fromBits(UINT32_MAX, UINT32_MAX, true)]);
        },
      );
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        {
          uint64: Long.fromNumber(1, true),
          fixed64: Long.fromNumber(2, true),
        },
        x => {
          expect(x.uint64).toEqual(Long.fromNumber(2, true));
          expect(x.fixed64).toEqual(Long.fromNumber(3, true));
        },
        x => {
          x.uint64 = Long.fromNumber(2, true);
          x.fixed64 = Long.fromNumber(3, true);
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        {
          uint64: Long.fromNumber(1, true),
          fixed64: Long.fromNumber(2, true),
        },
        x => {
          expect(x.uint64).toEqual(Long.fromNumber(0, true));
          expect(x.fixed64).toEqual(Long.fromNumber(0, true));
        },
        x => {
          x.uint64 = undefined as any;
          x.fixed64 = undefined as any;
        },
      );
    });

    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          uint64: [Long.fromNumber(1, true)],
          fixed64: [Long.fromNumber(2, true)],
        },
        x => {
          expect(x.uint64).toEqual([Long.fromNumber(1, true), Long.fromNumber(101, true)]);
          expect(x.fixed64).toEqual([Long.fromNumber(2, true), UINT64_MIN, UINT64_MAX]);
        },
        x => {
          x.uint64 = [...x.uint64, Long.fromNumber(101, true)];
          x.fixed64 = [...x.fixed64, UINT64_MIN, UINT64_MAX];
        },
      );
    });
  });

  describe('float fields', () => {
    function floor(value: number, decimals: number): number {
      const pow = Math.pow(10, decimals);
      return Math.floor(value * pow) / pow;
    }
    function ceil(value: number, decimals: number): number {
      const pow = Math.pow(10, decimals);
      return Math.ceil(value * pow) / pow;
    }
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.float).toEqual(0);
      });
    });
    it('positive values', () => {
      testMessage(test.Message, { float: 1 }, x => {
        expect(x.float).toEqual(1);
      });
      testMessage(test.Message, { float: 100.123 }, x => {
        expect(floor(x.float, 3)).toEqual(100.123);
      });
    });
    it('negative values', () => {
      testMessage(test.Message, { float: -1 }, x => {
        expect(x.float).toEqual(-1);
      });
      testMessage(test.Message, { float: -100.123 }, x => {
        expect(ceil(x.float, 3)).toEqual(-100.123);
      });
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.float).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(test.RepeatedMessage, { float: [1.1, -2.2] }, x => {
        expect(x.float.length).toEqual(2);
        expect(floor(x.float[0], 2)).toEqual(1.1);
        expect(ceil(x.float[1], 2)).toEqual(-2.2);
      });
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        { float: 1.1 },
        x => {
          expect(floor(x.float, 1)).toEqual(2.2);
        },
        x => {
          x.float *= 2;
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        { float: 1.1 },
        x => {
          expect(x.float).toEqual(0);
        },
        x => {
          x.float = undefined as any;
        },
      );
      testMessage(
        test.Message,
        { float: 1.1 },
        x => {
          expect(x.float).toEqual(0);
        },
        x => {
          x.float = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        { float: [1.1] },
        x => {
          expect(x.float.length).toEqual(2);
          expect(floor(x.float[0], 2)).toEqual(1.1);
          expect(floor(x.float[1], 2)).toEqual(2.2);
        },
        x => {
          x.float = [...x.float, 2.2];
        },
      );
    });
  });

  describe('double fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.double).toEqual(0);
      });
    });
    it('positive values', () => {
      testMessage(test.Message, { double: 1 }, x => {
        expect(x.double).toEqual(1);
      });
      testMessage(test.Message, { double: 100.123 }, x => {
        expect(x.double).toEqual(100.123);
      });
    });
    it('negative values', () => {
      testMessage(test.Message, { double: -1 }, x => {
        expect(x.double).toEqual(-1);
      });
      testMessage(test.Message, { double: -100.123 }, x => {
        expect(x.double).toEqual(-100.123);
      });
    });
    it('large values', () => {
      testMessage(test.Message, { double: UINT32_MAX }, x => {
        expect(x.double).toEqual(UINT32_MAX);
      });
      testMessage(test.Message, { double: -UINT32_MAX }, x => {
        expect(x.double).toEqual(-UINT32_MAX);
      });
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.double).toEqual([]);
      });
    });
    it('normal repeated values', () => {
      testMessage(test.RepeatedMessage, { double: [1.1, 2.2] }, x => {
        expect(x.double).toEqual([1.1, 2.2]);
      });
    });
    it('large repeated values', () => {
      testMessage(test.RepeatedMessage, { double: [UINT32_MAX, -UINT32_MAX] }, x => {
        expect(x.double).toEqual([UINT32_MAX, -UINT32_MAX]);
      });
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        { double: 1.1 },
        x => {
          expect(x.double).toEqual(2.2);
        },
        x => {
          x.double *= 2;
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        { double: 1 },
        x => {
          expect(x.double).toEqual(0);
        },
        x => {
          x.double = undefined as any;
        },
      );
      testMessage(
        test.Message,
        { double: 1 },
        x => {
          expect(x.double).toEqual(0);
        },
        x => {
          x.double = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        { double: [1.1] },
        x => {
          expect(x.double).toEqual([1.1, 2.2]);
        },
        x => {
          x.double = [...x.double, 2.2];
        },
      );
    });
  });

  describe('bool fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.bool).toEqual(false);
      });
    });
    it('normal values', () => {
      testMessage(test.Message, { bool: true }, x => {
        expect(x.bool).toEqual(true);
      });
      testMessage(test.Message, { bool: false }, x => {
        expect(x.bool).toEqual(false);
      });
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.bool).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(test.RepeatedMessage, { bool: [false, true] }, x => {
        expect(x.bool).toEqual([false, true]);
      });
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        { bool: false },
        x => {
          expect(x.bool).toEqual(true);
        },
        x => {
          x.bool = !x.bool;
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        { bool: true },
        x => {
          expect(x.bool).toEqual(false);
        },
        x => {
          x.bool = undefined as any;
        },
      );
      testMessage(
        test.Message,
        { bool: true },
        x => {
          expect(x.bool).toEqual(false);
        },
        x => {
          x.bool = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        { bool: [false] },
        x => {
          expect(x.bool).toEqual([false, true]);
        },
        x => {
          x.bool = [...x.bool, true];
        },
      );
    });
  });

  describe('string fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.string).toEqual('');
      });
    });
    it('empty values', () => {
      testMessage(test.Message, { string: '' }, x => {
        expect(x.string).toEqual('');
      });
    });
    it('normal values', () => {
      testMessage(test.Message, { string: 'abc123' }, x => {
        expect(x.string).toEqual('abc123');
      });
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.string).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(test.RepeatedMessage, { string: ['value1', 'value2', ''] }, x => {
        expect(x.string).toEqual(['value1', 'value2', '']);
      });
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        { string: 'value1' },
        x => {
          expect(x.string).toEqual('value2');
        },
        x => {
          x.string = 'value2';
        },
      );
      testMessage(
        test.Message,
        { string: 'value1' },
        x => {
          expect(x.string).toEqual('');
        },
        x => {
          x.string = '';
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        { string: 'value1' },
        x => {
          expect(x.string).toEqual('');
        },
        x => {
          x.string = undefined as any;
        },
      );
      testMessage(
        test.Message,
        { string: 'value1' },
        x => {
          expect(x.string).toEqual('');
        },
        x => {
          x.string = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        { string: ['value1'] },
        x => {
          expect(x.string).toEqual(['value1', 'value2', 'value3']);
        },
        x => {
          x.string = [...x.string, 'value2', 'value3'];
        },
      );
    });
  });

  describe('bytes fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.bytes).toEqual(new Uint8Array([]));
      });
    });
    it('normal values', () => {
      testMessage(test.Message, { bytes: new Uint8Array([0, 1, 2, 3]) }, x => {
        expect(x.bytes).toEqual(new Uint8Array([0, 1, 2, 3]));
      });
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.bytes).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(test.RepeatedMessage, { bytes: [new Uint8Array([1, 2]), new Uint8Array([3, 4])] }, x => {
        expect(x.bytes).toEqual([new Uint8Array([1, 2]), new Uint8Array([3, 4])]);
      });
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        { bytes: new Uint8Array([0, 1]) },
        x => {
          expect(x.bytes).toEqual(new Uint8Array([0, 1, 2, 3]));
        },
        x => {
          x.bytes = new Uint8Array([0, 1, 2, 3]);
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        { bytes: new Uint8Array([0, 1]) },
        x => {
          expect(x.bytes).toEqual(new Uint8Array([]));
        },
        x => {
          x.bytes = undefined as any;
        },
      );
      testMessage(
        test.Message,
        { bytes: new Uint8Array([0, 1]) },
        x => {
          expect(x.bytes).toEqual(new Uint8Array([]));
        },
        x => {
          x.bytes = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        { bytes: [new Uint8Array([0, 1])] },
        x => {
          expect(x.bytes).toEqual([new Uint8Array([0, 1]), new Uint8Array([2, 3])]);
        },
        x => {
          x.bytes = [...x.bytes, new Uint8Array([2, 3])];
        },
      );
    });
    describe('nested bytes decoded from a buffer', () => {
      const payload = new Uint8Array([9, 8, 7, 6, 5]);
      const encoded = test.Message.create(new Arena(), {
        string: 'header',
        selfMessage: { int32: 42, bytes: payload },
        int32: 7,
      }).encode();

      const expectPayload = (buffer: Uint8Array) => {
        const decoded = test.Message.decode(new Arena(), buffer);
        expect(decoded.selfMessage!.bytes).toEqual(payload);
        expect(decoded.selfMessage!.int32).toEqual(42);
      };

      it('from native-backed buffer (control)', () => {
        expectPayload(encoded);
      });
      it('from Base64.toByteArray', () => {
        expectPayload(Base64.toByteArray(Base64.fromByteArray(encoded)));
      });
      it('from JS Uint8Array copy', () => {
        expectPayload(new Uint8Array(encoded));
      });
      it('from JS subarray', () => {
        const padded = new Uint8Array(encoded.length + 4);
        padded.set(encoded, 2);
        expectPayload(padded.subarray(2, 2 + encoded.length));
      });
    });
  });

  describe('enum fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.enum).toEqual(test.Enum.VALUE_0);
      });
    });
    it('normal values', () => {
      testMessage(test.Message, { enum: test.Enum.VALUE_0 }, x => {
        expect(x.enum).toEqual(test.Enum.VALUE_0);
      });
      testMessage(test.Message, { enum: test.Enum.VALUE_1 }, x => {
        expect(x.enum).toEqual(test.Enum.VALUE_1);
      });
    });
    it('unknown values', () => {
      testMessage(test.Message, { enum: 100 as unknown as test.Enum }, x => {
        expect(x.enum).toEqual(test.Enum.$UNRECOGNIZED_VALUE);
      });
    });
    it('setting unknown values should be preserved', () => {
      const arena = new Arena();
      const newMessage = test.NewEnumMessage.create(arena, {
        value: test.NewEnumMessage.NewEnum.VALUE_2,
        repeatedValue: [
          test.NewEnumMessage.NewEnum.VALUE_1,
          test.NewEnumMessage.NewEnum.VALUE_2,
          test.NewEnumMessage.NewEnum.VALUE_3,
        ],
        mappedValue: new Map([
          ['a', test.NewEnumMessage.NewEnum.VALUE_1],
          ['b', test.NewEnumMessage.NewEnum.VALUE_2],
          ['c', test.NewEnumMessage.NewEnum.VALUE_3],
        ]),
      });

      const oldMessage = test.OldEnumMessage.decode(arena, newMessage.encode());
      expect(oldMessage.value).toEqual(test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE);
      expect(oldMessage.repeatedValue).toEqual([
        test.OldEnumMessage.OldEnum.VALUE_1,
        test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE,
        test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE,
      ]);
      expect(oldMessage.mappedValue).toEqual(
        new Map([
          ['a', test.OldEnumMessage.OldEnum.VALUE_1],
          ['b', test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE],
          ['c', test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE],
        ]),
      );

      let decodedNewMessage = test.NewEnumMessage.decode(arena, oldMessage.encode());
      expect(decodedNewMessage.value).toEqual(test.NewEnumMessage.NewEnum.VALUE_2);
      expect(decodedNewMessage.repeatedValue).toEqual([
        test.NewEnumMessage.NewEnum.VALUE_1,
        test.NewEnumMessage.NewEnum.VALUE_2,
        test.NewEnumMessage.NewEnum.VALUE_3,
      ]);
      expect(decodedNewMessage.mappedValue).toEqual(
        new Map([
          ['a', test.NewEnumMessage.NewEnum.VALUE_1],
          ['b', test.NewEnumMessage.NewEnum.VALUE_2],
          ['c', test.NewEnumMessage.NewEnum.VALUE_3],
        ]),
      );

      oldMessage.value = test.NewEnumMessage.NewEnum.VALUE_3 as any;
      expect(oldMessage.value).toEqual(test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE);
      oldMessage.repeatedValue = [test.NewEnumMessage.NewEnum.VALUE_2, test.NewEnumMessage.NewEnum.VALUE_3] as any;
      expect(oldMessage.repeatedValue).toEqual([
        test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE,
        test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE,
      ]);
      oldMessage.mappedValue = new Map([
        ['a', test.NewEnumMessage.NewEnum.VALUE_1 as any],
        ['b', test.NewEnumMessage.NewEnum.VALUE_2 as any],
        ['c', test.NewEnumMessage.NewEnum.VALUE_3 as any],
      ]);
      expect(oldMessage.mappedValue).toEqual(
        new Map([
          ['a', test.OldEnumMessage.OldEnum.VALUE_1],
          ['b', test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE],
          ['c', test.OldEnumMessage.OldEnum.$UNRECOGNIZED_VALUE],
        ]),
      );

      decodedNewMessage = test.NewEnumMessage.decode(arena, oldMessage.encode());
      expect(decodedNewMessage.value).toEqual(test.NewEnumMessage.NewEnum.VALUE_3);
      expect(decodedNewMessage.repeatedValue).toEqual([
        test.NewEnumMessage.NewEnum.VALUE_2,
        test.NewEnumMessage.NewEnum.VALUE_3,
      ]);
      expect(decodedNewMessage.mappedValue).toEqual(
        new Map([
          ['a', test.NewEnumMessage.NewEnum.VALUE_1],
          ['b', test.NewEnumMessage.NewEnum.VALUE_2],
          ['c', test.NewEnumMessage.NewEnum.VALUE_3],
        ]),
      );
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.enum).toEqual([]);
      });
    });
    it('normal repeated values', () => {
      testMessage(test.RepeatedMessage, { enum: [test.Enum.VALUE_1, test.Enum.VALUE_0] }, x => {
        expect(x.enum).toEqual([test.Enum.VALUE_1, test.Enum.VALUE_0]);
      });
    });
    it('unknown repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        { enum: [test.Enum.VALUE_1, test.Enum.VALUE_0, 100 as unknown as test.Enum] },
        x => {
          expect(x.enum).toEqual([test.Enum.VALUE_1, test.Enum.VALUE_0, test.Enum.$UNRECOGNIZED_VALUE]);
        },
      );
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        { enum: test.Enum.VALUE_0 },
        x => {
          expect(x.enum).toEqual(test.Enum.VALUE_1);
        },
        x => {
          x.enum = test.Enum.VALUE_1;
        },
      );
    });
    it('setting unknown values', () => {
      testMessage(
        test.Message,
        { enum: test.Enum.VALUE_1 },
        x => {
          expect(x.enum).toEqual(test.Enum.$UNRECOGNIZED_VALUE);
        },
        x => {
          x.enum = 100 as unknown as test.Enum;
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        { enum: test.Enum.VALUE_0 },
        x => {
          expect(x.enum).toEqual(test.Enum.VALUE_0);
        },
        x => {
          x.enum = undefined as any;
        },
      );
      testMessage(
        test.Message,
        { enum: test.Enum.VALUE_0 },
        x => {
          expect(x.enum).toEqual(test.Enum.VALUE_0);
        },
        x => {
          x.enum = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        { enum: [test.Enum.VALUE_0] },
        x => {
          expect(x.enum).toEqual([test.Enum.VALUE_0, test.Enum.VALUE_1]);
        },
        x => {
          x.enum = [...x.enum, test.Enum.VALUE_1];
        },
      );
    });
    it('setting repeated unknown values', () => {
      testMessage(
        test.RepeatedMessage,
        { enum: [test.Enum.VALUE_0] },
        x => {
          expect(x.enum).toEqual([test.Enum.VALUE_0, test.Enum.$UNRECOGNIZED_VALUE]);
        },
        x => {
          x.enum = [...x.enum, 100 as unknown as test.Enum];
        },
      );
    });
  });

  describe('message fields', () => {
    it('default values', () => {
      testMessage(test.Message, {}, x => {
        expect(x.selfMessage).toBeFalsy();
        expect(x.otherMessage).toBeFalsy();
      });
    });
    it('normal values', () => {
      testMessage(
        test.Message,
        {
          selfMessage: { int32: 1 },
          otherMessage: { value: 'value' },
        },
        x => {
          expect(x.selfMessage!.int32).toEqual(1);
          expect(x.otherMessage!.value).toEqual('value');
        },
      );
    });
    it('default repeated values', () => {
      testMessage(test.RepeatedMessage, {}, x => {
        expect(x.selfMessage).toEqual([]);
        expect(x.otherMessage).toEqual([]);
      });
    });
    it('repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          selfMessage: [{ int32: [1] }, { int32: [2] }],
          otherMessage: [{ value: 'value1' }, { value: 'value2' }],
        },
        x => {
          expect(x.selfMessage.length).toEqual(2);
          expect(x.selfMessage[0].int32).toEqual([1]);
          expect(x.selfMessage[1].int32).toEqual([2]);
          expect(x.otherMessage.length).toEqual(2);
          expect(x.otherMessage[0].value).toEqual('value1');
          expect(x.otherMessage[1].value).toEqual('value2');
        },
      );
    });
    it('setting values', () => {
      testMessage(
        test.Message,
        {},
        x => {
          expect(x.selfMessage!.int32).toEqual(1);
        },
        (x, arena) => {
          x.selfMessage = test.Message.create(arena, { int32: 1 });
        },
      );
      testMessage(
        test.Message,
        {},
        x => {
          expect(x.otherMessage!.value).toEqual('value');
        },
        (x, arena) => {
          x.otherMessage = test.OtherMessage.create(arena, { value: 'value' });
        },
      );
    });
    it('setting null values', () => {
      testMessage(
        test.Message,
        {
          selfMessage: { int32: 100 },
          otherMessage: { value: 'value' },
        },
        x => {
          expect(x.selfMessage).toBeFalsy();
          expect(x.otherMessage).toBeFalsy();
        },
        x => {
          x.selfMessage = undefined;
          x.otherMessage = undefined as any;
        },
      );
    });
    it('setting repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {},
        x => {
          expect(x.selfMessage[0].int32).toEqual([1]);
        },
        (x, arena) => {
          x.selfMessage = [test.RepeatedMessage.create(arena, { int32: [1] })];
        },
      );
      testMessage(
        test.RepeatedMessage,
        {},
        x => {
          expect(x.otherMessage[0].value).toEqual('value');
        },
        (x, arena) => {
          x.otherMessage = [test.OtherMessage.create(arena, { value: 'value' })];
        },
      );
      testMessage(
        test.RepeatedMessage,
        {
          otherMessage: [{ value: 'abc' }],
        },
        x => {
          expect(x.otherMessage[0].value).toEqual('value');
        },
        (x, arena) => {
          x.otherMessage = [test.OtherMessage.create(arena, { value: 'value' })];
        },
      );
    });
    it('mutating repeated values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          otherMessage: [{ value: 'value' }],
        },
        x => {
          expect(x.otherMessage[0].value).toEqual('abc');
        },
        x => {
          x.otherMessage[0].value = 'abc';
        },
      );
    });
    it('highly nested values', () => {
      testMessage(
        test.RepeatedMessage,
        {
          int32: [-10],
          selfMessage: [
            {
              int32: [-20],
              selfMessage: [
                {
                  int32: [1],
                  otherMessage: [{ value: 'value' }],
                },
                {
                  int32: [2],
                  otherMessage: [{ value: 'value2' }],
                },
              ],
              otherMessage: [{ value: 'value3' }],
            },
          ],
        },
        x => {
          expect(x.int32).toEqual([-10]);
          expect(x.selfMessage[0].int32).toEqual([-20]);
          expect(x.selfMessage[0].selfMessage[0].int32).toEqual([1]);
          expect(x.selfMessage[0].selfMessage[0].otherMessage[0].value).toEqual('test');
          expect(x.selfMessage[0].selfMessage[1].int32).toEqual([2, 3]);
          expect(x.selfMessage[0].selfMessage[1].otherMessage[0].value).toEqual('abc1');
          expect(x.selfMessage[0].otherMessage[0].value).toEqual('value3');
          expect(x.otherMessage[0].value).toEqual('abc2');
        },
        (x, arena) => {
          x.selfMessage[0].selfMessage[1].int32 = [2, 3];
          x.selfMessage[0].selfMessage[0].otherMessage = [test.OtherMessage.create(arena, { value: 'test' })];
          x.selfMessage[0].selfMessage[1].otherMessage[0].value = 'abc1';
          x.otherMessage = [test.OtherMessage.create(arena, { value: 'abc2' })];
        },
      );
    });
    it('updating fields on a referenced message', () => {
      const arena = new Arena();
      const otherMessage = test.OtherMessage.create(arena, { value: 'value' });
      const message1 = test.Message.create(arena, { otherMessage });

      const expectValue = (value: string) => {
        expect(otherMessage.value).toEqual(value);
        expect(message1.otherMessage?.value).toEqual(value);
        expect(test.Message.decode(arena, message1.encode()).otherMessage?.value).toEqual(value);
        expect(test.OtherMessage.decode(arena, otherMessage.encode()).value).toEqual(value);
      };

      expectValue('value');
      otherMessage.value = 'value2';
      expectValue('value2');
      otherMessage.value = 'value3';
      expectValue('value3');
    });
    it('reordering repeated fields', () => {
      testMessage(
        test.RepeatedMessage,
        {
          otherMessage: [{ value: 'value1' }, { value: 'value2' }],
        },
        x => {
          expect(x.otherMessage[0].value).toEqual('value2');
          expect(x.otherMessage[1].value).toEqual('value1');
        },
        x => {
          const [v0, v1] = x.otherMessage;
          x.otherMessage = [v1, v0];
        },
      );
    });
    it('reusing repeated field values', () => {
      const arena = new Arena();
      const message = test.RepeatedMessage.decode(
        arena,
        test.RepeatedMessage.create(arena, {
          otherMessage: [
            test.OtherMessage.create(arena, { value: 'value1' }),
            test.OtherMessage.create(arena, { value: 'value2' }),
          ],
        }).encode(),
      );

      const otherMessage1 = message.otherMessage[0];
      const otherMessage2 = message.otherMessage[1];
      message.otherMessage = [otherMessage2];
      const decoded1 = test.RepeatedMessage.decode(arena, message.encode());
      expect(decoded1.otherMessage.length).toEqual(1);
      expect(decoded1.otherMessage[0].value).toEqual('value2');

      const message2 = test.RepeatedMessage.create(arena, { otherMessage: [otherMessage2.clone(), otherMessage1] });
      const decoded2 = test.RepeatedMessage.decode(arena, message2.encode());
      expect(decoded2.otherMessage.length).toEqual(2);
      expect(decoded2.otherMessage[0].value).toEqual('value2');
      expect(decoded2.otherMessage[1].value).toEqual('value1');
    });
  });

  describe('one-of fields', () => {
    it('default values', () => {
      testMessage(test.OneOfMessage, {}, x => {
        expect(x.string0).toEqual('');
        expect(x.string1).toEqual('');
        expect(x.message0).toBeFalsy();
        expect(x.message1).toBeFalsy();
        expect(x.bytes0).toEqual(new Uint8Array([]));
        expect(x.bytes1).toEqual(new Uint8Array([]));
      });
    });

    it('should always serialize default values', () => {
      testMessage(
        test.OneOfMessage,
        {
          message1: {},
        },
        x => {
          expect(x.string0).toEqual('');
          expect(x.string1).toEqual('');
          expect(x.message0).toBeFalsy();
          expect(x.message1).toBeTruthy();
          expect(x.bytes0).toEqual(new Uint8Array([]));
          expect(x.bytes1).toEqual(new Uint8Array([]));
        },
      );
    });

    it('normal values', () => {
      testMessage(
        test.OneOfMessage,
        {
          string0: 'value',
          message0: { value: 'abc' },
          bytes0: new Uint8Array([1, 2, 3, 4]),
        },
        x => {
          expect(x.string0).toEqual('value');
          expect(x.string1).toEqual('');
          expect(x.message0!.value).toEqual('abc');
          expect(x.message1).toBeFalsy();
          expect(x.bytes0).toEqual(new Uint8Array([1, 2, 3, 4]));
          expect(x.bytes1).toEqual(new Uint8Array([]));
        },
      );
    });
    it('updating values should clear others', () => {
      testMessage(
        test.OneOfMessage,
        {
          string0: 'value',
          message0: { value: 'abc' },
          bytes0: new Uint8Array([1, 2, 3, 4]),
        },
        x => {
          expect(x.string0).toEqual('');
          expect(x.string1).toEqual('abc');
          expect(x.message0).toBeFalsy();
          expect(x.message1?.value).toEqual('def');
          expect(x.bytes0).toEqual(new Uint8Array([]));
          expect(x.bytes1).toEqual(new Uint8Array([5, 6, 7, 8]));
        },
        (x, arena) => {
          x.string1 = 'abc';
          x.message1 = test.OtherMessage.create(arena, { value: 'def' });
          x.bytes1 = new Uint8Array([5, 6, 7, 8]);
        },
      );
    });
  });

  describe('map fields', () => {
    it('default values', () => {
      testMessage(test.MapMessage, {}, x => {
        expect(x.stringToString.values.length).toEqual(0);
        expect(x.stringToNumber.values.length).toEqual(0);
        expect(x.stringToUnsignedLong.values.length).toEqual(0);
        expect(x.stringToSignedLong.values.length).toEqual(0);
        expect(x.stringToMessage.values.length).toEqual(0);
      });
    });
    it('string to string', () => {
      testMessage(
        test.MapMessage,
        {
          stringToString: new Map([['a', 'b']]),
        },
        x => {
          expect(x.stringToString).toEqual(new Map([['a', 'b']]));
        },
      );
    });
    it('updating string to string', () => {
      testMessage(
        test.MapMessage,
        {
          stringToString: new Map([['a', 'b']]),
        },
        x => {
          expect(x.stringToString).toEqual(
            new Map([
              ['a', 'e'],
              ['c', 'd'],
            ]),
          );
        },
        x => {
          x.stringToString = new Map([
            ['a', 'e'],
            ['c', 'd'],
          ]);
        },
      );
    });
    it('string to number', () => {
      testMessage(
        test.MapMessage,
        {
          stringToNumber: new Map([['a', 1]]),
        },
        x => {
          expect(x.stringToNumber).toEqual(new Map([['a', 1]]));
        },
      );
    });
    it('updating string to string', () => {
      testMessage(
        test.MapMessage,
        {
          stringToNumber: new Map([['a', 1]]),
        },
        x => {
          expect(x.stringToNumber).toEqual(
            new Map([
              ['a', 2],
              ['b', 3],
            ]),
          );
        },
        x => {
          x.stringToNumber = new Map([
            ['a', 2],
            ['b', 3],
          ]);
        },
      );
    });
    it('string to long', () => {
      testMessage(
        test.MapMessage,
        {
          stringToUnsignedLong: new Map([['a', Long.fromNumber(123, true)]]),
          stringToSignedLong: new Map([['a', Long.fromNumber(123, false)]]),
        },
        x => {
          expect(x.stringToUnsignedLong).toEqual(new Map([['a', Long.fromNumber(123, true)]]));
          expect(x.stringToSignedLong).toEqual(new Map([['a', Long.fromNumber(123, false)]]));
        },
      );
    });
    it('updating string to long', () => {
      testMessage(
        test.MapMessage,
        {
          stringToUnsignedLong: new Map([['a', Long.fromNumber(123, true)]]),
          stringToSignedLong: new Map([['a', Long.fromNumber(123, false)]]),
        },
        x => {
          expect(x.stringToUnsignedLong).toEqual(
            new Map([
              ['a', Long.fromNumber(2, true)],
              ['b', Long.fromNumber(3, true)],
            ]),
          );
          expect(x.stringToSignedLong).toEqual(
            new Map([
              ['a', Long.fromNumber(2, false)],
              ['b', Long.fromNumber(3, false)],
            ]),
          );
        },
        x => {
          x.stringToUnsignedLong = new Map([
            ['a', Long.fromNumber(2, true)],
            ['b', Long.fromNumber(3, true)],
          ]);
          x.stringToSignedLong = new Map([
            ['a', Long.fromNumber(2, false)],
            ['b', Long.fromNumber(3, false)],
          ]);
        },
      );
    });
    it('string to double', () => {
      testMessage(
        test.MapMessage,
        {
          stringToDouble: new Map([['a', 123.123]]),
        },
        x => {
          expect(x.stringToDouble).toEqual(new Map([['a', 123.123]]));
        },
      );
    });
    it('updating string to double', () => {
      testMessage(
        test.MapMessage,
        {
          stringToDouble: new Map([['a', 123.123]]),
        },
        x => {
          expect(x.stringToDouble).toEqual(
            new Map([
              ['a', 123.234],
              ['b', 100.543],
            ]),
          );
        },
        x => {
          x.stringToDouble = new Map([
            ['a', 123.234],
            ['b', 100.543],
          ]);
        },
      );
    });
    it('string to message', () => {
      testMessage(
        test.MapMessage,
        {
          stringToMessage: new Map([['a', { value: '123' }]]),
        },
        x => {
          expect(x.stringToMessage.size).toEqual(1);
          expect(x.stringToMessage.get('a')?.value).toEqual('123');
        },
      );
    });
    it('updating string to message', () => {
      testMessage(
        test.MapMessage,
        {
          stringToMessage: new Map([['a', { value: '123' }]]),
        },
        x => {
          expect(x.stringToMessage.size).toEqual(2);
          expect(x.stringToMessage.get('a')?.value).toEqual('234');
          expect(x.stringToMessage.get('b')?.value).toEqual('xyz');
        },
        (x, arena) => {
          x.stringToMessage = new Map([
            ['a', test.OtherMessage.create(arena, { value: '234' })],
            ['b', test.OtherMessage.create(arena, { value: 'xyz' })],
          ]);
        },
      );
    });
    it('appending message to string to message', () => {
      testMessage(
        test.MapMessage,
        {
          stringToMessage: new Map([['a', { value: '123' }]]),
        },
        x => {
          expect(x.stringToMessage.size).toEqual(2);
          expect(x.stringToMessage.get('a')?.value).toEqual('123');
          expect(x.stringToMessage.get('b')?.value).toEqual('456');
        },
        x => {
          const originalMessage = x.stringToMessage.get('a')!;
          const newMessage = test.OtherMessage.create(x.$arena, { value: '456' });

          x.stringToMessage = new Map([
            ['a', originalMessage],
            ['b', newMessage],
          ]);
        },
      );
    });
    it('updating string to message values through reference', () => {
      const arena = new Arena();
      const otherMessage = test.OtherMessage.create(arena, { value: '123' });
      const x = test.MapMessage.create(arena, {
        stringToMessage: new Map([['a', otherMessage]]),
      });
      expect(x.stringToMessage.get('a')!.value).toEqual('123');
      otherMessage.value = '234';
      expect(x.stringToMessage.get('a')!.value).toEqual('234');
      otherMessage.value = '345';
      const decoded = test.MapMessage.decode(arena, x.encode());
      expect(x.stringToMessage.get('a')!.value).toEqual('345');
      otherMessage.value = '456';
      expect(x.stringToMessage.get('a')!.value).toEqual('456');
      expect(decoded.stringToMessage.get('a')!.value).toEqual('345');
    });
    it('int to string', () => {
      testMessage(
        test.MapMessage,
        {
          intToString: new Map([[100, 'b']]),
        },
        x => {
          expect(x.intToString).toEqual(new Map([[100, 'b']]));
        },
      );
    });
    it('updating int to string', () => {
      testMessage(
        test.MapMessage,
        {
          intToString: new Map([[100, 'a']]),
        },
        x => {
          expect(x.intToString).toEqual(
            new Map([
              [100, 'b'],
              [200, 'c'],
            ]),
          );
        },
        x => {
          x.intToString = new Map([
            [100, 'b'],
            [200, 'c'],
          ]);
        },
      );
    });
    it('long to string', () => {
      testMessage(
        test.MapMessage,
        {
          longToString: new Map([[Long.fromNumber(100, true), 'b']]),
        },
        x => {
          const keys = Array.from(x.longToString.keys());
          expect(x.longToString.get(keys[0])).toEqual('b');
        },
      );
    });
    it('updating long to string', () => {
      testMessage(
        test.MapMessage,
        {
          longToString: new Map([[Long.fromNumber(100, true), 'a']]),
        },
        x => {
          const keys = Array.from(x.longToString.keys());
          expect(x.longToString.get(keys[0])).toEqual('b');
        },
        x => {
          const keys = Array.from(x.longToString.keys());
          const newMap = new Map(x.longToString);
          newMap.set(keys[0], 'b');
          x.longToString = newMap;
        },
      );
    });
    it('should be mutable', () => {
      testMessage(
        test.MapMessage,
        { stringToString: new Map([['a', 'abc']]) },
        x => {
          expect(x.stringToString.get('a')).toEqual('123');
          expect(x.stringToString.get('b')).toEqual('def');
          expect(x.stringToDouble.get('c')).toEqual(100.123);
        },
        x => {
          const newStringToString = new Map(x.stringToString);
          const newStringToDouble = new Map(x.stringToDouble);

          newStringToString.set('a', '123');
          newStringToString.set('b', 'def');
          newStringToDouble.set('c', 100.123);

          x.stringToString = newStringToString;
          x.stringToDouble = newStringToDouble;
        },
      );
    });
  });

  describe('repeated fields', () => {
    it('should be mutable', () => {
      testMessage(
        test.RepeatedMessage,
        { string: ['abc'] },
        x => {
          expect(x.string).toEqual(['abc', '123']);
          expect(x.double).toEqual([100.123, 200.123]);
        },
        x => {
          x.string = [...x.string, '123'];
          x.double = [...x.double, 100.123, 200.123];
        },
      );
    });
  });

  it('nested messages and enums should work', () => {
    const arena = new Arena();
    const child = test.ParentMessage.ChildMessage.create(arena, {});
    expect(child).toBeTruthy();

    testMessage(test.ParentMessage.ChildMessage, { value: 'val' }, x => {
      expect(x.value).toEqual('val');
    });
    testMessage(
      test.ParentMessage,
      {
        childEnum: test.ParentMessage.ChildEnum.VALUE_1,
        childMessage: { value: 'val' },
      },
      x => {
        expect(x.childEnum).toEqual(test.ParentMessage.ChildEnum.VALUE_1);
        expect(x.childMessage!.value).toEqual('val');
      },
    );
  });

  describe('external messages', () => {
    it('messages from another file', () => {
      testMessage(
        test.ExternalMessages,
        {
          otherMessage: { value: 'abc' },
        },
        x => {
          expect(x.otherMessage!.value).toEqual('abc');
        },
      );
    });
  });

  it('unknown fields should be preserved', () => {
    const arena = new Arena();
    const newMessage = test.NewMessage.create(arena, { oldValue: 'old', newValue: 'new' });
    const oldMessage = test.OldMessage.decode(arena, newMessage.encode());
    expect(oldMessage.oldValue).toEqual('old');
    oldMessage.oldValue = 'old2';
    const decodedNewMessage = test.NewMessage.decode(arena, oldMessage.encode());
    expect(decodedNewMessage.oldValue).toEqual('old2');
    expect(decodedNewMessage.newValue).toEqual('new');
  });

  describe('message properties', () => {
    it('message destructuring should work correctly', () => {
      const arena = new Arena();
      const message = test.Message.create(arena, { int32: 1, string: 'a' });
      const { int32, string } = message;
      expect(int32).toEqual(1);
      expect(string).toEqual('a');
    });
    it('message keys should contain all available fields', () => {
      const arena = new Arena();
      const message = test.ParentMessage.create(arena);
      const allKeys = [];
      for (const key in message) {
        if (!key.startsWith('$')) {
          allKeys.push(key);
        }
      }
      const keySet = new Set(allKeys);
      expect(keySet.size).toEqual(2);
      expect(keySet.has('childMessage')).toBeTruthy();
      expect(keySet.has('childEnum')).toBeTruthy();
    });
  });

  describe('bad inputs', () => {
    it('decoding bad data should throw', () => {
      const arena = new Arena();
      expect(() => test.Message.decode(arena, new Uint8Array([0, 1, 2, 3]))).toThrow();
      expect(() => test.Message.decode(arena, undefined as any)).toThrow();
      expect(() => test.Message.decode(arena, null as any)).toThrow();
      expect(() => test.Message.decode(arena, 'abc' as any)).toThrow();
      expect(() => test.Message.decode(arena, 123 as any)).toThrow();
      expect(() => test.Message.decode(arena, {} as any)).toThrow();
    });
  });

  describe('multiple arenas', () => {
    it('setting message value from a different arena', () => {
      const message1 = test.Message.create(new Arena(), { int32: 100 });
      const message2 = test.Message.create(new Arena(), { int32: 200 });
      message1.selfMessage = message2;
      expect(message1.selfMessage?.int32).toEqual(message2.int32);
    });
    it('create message with field from a different arena', () => {
      const message = test.Message.create(new Arena(), {
        int32: 100,
        selfMessage: test.Message.create(new Arena(), {
          int32: 200,
        }),
      });
      expect(message.int32).toEqual(100);
      expect(message.selfMessage?.int32).toEqual(200);
    });
    it('setting cloned message value from a different arena', () => {
      const message1 = test.Message.create(new Arena(), { int32: 100 });
      const message2 = test.Message.create(new Arena(), { int32: 200 });
      message1.selfMessage = message2.clone();
      expect(message1.selfMessage?.int32).toEqual(message2.int32);
    });
    it('setting cloned message value from the same arena', () => {
      const message1 = test.Message.create(new Arena(), { int32: 100 });
      const message2 = test.Message.create(new Arena(), { int32: 200 });
      message1.selfMessage = message2.clone(message1.$arena);
      expect(message1.selfMessage?.int32).toEqual(message2.int32);
    });
  });

  describe('to plain object conversion', () => {
    it('can copy values', () => {
      const message = test.Message.create(new Arena(), { uint32: 42, string: 'hello' });
      const newMessage = message.toPlainObject() as test.IMessage;

      expect((newMessage as any).prototype).toBeUndefined();
      expect(newMessage.uint32).toBe(42);
      expect(newMessage.string).toBe('hello');
      expect(newMessage).not.toEqual(message);
    });

    it('responds to Object.keys()', () => {
      const message = test.Message.create(new Arena(), {});

      const newMessage = message.toPlainObject();
      expect(Object.keys(newMessage)).toEqual([
        'int32',
        'int64',
        'uint32',
        'uint64',
        'sint32',
        'sint64',
        'fixed32',
        'fixed64',
        'sfixed32',
        'sfixed64',
        'float',
        'double',
        'bool',
        'string',
        'bytes',
        'enum',
      ]);
    });

    it('can be copied through spread', () => {
      const message = test.Message.create(new Arena(), { uint32: 42, string: 'hello' });
      const newMessage = message.toPlainObject();

      const newMessage2 = { ...newMessage };

      expect(newMessage).not.toBe(newMessage2);
      expect(newMessage).toEqual(newMessage2);
    });

    it('copies sub messages', () => {
      const message = test.Message.create(new Arena(), { uint32: 42, selfMessage: { string: 'hello' } });
      const newMessage = message.toPlainObject() as test.IMessage;

      expect(newMessage.uint32).toBe(42);
      expect(newMessage.selfMessage).toBeTruthy();
      expect((newMessage.selfMessage! as any).prototype).toBeUndefined();
      expect(newMessage.selfMessage!.string).toBe('hello');
    });

    it('copies repeated messages', () => {
      const message = test.RepeatedMessage.create(new Arena(), {
        uint32: [1, 2],
        selfMessage: [test.RepeatedMessage.create(new Arena(), { string: ['foo', 'bar'] })],
      });
      const newMessage = message.toPlainObject() as test.IRepeatedMessage;

      expect(newMessage.uint32).toEqual([1, 2]);
      expect(newMessage.selfMessage).toBeTruthy();
      expect((newMessage.selfMessage! as any).prototype).toBeUndefined();
      expect(newMessage.selfMessage!.length).toBe(1);
      expect((newMessage.selfMessage![0] as any).prototype).toBeUndefined();
      expect(newMessage.selfMessage![0].string).toEqual(['foo', 'bar']);
    });

    it('copies map messages', () => {
      const message = test.MapMessage.create(new Arena(), {
        stringToMessage: new Map([['foo', test.OtherMessage.create(new Arena(), { value: 'bar' })]]),
      });
      const newMessage = message.toPlainObject() as test.IMapMessage;

      expect(newMessage.stringToMessage).toBeTruthy();
      expect(newMessage.stringToMessage?.size).toEqual(1);
      expect((newMessage.stringToMessage?.get('foo') as any).prototype).toBeUndefined();
      expect(newMessage.stringToMessage?.get('foo')).toEqual({ value: 'bar' });
    });
  });

  describe('underscores in names', () => {
    it('package, message, and field names', () => {
      testMessage(
        package_with_underscores.Message_With_Underscores,
        {
          fieldUnderscore: 1,
          fieldUnderscoreAgain: 2,
          enumValue: package_with_underscores.Enum_With_Underscores.ENUM_VALUE_0,
        },
        msg => {
          expect(msg.fieldUnderscore).toEqual(100);
          expect(msg.fieldUnderscoreAgain).toEqual(200);
          expect(msg.enumValue).toEqual(package_with_underscores.Enum_With_Underscores.ENUM_VALUE_1);
          expect(msg.messageValue?.fieldUnderscore).toEqual(1);
        },
        (msg, arena) => {
          msg.fieldUnderscore = 100;
          msg.fieldUnderscoreAgain = 200;
          msg.enumValue = package_with_underscores.Enum_With_Underscores.ENUM_VALUE_1;
          msg.messageValue = package_with_underscores.Message_With_Underscores.create(arena, {
            fieldUnderscore: 1,
          });
        },
      );
    });
  });
  it('field names with numbers', () => {
    testMessage(
      package_with_underscores.M3ssage1WithNumb3r2,
      {
        fie1ld: 1,
        field2: 2,
        field3: 3,
        a4Fiel4d: 4,
        a5aFiel5d: 5,
        enum: package_with_underscores.Enum_With_Underscores.E2NUM_VALUE_WITH_NUMBER,
      },
      msg => {
        expect(msg.fie1ld).toEqual(100);
        expect(msg.field2).toEqual(200);
        expect(msg.field3).toEqual(300);
        expect(msg.a4Fiel4d).toEqual(400);
        expect(msg.a5aFiel5d).toEqual(500);
        expect(msg.enum).toEqual(package_with_underscores.Enum_With_Underscores.E2NUM_VALUE_WITH_NUMBER);
      },
      msg => {
        msg.fie1ld = 100;
        msg.field2 = 200;
        msg.field3 = 300;
        msg.a4Fiel4d = 400;
        msg.a5aFiel5d = 500;
      },
    );
  });
  describe('async', () => {
    it('can encode async', async () => {
      const arena = new Arena();

      const message = test.Message.create(arena, {
        bool: true,
        sfixed32: 42,
        string: 'Hello World',
      });

      const encoded = await message.encodeAsync();

      const decoded = test.Message.decode(arena, encoded);

      expect(decoded.bool).toEqual(true);
      expect(decoded.sfixed32).toEqual(42);
      expect(decoded.string).toEqual('Hello World');
    });

    it('can decode async', async () => {
      const encoded = test.Message.encode({
        bool: true,
        sfixed32: 42,
        string: 'Hello World',
      });

      const arena = new Arena();
      const decoded = await test.Message.decodeAsync(arena, encoded);

      expect(decoded.bool).toEqual(true);
      expect(decoded.sfixed32).toEqual(42);
      expect(decoded.string).toEqual('Hello World');
    });

    it('can encode async in batch', async () => {
      const arena = new Arena();

      const message1 = test.Message.create(arena, {
        bool: true,
        sfixed32: 1,
        string: 'Hello World',
      });
      const message2 = test.Message.create(arena, {
        bool: false,
        sfixed32: 2,
        string: 'Goodbye',
      });
      const message3 = test.Message.create(arena, {
        bool: true,
        sfixed32: 3,
        string: 'And Hello Again',
      });

      const decoded = await arena.batchEncodeMessageAsync([message1, message2, message3]);

      expect(decoded.length).toBe(3);

      const decodedMessages = await Promise.all([
        test.Message.decodeAsync(arena, decoded[0]),
        test.Message.decodeAsync(arena, decoded[1]),
        test.Message.decodeAsync(arena, decoded[2]),
      ]);

      expect(decodedMessages.length).toBe(3);

      expect(decodedMessages[0].bool).toBe(true);
      expect(decodedMessages[0].sfixed32).toBe(1);
      expect(decodedMessages[0].string).toBe('Hello World');

      expect(decodedMessages[1].bool).toBe(false);
      expect(decodedMessages[1].sfixed32).toBe(2);
      expect(decodedMessages[1].string).toBe('Goodbye');

      expect(decodedMessages[2].bool).toBe(true);
      expect(decodedMessages[2].sfixed32).toBe(3);
      expect(decodedMessages[2].string).toBe('And Hello Again');
    });
  });

  describe('JSON', () => {
    it('can convert message to JSON', () => {
      const arena = new Arena();
      const message = test.Message.create(arena, {
        int32: 42,
        int64: Long.fromNumber(42000000, false),
        double: 90.5,
        float: Infinity,
        otherMessage: test.OtherMessage.create(arena, {
          value: 'This is a string',
        }),
      });

      const json = message.toDebugJSON();
      const parsedJson = JSON.parse(json);

      expect(parsedJson).toEqual({
        int32: 42,
        double: 90.5,
        float: 'Infinity',
        int64: '42000000',
        otherMessage: {
          value: 'This is a string',
        },
      });
    });

    it('can parse from JSON', async () => {
      const arena = new Arena();
      const message = await test.Message.decodeDebugJSONAsync(
        arena,
        '{"int32": 42, "double": 90.5, "otherMessage": { "value": "This is a string" }}',
      );

      expect(message.int32).toBe(42);
      expect(message.double).toBe(90.5);
      expect(message.otherMessage).toBeDefined();
      expect(message.otherMessage!.value).toBe('This is a string');
    });
  });

  describe('Reflection', () => {
    it('can parse and load proto files', () => {
      interface MyTestMessage {
        title: string;
        count: number;
      }

      const descriptor = loadDescriptorPoolFromProtoFiles(
        'my_test.proto',
        `
      syntax = "proto3";
      package my_test;

      message MyTestMessage {
        string title = 1;
        int32 count = 2;
      }
      `,
      );

      const messageNamespace = descriptor.getMessageNamespace('my_test.MyTestMessage');
      expect(messageNamespace).toBeDefined();
      const message = messageNamespace!.create(new Arena(), {
        title: 'Hello World',
        count: 42,
      }) as unknown as MyTestMessage;

      expect(message.title).toBe('Hello World');
      expect(message.count).toBe(42);
    });
  });
});
