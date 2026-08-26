export interface AttributeOwner {
  readonly priority: number;
  readonly source: string;
}

const APPEARANCE_OWNER_PRIORITY = 1000;
const NATIVE_OVERRIDE_OWNER_PRIORITY = 0;

let appearanceAttributeOwner: AttributeOwner | undefined;
let nativeOverrideAttributeOwner: AttributeOwner | undefined;

export function getAppearanceAttributeOwner(): AttributeOwner {
  return (appearanceAttributeOwner ??= {
    priority: APPEARANCE_OWNER_PRIORITY,
    source: 'appearance',
  });
}

export function getNativeOverrideAttributeOwner(): AttributeOwner {
  return (nativeOverrideAttributeOwner ??= {
    priority: NATIVE_OVERRIDE_OWNER_PRIORITY,
    source: 'nativeOverride',
  });
}
