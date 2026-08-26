import {
  AttributesBinder,
  MIN_VISIBLE_CHANGE_COLOR,
  MIN_VISIBLE_CHANGE_PIXEL,
} from '../attributes/AttributesBinder';
import { AttributeApplierContext, ElementClass } from '../core/ElementClass';
import { geometricPathToSvgPath, isGeometricPathValue } from '../utils/geometricPath';
import { assignStyles, AttributeApplierMap } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

const SVG_NS = 'http://www.w3.org/2000/svg';
const SHAPE_STATE = '__shapeElementClassState';

interface ShapeState {
  strokeStart: number;
  strokeEnd: number;
}

function getShapeState(context: AttributeApplierContext): ShapeState {
  const existing = context.getState<ShapeState>(SHAPE_STATE);
  if (existing) {
    return existing;
  }
  const state: ShapeState = { strokeStart: 0, strokeEnd: 1 };
  context.setState(SHAPE_STATE, state);
  return state;
}

function getSvgElement(element: HTMLElement): SVGSVGElement {
  return element.querySelector('svg') as SVGSVGElement;
}

function getPathElement(element: HTMLElement): SVGPathElement {
  return element.querySelector('path') as SVGPathElement;
}

function pathToString(element: HTMLElement, path: unknown): string | undefined {
  if (typeof path === 'string') {
    return path;
  }
  if (isGeometricPathValue(path)) {
    const result = geometricPathToSvgPath(path);
    getSvgElement(element).setAttribute('viewBox', result.viewBox);
    getSvgElement(element).setAttribute('preserveAspectRatio', result.preserveAspectRatio);
    return result.d || undefined;
  }
  return undefined;
}

function applyStrokeDash(element: HTMLElement, context: AttributeApplierContext): void {
  const path = getPathElement(element);
  const state = getShapeState(context);
  if (state.strokeStart <= 0 && state.strokeEnd >= 1) {
    path.removeAttribute('stroke-dasharray');
    path.removeAttribute('stroke-dashoffset');
    return;
  }
  const total = path.getTotalLength();
  if (total <= 0) {
    path.removeAttribute('stroke-dasharray');
    path.removeAttribute('stroke-dashoffset');
    return;
  }
  const start = state.strokeStart * total;
  const length = (state.strokeEnd - state.strokeStart) * total;
  path.setAttribute('stroke-dasharray', `${length} ${total}`);
  path.setAttribute('stroke-dashoffset', String(-start));
}

function buildShapeAttributeAppliers(viewElementClass: ViewElementClass): AttributeApplierMap {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindAttribute('path', {
    apply(element, value, _attributeName, context) {
      const path = getPathElement(element);
      const d = pathToString(element, value);
      if (d !== undefined) {
        path.setAttribute('d', d);
        applyStrokeDash(element, context);
      } else {
        path.removeAttribute('d');
      }
    },
    reset(element) {
      getPathElement(element).removeAttribute('d');
    },
  });
  binder.bindAnimatableNumberAttribute(
    'strokeWidth',
    1,
    MIN_VISIBLE_CHANGE_PIXEL,
    (element, value) => {
      getPathElement(element).setAttribute('stroke-width', String(value));
    },
    element => {
      getPathElement(element).removeAttribute('stroke-width');
    },
  );
  binder.bindAnimatableColorAttribute(
    'strokeColor',
    'transparent',
    MIN_VISIBLE_CHANGE_COLOR,
    (element, value) => {
      getPathElement(element).setAttribute('stroke', value);
    },
    element => {
      getPathElement(element).setAttribute('stroke', 'transparent');
    },
  );
  binder.bindAnimatableColorAttribute(
    'fillColor',
    'transparent',
    MIN_VISIBLE_CHANGE_COLOR,
    (element, value) => {
      getPathElement(element).setAttribute('fill', value);
    },
    element => {
      getPathElement(element).setAttribute('fill', 'transparent');
    },
  );
  binder.bindStringAttribute(
    'strokeCap',
    (element, value) => {
      getPathElement(element).setAttribute('stroke-linecap', value);
    },
    element => {
      getPathElement(element).setAttribute('stroke-linecap', 'butt');
    },
  );
  binder.bindStringAttribute(
    'strokeJoin',
    (element, value) => {
      getPathElement(element).setAttribute('stroke-linejoin', value);
    },
    element => {
      getPathElement(element).setAttribute('stroke-linejoin', 'miter');
    },
  );
  binder.bindAnimatableNumberAttribute(
    'strokeStart',
    0,
    MIN_VISIBLE_CHANGE_PIXEL,
    (element, value, context) => {
      getShapeState(context).strokeStart = value;
      applyStrokeDash(element, context);
    },
    (element, context) => {
      getShapeState(context).strokeStart = 0;
      applyStrokeDash(element, context);
    },
  );
  binder.bindAnimatableNumberAttribute(
    'strokeEnd',
    1,
    MIN_VISIBLE_CHANGE_PIXEL,
    (element, value, context) => {
      getShapeState(context).strokeEnd = value;
      applyStrokeDash(element, context);
    },
    (element, context) => {
      getShapeState(context).strokeEnd = 1;
      applyStrokeDash(element, context);
    },
  );
  return { ...viewElementClass.attributeAppliers, ...binder.attributeAppliers };
}

export class ShapeElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    super('shape', buildShapeAttributeAppliers(viewElementClass), viewElementClass.compositeAttributes);
  }

  protected onCreateElement(): HTMLElement {
    const path = document.createElementNS(SVG_NS, 'path') as SVGPathElement;
    path.setAttribute('fill', 'none');
    path.setAttribute('vector-effect', 'non-scaling-stroke');

    const wrapper = document.createElement('div');
    assignStyles(wrapper, {
      display: 'block',
      height: '100%',
      width: '100%',
    });

    const svg = document.createElementNS(SVG_NS, 'svg') as SVGSVGElement;
    svg.setAttribute('viewBox', '0 0 1 1');
    svg.setAttribute('preserveAspectRatio', 'none');
    assignStyles(svg as unknown as HTMLElement, {
      display: 'block',
      height: '100%',
      width: '100%',
    });
    svg.appendChild(path);
    wrapper.appendChild(svg);
    return wrapper;
  }
}
