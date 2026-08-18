import { KeyAnimation } from '../animations/KeyAnimation';
import type { AttributeApplier, AttributeApplierContext, CompositeAttribute } from '../core/ElementClass';

export type AnimationInterpolator = (progress: number) => unknown;

export class AttributeAnimation extends KeyAnimation {
  private value: unknown;

  constructor(
    readonly startValue: unknown,
    readonly endValue: unknown,
    minimumVisibleChange: number,
    private readonly interpolate: AnimationInterpolator,
    private readonly definition: AttributeApplier | CompositeAttribute,
    private readonly key: string,
    private readonly element: HTMLElement,
    private readonly context: AttributeApplierContext,
    private readonly finishCallback: (animation: AttributeAnimation) => void,
  ) {
    super(minimumVisibleChange);
    this.value = startValue;
  }

  get currentValue(): unknown {
    return this.value;
  }

  override applyProgress(progress: number): boolean {
    const value = this.interpolate(progress);
    try {
      this.applyValue(value);
    } catch (error) {
      this.logApplyError(value, error);
      return false;
    }
    this.value = value;
    return true;
  }

  override applyFinalValue(): void {
    try {
      if (this.endValue === undefined || this.endValue === null) {
        this.definition.reset(this.element, this.key, this.context);
      } else {
        this.applyValue(this.endValue);
      }
      this.value = this.endValue;
    } catch (error) {
      this.logApplyError(this.endValue, error);
    }
  }

  protected override didFinish(): void {
    this.finishCallback(this);
  }

  private applyValue(value: unknown): void {
    if ('parts' in this.definition) {
      this.definition.apply(this.element, value as ReadonlyArray<unknown>, this.key, this.context);
    } else {
      this.definition.apply(this.element, value, this.key, this.context);
    }
  }

  private logApplyError(value: unknown, error: unknown): void {
    const message = error instanceof Error ? error.message : String(error);
    console.error(
      `Valdi web renderer failed to apply animated attribute '${this.key}' on node ${this.context.id} with value ${stringifyValue(value)}: ${message}`,
    );
  }
}

function stringifyValue(value: unknown): string {
  if (typeof value === 'function') {
    return '[function]';
  }
  try {
    const stringified = JSON.stringify(value);
    return stringified === undefined ? String(value) : stringified;
  } catch (_error) {
    return String(value);
  }
}
