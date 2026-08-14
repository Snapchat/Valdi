import type { Style } from 'valdi_core/src/Style';
import type { Animator } from '../animations/Animator';
import type {
  AnyElementClass,
  AttributeApplier,
  AttributeApplierContext,
  CompositeAttribute,
  ElementAttribute,
} from '../core/ElementClass';
import { IndexedRecord } from '../utils/IndexedRecord';
import { AttributeAnimation } from './AttributeAnimation';
import type { AttributeOwner } from './AttributeOwner';
import { MIN_VISIBLE_CHANGE_PIXEL } from './AttributesBinder';

type StyleLike = {
  attributes?: Record<string, unknown>;
};

export enum AttributeSetResult {
  Unchanged,
  Changed,
  ChangedAndInvalidatesLayout,
}

type OwnerValue = {
  owner: AttributeOwner;
  value: unknown;
};

const DIRECT_OWNER: AttributeOwner = { priority: 2, source: 'direct' };
const STYLE_OWNER: AttributeOwner = { priority: 4, source: 'style' };
const COMPOSITE_OWNER: AttributeOwner = { priority: 2, source: 'composite' };

class StoredAttribute {
  constructor(
    readonly elementAttribute: ElementAttribute | undefined,
    readonly invalidatesLayout: boolean,
  ) {
    this.lastAppliedValue = undefined;
  }

  private singleOwner?: AttributeOwner;
  private singleValue: unknown = undefined;
  private values?: OwnerValue[];
  animation: AttributeAnimation | undefined;
  lastAppliedValue: unknown;

  getResolvedValue(): unknown {
    if (this.values) {
      const resolved = this.resolveValueFromCollection();
      return resolved ? resolved.value : undefined;
    }
    return this.singleOwner ? this.singleValue : undefined;
  }

  setValue(owner: AttributeOwner, value: unknown): boolean {
    const oldResolvedValue = this.getResolvedValue();
    if (this.values) {
      const existing = this.values.find(entry => entry.owner === owner);
      if (existing) {
        if (existing.value === value) {
          return false;
        }
        existing.value = value;
      } else {
        this.values.push({ owner, value });
      }
    } else if (this.singleOwner) {
      if (this.singleOwner === owner) {
        if (this.singleValue === value) {
          return false;
        }
        this.singleValue = value;
      } else {
        this.values = [
          { owner: this.singleOwner!, value: this.singleValue },
          { owner, value },
        ];
        this.clearSingleValue();
      }
    } else {
      this.singleOwner = owner;
      this.singleValue = value;
    }
    return oldResolvedValue !== this.getResolvedValue();
  }

  removeValue(owner: AttributeOwner): boolean {
    const oldResolvedValue = this.getResolvedValue();
    if (this.values) {
      const index = this.values.findIndex(entry => entry.owner === owner);
      if (index < 0) {
        return false;
      }
      this.values.splice(index, 1);
      if (this.values.length === 1) {
        const single = this.values[0];
        this.singleOwner = single.owner;
        this.singleValue = single.value;
        this.values = undefined;
      }
    } else if (this.singleOwner === owner) {
      this.clearSingleValue();
    } else {
      return false;
    }
    return oldResolvedValue !== this.getResolvedValue();
  }

  empty(): boolean {
    return !this.singleOwner && (!this.values || this.values.length === 0);
  }

  private clearSingleValue(): void {
    this.singleOwner = undefined;
    this.singleValue = undefined;
  }

  private resolveValueFromCollection(): OwnerValue | undefined {
    if (!this.values || this.values.length === 0) {
      return undefined;
    }
    let best = this.values[0];
    for (let i = 1; i < this.values.length; i++) {
      const candidate = this.values[i];
      if (candidate.owner.priority < best.owner.priority) {
        best = candidate;
      }
    }
    return best;
  }
}

export class AttributesApplier {
  private readonly attributes = new IndexedRecord<StoredAttribute>();
  private readonly dirtyAttributes = new IndexedRecord<string>();
  private dirtyComposites?: IndexedRecord<CompositeAttribute>;
  private currentStyle?: StyleLike;
  private currentStyleAttributeNames?: string[];

  constructor(
    private readonly id: number,
    private readonly elementClass: AnyElementClass,
  ) {}

  setAttribute(attributeName: string, value: unknown): AttributeSetResult {
    const actualAttributeName = attributeName.startsWith('$') ? attributeName.substring(1) : attributeName;
    if (actualAttributeName === 'style') {
      return this.setStyle(value as Style<any> | undefined);
    }

    const changedAttribute = this.updateAttributeForOwner(actualAttributeName, DIRECT_OWNER, value);
    if (!changedAttribute) {
      return AttributeSetResult.Unchanged;
    }
    this.markAttributeDirty(actualAttributeName);
    if (changedAttribute.invalidatesLayout) {
      return AttributeSetResult.ChangedAndInvalidatesLayout;
    }
    return AttributeSetResult.Changed;
  }

