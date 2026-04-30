export class Lazy<T> {
  private value?: { value: T };
  private work?: () => T;

  constructor(work: () => T) {
    this.work = work;
  }

  get target(): T {
    if (!this.value) {
      this.value = { value: this.work!() };
      this.work = undefined;
    }
    return this.value.value;
  }

  get isCreated(): boolean {
    return this.value !== undefined;
  }
}
