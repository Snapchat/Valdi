export class ByteBuffer {
  private _inner: Uint8Array;
  private _size = 0;

  get inner(): Uint8Array {
    return this._inner.subarray(0, this._size);
  }

  get size(): number {
    return this._size;
  }

  get capacity(): number {
    return this._inner.length;
  }

  constructor(initialCapacity = 0) {
    this._inner = new Uint8Array(initialCapacity);
  }

  appendData(data: Uint8Array): void {
    const requiredCapacity = this._size + data.length;
    if (requiredCapacity > this.capacity) {
      this.grow(requiredCapacity);
    }

    this._inner.set(data, this._size);
    this._size = requiredCapacity;
  }

  private grow(requiredCapacity: number): void {
    let nextCapacity = this.capacity === 0 ? 1 : this.capacity;
    while (nextCapacity < requiredCapacity) {
      nextCapacity *= 2;
    }

    const nextInner = new Uint8Array(nextCapacity);
    nextInner.set(this.inner);
    this._inner = nextInner;
  }
}