  setAttributeForOwner(attributeName: string, owner: AttributeOwner, value: unknown): AttributeSetResult {
    const changedAttribute = this.updateAttributeForOwner(attributeName, owner, value);
    if (!changedAttribute) {
      return AttributeSetResult.Unchanged;
    }
    this.markAttributeDirty(attributeName);
    if (changedAttribute.invalidatesLayout) {
      return AttributeSetResult.ChangedAndInvalidatesLayout;
    }
    return AttributeSetResult.Changed;
  }

  flush(element: HTMLElement, context: AttributeApplierContext, animator: Animator | undefined): void {
    while (!this.dirtyAttributes.empty) {
      const attributeName = this.dirtyAttributes.pop();
      if (attributeName === undefined) {
        continue;
      }
      const attribute = this.attributes.get(attributeName);
      if (!attribute) {
        this.logMissingStoredAttribute(attributeName);
        continue;
      }
      const elementAttribute = attribute.elementAttribute;
      if (elementAttribute?.composite) {
        this.markCompositeDirty(elementAttribute.composite!);
      } else {
        const resolvedAnimator = animator && context.isAnimationEnabled() ? animator : undefined;
        this.applyAttribute(attributeName, attribute, element, context, resolvedAnimator);
      }
    }

    const dirtyComposites = this.dirtyComposites;
    if (dirtyComposites) {
      while (!dirtyComposites.empty) {
        const composite = dirtyComposites.pop();
        if (composite === undefined) {
          continue;
        }
        const resolvedAnimator = animator && context.isAnimationEnabled() ? animator : undefined;
        this.updateCompositeAttribute(composite, element, context, resolvedAnimator);
      }
    }
  }

  getAttribute(attributeName: string): unknown {
    return this.attributes.get(attributeName)?.getResolvedValue();
  }

  getDebugAttributes(): Record<string, unknown> {
    const debugAttributes: Record<string, unknown> = {};
    const attributeNames = this.attributes.keys;
    for (let i = 0; i < attributeNames.length; i++) {
      const attributeName = attributeNames[i];
      const attribute = this.attributes.get(attributeName);
      if (attribute?.elementAttribute?.composite && !attribute.elementAttribute.isCompositePart) {
        continue;
      }
      const value = attribute?.getResolvedValue();
      if (value !== undefined) {
        debugAttributes[attributeName] = toDebugValue(value, 0, new Set<object>());
      }
    }
    return debugAttributes;
  }

  cancelAnimations(): void {
    for (const attributeName of this.attributes.keys) {
      this.attributes.get(attributeName)?.animation?.cancel();
    }
  }

  completeAnimations(): void {
    for (const attributeName of this.attributes.keys) {
      this.attributes.get(attributeName)?.animation?.complete();
    }
  }

  markColorDependentAttributesDirty(): boolean {
    let shouldFlush = false;
    const attributeNames = this.attributes.keys;
    for (let i = 0; i < attributeNames.length; i++) {
      const attributeName = attributeNames[i];
      const attribute = this.attributes.get(attributeName);
      if (!attribute || attribute.empty()) {
        continue;
      }
      const elementAttribute = attribute.elementAttribute;
      const composite = elementAttribute?.composite;
      if (elementAttribute?.isCompositePart && composite?.colorDependent) {
        this.markCompositeDirty(composite);
        shouldFlush = true;
      } else if (elementAttribute?.applier?.colorDependent) {
        this.markAttributeDirty(attributeName);
        shouldFlush = true;
      }
    }
    return shouldFlush;
  }

  private setStyle(value: Style<any> | undefined | null): AttributeSetResult {
    if (value === undefined || value === null) {
      return this.clearCurrentStyle();
    }
    const style = value as StyleLike;
    const attributes = style.attributes;
    if (!attributes || typeof attributes !== 'object') {
      return this.clearCurrentStyle();
    }
    if (this.currentStyle === style) {
      return AttributeSetResult.Unchanged;
    }

    let result = this.clearCurrentStyle();
    const attributeNames = Object.keys(attributes);
    this.currentStyle = style;
    this.currentStyleAttributeNames = attributeNames;
    for (let i = 0; i < attributeNames.length; i++) {
      const attributeName = attributeNames[i];
      const changedAttribute = this.updateAttributeForOwner(attributeName, STYLE_OWNER, attributes[attributeName]);
      if (changedAttribute) {
        this.markAttributeDirty(attributeName);
        if (changedAttribute.invalidatesLayout) {
          result = AttributeSetResult.ChangedAndInvalidatesLayout;
        } else if (result === AttributeSetResult.Unchanged) {
          result = AttributeSetResult.Changed;
        }
      }
    }
    return result;
  }

