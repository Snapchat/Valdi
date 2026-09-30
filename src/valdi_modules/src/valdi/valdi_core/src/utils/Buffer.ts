/**
 * A Buffer which extends automatically and can
 * store arbitary bytes.
 */
export class Buffer {
  private array: DataView;
  private pos: number;

  constructor(initialCapacity: number) {
    const buffer = new ArrayBuffer(initialCapacity);
    this.array = new DataView(buffer);
    this.pos = 0;
  }

  inner(): ArrayBuffer {
    return this.array.buffer as ArrayBuffer;
  }

  empty(): boolean {
    return !this.pos;
  }

  /**
   * Reset the position of the buffer, but does not
   * clear it.
   */
  rewind() {
    this.pos = 0;
  }

  size(): number {
    return this.pos;
  }

  private ensureCapacity(desiredCapacity: number): DataView {
    const array = this.array;
    if (desiredCapacity < array.byteLength) {
      return array;
    }

    const newBuffer = new ArrayBuffer(Math.max(array.byteLength * 2, desiredCapacity));
    // Copy the buffers
    new Uint8Array(newBuffer).set(new Uint8Array(array.buffer));

    const newArray = new DataView(newBuffer);
    this.array = newArray;
    return newArray;
  }

  putUint32(value: number) {
    const pos = this.pos;
    const array = this.ensureCapacity(pos + 4);
    array.setUint32(pos, value, true);
    this.pos = pos + 4;
  }

  putUint32_2(value1: number, value2: number) {
    const pos = this.pos;
    const array = this.ensureCapacity(pos + 8);
    array.setUint32(pos, value1, true);
    array.setUint32(pos + 4, value2, true);
    this.pos = pos + 8;
  }

  putUint32_3(value1: number, value2: number, value3: number) {
    const pos = this.pos;
    const array = this.ensureCapacity(pos + 12);
    array.setUint32(pos, value1, true);
    array.setUint32(pos + 4, value2, true);
    array.setUint32(pos + 8, value3, true);
    this.pos = pos + 12;
  }

  putUint32_4(value1: number, value2: number, value3: number, value4: number) {
    const pos = this.pos;
    const array = this.ensureCapacity(pos + 16);
    array.setUint32(pos, value1, true);
    array.setUint32(pos + 4, value2, true);
    array.setUint32(pos + 8, value3, true);
    array.setUint32(pos + 12, value4, true);
    this.pos = pos + 16;
  }

  putUint32_5(value1: number, value2: number, value3: number, value4: number, value5: number) {
    const pos = this.pos;
    const array = this.ensureCapacity(pos + 20);
    array.setUint32(pos, value1, true);
    array.setUint32(pos + 4, value2, true);
    array.setUint32(pos + 8, value3, true);
    array.setUint32(pos + 12, value4, true);
    array.setUint32(pos + 16, value5, true);
    this.pos = pos + 20;
  }

  putFloat64(value: number) {
    const pos = this.pos;
    const array = this.ensureCapacity(pos + 8);
    array.setFloat64(pos, value, true);
    this.pos = pos + 8;
  }
}
