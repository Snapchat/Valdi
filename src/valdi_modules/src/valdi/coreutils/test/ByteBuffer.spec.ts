import { ByteBuffer } from 'coreutils/src/ByteBuffer';
import 'jasmine/src/jasmine';

describe('coreutils > ByteBuffer', () => {
  it('starts empty with the requested capacity', () => {
    const buffer = new ByteBuffer(8);

    expect(buffer.size).toEqual(0);
    expect(buffer.capacity).toEqual(8);
    expect(buffer.inner).toEqual(new Uint8Array(0));
  });

  it('appends data without growing when capacity is sufficient', () => {
    const buffer = new ByteBuffer(4);

    buffer.appendData(new Uint8Array([1, 2]));
    buffer.appendData(new Uint8Array([3]));

    expect(buffer.size).toEqual(3);
    expect(buffer.capacity).toEqual(4);
    expect(buffer.inner).toEqual(new Uint8Array([1, 2, 3]));
  });

  it('grows while preserving existing data', () => {
    const buffer = new ByteBuffer(2);

    buffer.appendData(new Uint8Array([1, 2]));
    buffer.appendData(new Uint8Array([3, 4, 5]));

    expect(buffer.size).toEqual(5);
    expect(buffer.capacity).toBeGreaterThanOrEqual(5);
    expect(buffer.inner).toEqual(new Uint8Array([1, 2, 3, 4, 5]));
  });

  it('supports appending to an initially empty buffer', () => {
    const buffer = new ByteBuffer();

    buffer.appendData(new Uint8Array([9]));

    expect(buffer.size).toEqual(1);
    expect(buffer.capacity).toBeGreaterThanOrEqual(1);
    expect(buffer.inner).toEqual(new Uint8Array([9]));
  });
});