  private clearCurrentStyle(): AttributeSetResult {
    const attributeNames = this.currentStyleAttributeNames;
    this.currentStyle = undefined;
    this.currentStyleAttributeNames = undefined;
    if (!attributeNames) {
      return AttributeSetResult.Unchanged;
    }
    let result = AttributeSetResult.Unchanged;
    for (let i = 0; i < attributeNames.length; i++) {
      const attributeName = attributeNames[i];
      const changedAttribute = this.removeAttributeForOwner(attributeName, STYLE_OWNER);
      if (changedAttribute) {
        this.markAttributeDirty(attributeName);
        if (changedAttribute.invalidatesLayout) {
          result = AttributeSetResult.ChangedAndInvalidatesLayout;
        } else if (result === AttributeSetResult.Unchanged) {
          result = AttributeSetResult.Changed;
        }
      }
    }
    return result;
  }

  private updateAttributeForOwner(
    attributeName: string,
    owner: AttributeOwner,
    value: unknown,
  ): StoredAttribute | undefined {
    if (value === undefined || value === null) {
      return this.removeAttributeForOwner(attributeName, owner);
    }
    let attribute = this.attributes.get(attributeName);
    if (!attribute) {
      attribute = this.createStoredAttribute(attributeName);
      this.attributes.set(attributeName, attribute);
    }
    return attribute.setValue(owner, value) ? attribute : undefined;
  }

  private removeAttributeForOwner(attributeName: string, owner: AttributeOwner): StoredAttribute | undefined {
    const attribute = this.attributes.get(attributeName);
    if (!attribute) {
      return undefined;
    }
    return attribute.removeValue(owner) ? attribute : undefined;
  }

  private createStoredAttribute(attributeName: string): StoredAttribute {
    const elementAttribute = this.elementClass.elementAttributes[attributeName];
    if (!elementAttribute) {
      return new StoredAttribute(undefined, !!this.elementClass.unknownAttributeApplier?.layoutDependent);
    }
    return new StoredAttribute(
      elementAttribute,
      !!(elementAttribute.applier?.layoutDependent || elementAttribute.composite?.layoutDependent),
    );
  }

  private applyAttribute(
    attributeName: string,
    attribute: StoredAttribute,
    element: HTMLElement,
    context: AttributeApplierContext,
    animator: Animator | undefined,
  ): void {
    const applier = attribute.elementAttribute?.applier;
    const value = attribute.getResolvedValue();
    if (animator && attribute.invalidatesLayout) {
      animator.willApplyLayoutMutation();
    }
    if (!applier) {
      const unknownApplier = this.elementClass.unknownAttributeApplier;
      if (unknownApplier) {
        try {
          if (value === undefined || value === null) {
            unknownApplier.reset(element, attributeName, context);
          } else {
            unknownApplier.apply(element, value, attributeName, context);
          }
        } catch (error) {
          this.logApplyError(attributeName, value, error);
        }
        return;
      }
      this.logMissingAttribute(attributeName, value);
      return;
    }
    if (applier.makeAnimationInterpolator) {
      if (animator) {
        if (this.startAnimation(applier, attribute, attributeName, value, element, context, animator)) {
          attribute.lastAppliedValue = value;
          return;
        }
      } else {
        this.cancelAnimationIfNeeded(attribute);
      }
    }
    try {
      if (value === undefined || value === null) {
        applier.reset(element, attributeName, context);
      } else {
        applier.apply(element, value, attributeName, context);
      }
    } catch (error) {
      this.logApplyError(attributeName, value, error);
      return;
    }
    attribute.lastAppliedValue = value;
  }

  private updateCompositeAttribute(
    composite: CompositeAttribute,
    element: HTMLElement,
    context: AttributeApplierContext,
    animator: Animator | undefined,
  ): void {
    const values: unknown[] = [];
    let hasValue = false;
    for (const part of composite.parts) {
      const value = this.getAttribute(part.name);
      if ((value === undefined || value === null) && !part.optional) {
        this.logApplyError(composite.name, value, new Error(`Composite attribute is missing '${part.name}'`));
        return;
      }
      if (value !== undefined && value !== null) {
        hasValue = true;
      }
      try {
        values.push(
          value === undefined || value === null || !part.parse ? value : part.parse(element, value, part.name, context),
        );
      } catch (error) {
        this.logApplyError(part.name, value, error);
        return;
      }
    }

    let attribute = this.attributes.get(composite.name);
    if (!attribute) {
      attribute = this.createStoredAttribute(composite.name);
      this.attributes.set(composite.name, attribute);
    }
    if (hasValue) {
      attribute.setValue(COMPOSITE_OWNER, values);
    } else {
      attribute.removeValue(COMPOSITE_OWNER);
    }
    this.applyComposite(composite, attribute, element, context, animator);
  }

