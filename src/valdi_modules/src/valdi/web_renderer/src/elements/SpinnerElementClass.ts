import { parseCssLength } from '../attributes/AttributeApplierHelpers';
import { AttributesBinder, MIN_VISIBLE_CHANGE_COLOR } from '../attributes/AttributesBinder';
import { ElementClass } from '../core/ElementClass';
import { assignStyles, AttributeApplierMap, createBaseElement } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

const SVG_NS = 'http://www.w3.org/2000/svg';

function getSpinnerSvg(element: HTMLElement): SVGElement | null {
  return element.querySelector('svg');
}

function buildSpinnerAttributeAppliers(viewElementClass: ViewElementClass): AttributeApplierMap {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindAnimatableColorAttribute(
    'color',
    'currentColor',
    MIN_VISIBLE_CHANGE_COLOR,
    (element, color) => {
      element.style.color = color;
      const svg = getSpinnerSvg(element);
      if (svg) {
        svg.style.color = color;
      }
    },
    element => {
      element.style.color = '';
      const svg = getSpinnerSvg(element);
      if (svg) {
        svg.style.color = 'currentColor';
      }
    },
  );
  binder.bindAttribute('width', {
    layoutDependent: true,
    apply(element, value, attributeName) {
      const width = parseCssLength(value, attributeName);
      element.style.width = width;
      const svg = getSpinnerSvg(element);
      if (svg) {
        svg.style.width = width;
      }
    },
    reset(element) {
      element.style.width = '';
      const svg = getSpinnerSvg(element);
      if (svg) {
        svg.style.width = '20px';
      }
    },
  });
  binder.bindAttribute('height', {
    layoutDependent: true,
    apply(element, value, attributeName) {
      const height = parseCssLength(value, attributeName);
      element.style.height = height;
      const svg = getSpinnerSvg(element);
      if (svg) {
        svg.style.height = height;
      }
    },
    reset(element) {
      element.style.height = '';
      const svg = getSpinnerSvg(element);
      if (svg) {
        svg.style.height = '20px';
      }
    },
  });
  return { ...viewElementClass.attributeAppliers, ...binder.attributeAppliers };
}

export class SpinnerElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    super('spinner', buildSpinnerAttributeAppliers(viewElementClass), viewElementClass.compositeAttributes);
  }

  protected onCreateElement(): HTMLElement {
    const element = createBaseElement('div');
    assignStyles(element, {
      alignItems: 'center',
      justifyContent: 'center',
      pointerEvents: 'none',
    });

    const svg = document.createElementNS(SVG_NS, 'svg');
    svg.classList.add('valdi-spinner');
    svg.setAttribute('viewBox', '0 0 12 12');
    svg.setAttribute('role', 'status');
    Object.assign(svg.style, {
      color: 'currentColor',
      filter: 'drop-shadow(0 0 4px rgba(0, 0, 0, 0.35))',
      height: '20px',
      overflow: 'visible',
      width: '20px',
    });

    const outerCircle = document.createElementNS(SVG_NS, 'circle');
    outerCircle.classList.add('valdi-spinner-outer');
    outerCircle.setAttribute('cx', '6');
    outerCircle.setAttribute('cy', '6');
    outerCircle.setAttribute('r', '5');
    outerCircle.setAttribute('fill', 'none');
    outerCircle.setAttribute('stroke', 'currentColor');
    outerCircle.setAttribute('stroke-linecap', 'round');
    outerCircle.setAttribute('stroke-width', '1');
    outerCircle.setAttribute('stroke-dasharray', '31.416');
    Object.assign(outerCircle.style, {
      animation:
        'valdi-spin-cw 1s linear infinite, valdi-dash-outer 0.9s ease-out forwards, valdi-grow 0.5s ease-out forwards',
      transformBox: 'fill-box',
      transformOrigin: 'center',
    });

    const innerCircle = document.createElementNS(SVG_NS, 'circle');
    innerCircle.classList.add('valdi-spinner-inner');
    innerCircle.setAttribute('cx', '6');
    innerCircle.setAttribute('cy', '6');
    innerCircle.setAttribute('r', '3');
    innerCircle.setAttribute('fill', 'none');
    innerCircle.setAttribute('stroke', 'currentColor');
    innerCircle.setAttribute('stroke-linecap', 'round');
    innerCircle.setAttribute('stroke-width', '1');
    innerCircle.setAttribute('stroke-dasharray', '18.85');
    Object.assign(innerCircle.style, {
      animation:
        'valdi-spin-ccw 1s linear infinite, valdi-dash-inner 0.9s ease-out forwards, valdi-grow 0.5s ease-out forwards',
      transformBox: 'fill-box',
      transformOrigin: 'center',
    });

    svg.appendChild(outerCircle);
    svg.appendChild(innerCircle);

    const style = document.createElement('style');
    style.textContent = `
      @keyframes valdi-spin-cw { to { transform: rotate(360deg); } }
      @keyframes valdi-spin-ccw { to { transform: rotate(-360deg); } }
      @keyframes valdi-dash-outer { from { stroke-dashoffset: 31.416; } to { stroke-dashoffset: 12.566; } }
      @keyframes valdi-dash-inner { from { stroke-dashoffset: 18.85; } to { stroke-dashoffset: 7.54; } }
      @keyframes valdi-grow { from { stroke-width: 0; } to { stroke-width: 1; } }
    `;

    element.appendChild(style);
    element.appendChild(svg);
    return element;
  }
}
