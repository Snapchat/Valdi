export class IndexedRecord<T> {
  private readonly values: Record<string, T | undefined> = {};
  private readonly keyIndexes: Record<string, number | undefined> = {};
  private readonly recordKeys: string[] = [];

  get length(): number {
    return this.recordKeys.length;
  }

  get empty(): boolean {
    return this.recordKeys.length === 0;
  }

  get keys(): string[] {
    return this.recordKeys;
  }

  get(key: string): T | undefined {
    return this.values[key];
  }

  set(key: string, value: T): void {
    if (this.keyIndexes[key] === undefined) {
      this.keyIndexes[key] = this.recordKeys.length;
      this.recordKeys.push(key);
    }
    this.values[key] = value;
  }

  remove(key: string): void {
    const index = this.keyIndexes[key];
    if (index === undefined) {
      return;
    }

    const lastIndex = this.recordKeys.length - 1;
    const lastKey = this.recordKeys[lastIndex];
    this.recordKeys.pop();
    if (index !== lastIndex) {
      this.recordKeys[index] = lastKey;
      this.keyIndexes[lastKey] = index;
    }

    this.keyIndexes[key] = undefined;
    this.values[key] = undefined;
  }

  pop(): T | undefined {
    if (this.recordKeys.length === 0) {
      return undefined;
    }

    const lastIndex = this.recordKeys.length - 1;
    const key = this.recordKeys[lastIndex];
    const value = this.values[key];
    this.recordKeys.pop();
    this.keyIndexes[key] = undefined;
    this.values[key] = undefined;
    return value;
  }

  clear(): void {
    const keys = this.recordKeys;
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      this.keyIndexes[key] = undefined;
      this.values[key] = undefined;
    }
    keys.length = 0;
  }
}