  private applyComposite(
    composite: CompositeAttribute,
    attribute: StoredAttribute,
    element: HTMLElement,
    context: AttributeApplierContext,
    animator: Animator | undefined,
  ): void {
    const value = attribute.getResolvedValue();
    if (composite.makeAnimationInterpolator) {
      if (animator) {
        if (this.startAnimation(composite, attribute, composite.name, value, element, context, animator)) {
          attribute.lastAppliedValue = value;
          return;
        }
      } else {
        this.cancelAnimationIfNeeded(attribute);
      }
    }
    try {
      if (animator && attribute.invalidatesLayout) {
        animator.willApplyLayoutMutation();
      }
      if (value === undefined || value === null) {
        composite.reset(element, composite.name, context);
      } else {
        composite.apply(element, value as ReadonlyArray<unknown>, composite.name, context);
      }
    } catch (error) {
      this.logApplyError(composite.name, value, error);
      return;
    }
    attribute.lastAppliedValue = value;
  }

  private startAnimation(
    definition: AttributeApplier | CompositeAttribute,
    attribute: StoredAttribute,
    key: string,
    value: unknown,
    element: HTMLElement,
    context: AttributeApplierContext,
    animator: Animator,
  ): boolean {
    const previousAnimation = attribute.animation;
    const startValue = previousAnimation
      ? animator.options.beginFromCurrentState
        ? previousAnimation.currentValue
        : previousAnimation.endValue
      : attribute.lastAppliedValue;
    try {
      const interpolator = definition.makeAnimationInterpolator!(element, startValue, value, context);
      if (!interpolator) {
        this.cancelAnimationIfNeeded(attribute);
        return false;
      }
      this.cancelAnimationIfNeeded(attribute);
      const animation = new AttributeAnimation(
        startValue,
        value,
        definition.animationMinimumVisibleChange ?? MIN_VISIBLE_CHANGE_PIXEL,
        interpolator,
        definition,
        key,
        element,
        context,
        finishedAnimation => {
          if (attribute.animation === finishedAnimation) {
            attribute.animation = undefined;
          }
        },
      );
      attribute.animation = animation;
      animator.addAnimation(this, key, animation);
      return true;
    } catch (error) {
      this.logApplyError(key, value, error);
      return true;
    }
  }

  private cancelAnimationIfNeeded(attribute: StoredAttribute): void {
    attribute.animation?.cancel();
  }

  private markAttributeDirty(attributeName: string): void {
    this.dirtyAttributes.set(attributeName, attributeName);
  }

  private markCompositeDirty(composite: CompositeAttribute): void {
    if (!this.dirtyComposites) {
      this.dirtyComposites = new IndexedRecord<CompositeAttribute>();
    }
    this.dirtyComposites.set(composite.name, composite);
  }

  private logMissingAttribute(attributeName: string, value: unknown): void {
    console.warn(
      `Valdi web renderer has no applier for attribute '${attributeName}' on node ${this.id} (${this.elementClass.className}) with value ${stringifyValue(value)}`,
    );
  }

  private logMissingStoredAttribute(attributeName: string): void {
    console.error(
      `Valdi web renderer marked attribute '${attributeName}' dirty on node ${this.id} (${this.elementClass.className}) but no stored attribute exists`,
    );
  }

  private logApplyError(attributeName: string, value: unknown, error: unknown): void {
    const message = error instanceof Error ? error.message : String(error);
    console.error(
      `Valdi web renderer failed to apply attribute '${attributeName}' on node ${this.id} (${this.elementClass.className}) with value ${stringifyValue(value)}: ${message}`,
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

function toDebugValue(value: unknown, depth: number, seen: Set<object>): unknown {
  if (
    value === undefined ||
    value === null ||
    typeof value === 'string' ||
    typeof value === 'number' ||
    typeof value === 'boolean'
  ) {
    return value;
  }
  if (typeof value === 'function') {
    return '[function]';
  }
  if (typeof value !== 'object') {
    return String(value);
  }
  if (value instanceof Uint8Array) {
    return `[Uint8Array ${value.byteLength}]`;
  }
  if (seen.has(value)) {
    return '[circular]';
  }
  if (depth >= 4) {
    return `[${value.constructor?.name || 'object'}]`;
  }

  seen.add(value);
  if (Array.isArray(value)) {
    return value.map(item => toDebugValue(item, depth + 1, seen));
  }

  const debugValue: Record<string, unknown> = {};
  const entries = Object.entries(value);
  const entryCount = Math.min(entries.length, 80);
  for (let i = 0; i < entryCount; i++) {
    const [key, entryValue] = entries[i];
    debugValue[key] = toDebugValue(entryValue, depth + 1, seen);
  }
  if (entries.length > entryCount) {
    debugValue.__truncated__ = `${entries.length - entryCount} more fields`;
  }
  return debugValue;
}
