import { AttributeApplier, ElementClass } from '../core/ElementClass';
import { AttributesBinder } from '../attributes/AttributesBinder';
import { createBorderRadiusAttributeApplier } from '../attributes/BorderRadiusAttribute';
import { assignStyles, createBaseElement } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

interface BlurMaterial {
  blur: number;
  saturate: number;
  backgroundColor: string;
}

const MAX_MATERIAL_SATURATION = 110;

const DEFAULT_MATERIAL: BlurMaterial = {
  blur: 20,
  saturate: 180,
  backgroundColor: 'rgba(255, 255, 255, 0.55)',
};

const MATERIALS: Record<string, BlurMaterial> = {
  extraLight: { blur: 18, saturate: 170, backgroundColor: 'rgba(255, 255, 255, 0.72)' },
  light: DEFAULT_MATERIAL,
  regular: { blur: 22, saturate: 180, backgroundColor: 'rgba(246, 246, 246, 0.36)' },
  prominent: { blur: 28, saturate: 190, backgroundColor: 'rgba(255, 255, 255, 0.58)' },
  dark: { blur: 22, saturate: 150, backgroundColor: 'rgba(28, 28, 30, 0.62)' },
  systemUltraThinMaterial: { blur: 14, saturate: 180, backgroundColor: 'rgba(246, 246, 246, 0.22)' },
  systemThinMaterial: { blur: 18, saturate: 180, backgroundColor: 'rgba(246, 246, 246, 0.30)' },
  systemMaterial: { blur: 22, saturate: 180, backgroundColor: 'rgba(246, 246, 246, 0.38)' },
  systemThickMaterial: { blur: 28, saturate: 185, backgroundColor: 'rgba(246, 246, 246, 0.50)' },
  systemChromeMaterial: { blur: 24, saturate: 190, backgroundColor: 'rgba(246, 246, 246, 0.62)' },
  systemUltraThinMaterialLight: { blur: 14, saturate: 180, backgroundColor: 'rgba(255, 255, 255, 0.32)' },
  systemThinMaterialLight: { blur: 18, saturate: 180, backgroundColor: 'rgba(255, 255, 255, 0.44)' },
  systemMaterialLight: { blur: 22, saturate: 180, backgroundColor: 'rgba(255, 255, 255, 0.54)' },
  systemThickMaterialLight: { blur: 28, saturate: 185, backgroundColor: 'rgba(255, 255, 255, 0.66)' },
  systemChromeMaterialLight: { blur: 24, saturate: 190, backgroundColor: 'rgba(255, 255, 255, 0.78)' },
  systemUltraThinMaterialDark: { blur: 14, saturate: 150, backgroundColor: 'rgba(28, 28, 30, 0.32)' },
  systemThinMaterialDark: { blur: 18, saturate: 150, backgroundColor: 'rgba(28, 28, 30, 0.44)' },
  systemMaterialDark: { blur: 22, saturate: 150, backgroundColor: 'rgba(28, 28, 30, 0.56)' },
  systemThickMaterialDark: { blur: 28, saturate: 150, backgroundColor: 'rgba(28, 28, 30, 0.68)' },
  systemChromeMaterialDark: { blur: 24, saturate: 150, backgroundColor: 'rgba(28, 28, 30, 0.78)' },
};

function applyBlurMaterial(element: HTMLElement, material: BlurMaterial): void {
  const filter = `blur(${material.blur}px) saturate(${Math.min(material.saturate, MAX_MATERIAL_SATURATION)}%)`;
  element.style.backdropFilter = filter;
  element.style.setProperty('-webkit-backdrop-filter', filter);
  element.style.backgroundColor = material.backgroundColor;
}

function blurStyleAttributeApplier(): AttributeApplier {
  return {
    apply(element, value, attributeName) {
      if (typeof value !== 'string') {
        throw new Error(`Expected '${attributeName}' to be a string`);
      }
      applyBlurMaterial(element, MATERIALS[value] ?? DEFAULT_MATERIAL);
    },
    reset(element) {
      applyBlurMaterial(element, DEFAULT_MATERIAL);
    },
  };
}

export class BlurElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    const binder = new AttributesBinder<HTMLElement>();
    binder.bindAttribute('borderRadius', createBorderRadiusAttributeApplier(true));
    binder.bindAttribute('blurStyle', blurStyleAttributeApplier());
    super(
      'blur',
      { ...viewElementClass.attributeAppliers, ...binder.attributeAppliers },
      viewElementClass.compositeAttributes,
    );
  }

  protected onCreateElement(): HTMLElement {
    const element = createBaseElement('div');
    assignStyles(element, {
      overflow: 'hidden',
    });
    applyBlurMaterial(element, DEFAULT_MATERIAL);
    return element;
  }
}
