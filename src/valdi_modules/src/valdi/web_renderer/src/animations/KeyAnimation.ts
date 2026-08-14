export abstract class KeyAnimation {
  private finishedValue = false;

  constructor(readonly minimumVisibleChange: number) {}

  get finished(): boolean {
    return this.finishedValue;
  }

  abstract applyProgress(progress: number): boolean;

  abstract applyFinalValue(): void;

  cancel(): void {
    if (this.finishedValue) {
      return;
    }
    this.finishedValue = true;
    this.didFinish();
  }

  complete(): void {
    if (this.finishedValue) {
      return;
    }
    try {
      this.applyFinalValue();
    } finally {
      this.cancel();
    }
  }

  protected didFinish(): void {}
}
