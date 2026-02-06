// @ts-nocheck
import { getLoadedDescriptorDatabase } from './DescriptorDatabaseTestUtils';
import { ScalarType } from '@protobuf-ts/runtime';

describe('DescriptorDatabase', () => {
  it('can load type names', () => {
    const database = getLoadedDescriptorDatabase();

    expect(database.getAllTypeNames().map(t => t.fullName)).toEqual([
      'test3.Message3',
      'test.OtherMessage',
      'test.Message',
      'test.RepeatedMessage',
      'test.ParentMessage.ChildMessage',
      'test.ParentMessage',
      'test.ParentMessage.ChildEnum',
      'test.OneOfMessage',
      'test.OldMessage',
      'test.NewMessage',
      'test.OldEnumMessage',
      'test.OldEnumMessage.OldEnum',
      'test.NewEnumMessage',
      'test.NewEnumMessage.NewEnum',
      'test.MapMessage',
      'test.ExternalMessages',
      'test.Enum',
      'test2.Message2',
      'package_with_underscores.Message_With_Underscores',
      'package_with_underscores.M3ssage1WithNumb3r2',
      'package_with_underscores.Enum_With_Underscores',
    ]);

    const message = database.getMessageType('test.Message');
    expect(message).toBeTruthy();

    const nestedMessage = database.getMessageType('test.ParentMessage.ChildMessage');
    expect(nestedMessage).toBeTruthy();

    const underscorePackage = database.getMessageType('package_with_underscores.Message_With_Underscores');
    expect(underscorePackage).toBeTruthy();
  });

  it('can load root enum type', () => {
    const database = getLoadedDescriptorDatabase();
    const enumInfo = database.getEnumInfo('test.Enum');
    // EnumInfo is [typeName, enumObject]
    const enumObj = enumInfo[1];
    expect(enumObj['VALUE_0']).toBe(0);
    expect(enumObj['VALUE_1']).toBe(1);
  });

  it('can load nested enum type', () => {
    const database = getLoadedDescriptorDatabase();
    const enumInfo = database.getEnumInfo('test.ParentMessage.ChildEnum');
    const enumObj = enumInfo[1];
    expect(enumObj['VALUE_0']).toBe(0);
    expect(enumObj['VALUE_1']).toBe(1);
  });

  it('can load message', () => {
    const database = getLoadedDescriptorDatabase();
    const messageType = database.getMessageType('test.Message');

    // Find int32 field
    const int32Field = messageType.fields.find(f => f.name === 'int32');
    expect(int32Field).toBeTruthy();
    expect(int32Field!.no).toBe(1);
    expect(int32Field!.kind).toBe('scalar');
    if (int32Field!.kind === 'scalar') {
      expect(int32Field!.T).toBe(ScalarType.INT32);
    }

    // Find otherMessage field
    const otherMessageField = messageType.fields.find(f => f.name === 'otherMessage');
    expect(otherMessageField).toBeTruthy();
    expect(otherMessageField!.no).toBe(18);
    expect(otherMessageField!.kind).toBe('message');
  });

  it('can load nested message', () => {
    const database = getLoadedDescriptorDatabase();
    const messageType = database.getMessageType('test.ParentMessage.ChildMessage');

    const valueField = messageType.fields.find(f => f.name === 'value');
    expect(valueField).toBeTruthy();
    expect(valueField!.no).toBe(1);
    expect(valueField!.kind).toBe('scalar');
    if (valueField!.kind === 'scalar') {
      expect(valueField!.T).toBe(ScalarType.STRING);
    }
  });

  it('can load repeated message', () => {
    const database = getLoadedDescriptorDatabase();
    const messageType = database.getMessageType('test.RepeatedMessage');

    const int32Field = messageType.fields.find(f => f.name === 'int32');
    expect(int32Field).toBeTruthy();
    expect(int32Field!.no).toBe(1);
    expect(int32Field!.kind).toBe('scalar');
    // Check if repeated (repeat !== RepeatType.NO which is 0)
    expect(int32Field!.repeat).not.toBe(0);
  });

  it('can load map message', () => {
    const database = getLoadedDescriptorDatabase();
    const messageType = database.getMessageType('test.MapMessage');

    const stringToStringField = messageType.fields.find(f => f.name === 'stringToString');
    expect(stringToStringField).toBeTruthy();
    expect(stringToStringField!.no).toBe(1);
    expect(stringToStringField!.kind).toBe('map');
    if (stringToStringField!.kind === 'map') {
      expect(stringToStringField!.K).toBe(ScalarType.STRING);
      expect(stringToStringField!.V.kind).toBe('scalar');
    }

    const stringToDoubleField = messageType.fields.find(f => f.name === 'stringToDouble');
    expect(stringToDoubleField).toBeTruthy();
    expect(stringToDoubleField!.no).toBe(5);
    expect(stringToDoubleField!.kind).toBe('map');
    if (stringToDoubleField!.kind === 'map') {
      expect(stringToDoubleField!.K).toBe(ScalarType.STRING);
      expect(stringToDoubleField!.V.kind).toBe('scalar');
    }

    const longToStringField = messageType.fields.find(f => f.name === 'longToString');
    expect(longToStringField).toBeTruthy();
    expect(longToStringField!.no).toBe(8);
    expect(longToStringField!.kind).toBe('map');
    if (longToStringField!.kind === 'map') {
      expect(longToStringField!.K).toBe(ScalarType.INT64);
      expect(longToStringField!.V.kind).toBe('scalar');
    }
  });
});
