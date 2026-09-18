import { WebValdiLayout } from './WebValdiLayout';

export class WebValdiSpinner extends WebValdiLayout {
  public type = 'spinner';
  private svgElement: SVGElement | null = null;

  createHtmlElement(): HTMLElement {
    const element = document.createElement('div');

    Object.assign(element.style, {
      alignItems: 'center',
      justifyContent: 'center',
      backgroundColor: 'transparent',
      border: '0 solid black',
      boxSizing: 'border-box',
      display: 'flex',
      flexBasis: 'auto',
      flexDirection: 'column',
      flexShrink: 0,
      listStyle: 'none',
      margin: 0,
      minHeight: 0,
      minWidth: 0,
      padding: 0,
      position: 'relative',
      textDecoration: 'none',
      zIndex: 0,
      pointerEvents: 'none',
      overflow: 'visible',
    });

    this.svgElement = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    this.svgElement.classList.add('valdi-spinner');
    this.svgElement.setAttribute('viewBox', '0 0 12 12');
    this.svgElement.setAttribute('role', 'status');
    
    Object.assign(this.svgElement.style, {
      width: '20px',
      height: '20px',
      color: 'currentColor',
      overflow: 'visible',
      filter: 'drop-shadow(0 0 4px rgba(0, 0, 0, 0.35))',
    });

    const outerCircle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
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
      transformBox: 'fill-box',
      transformOrigin: 'center',
      animation: 'valdi-spin-cw 1s linear infinite, valdi-dash-outer 0.9s ease-out forwards, valdi-grow 0.5s ease-out forwards',
    });

    const innerCircle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
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
      transformBox: 'fill-box',
      transformOrigin: 'center',
      animation: 'valdi-spin-ccw 1s linear infinite, valdi-dash-inner 0.9s ease-out forwards, valdi-grow 0.5s ease-out forwards',
    });

    this.svgElement.appendChild(outerCircle);
    this.svgElement.appendChild(innerCircle);

    const style = document.createElement('style');
    style.textContent = `
      @keyframes valdi-spin-cw {
        to {
          transform: rotate(360deg);
        }
      }

      @keyframes valdi-spin-ccw {
        to {
          transform: rotate(-360deg);
        }
      }

      @keyframes valdi-dash-outer {
        from {
          stroke-dashoffset: 31.416;
        }
        to {
          stroke-dashoffset: 12.566;
        }
      }

      @keyframes valdi-dash-inner {
        from {
          stroke-dashoffset: 18.85;
        }
        to {
          stroke-dashoffset: 7.54;
        }
      }

      @keyframes valdi-grow {
        from {
          stroke-width: 0;
        }
        to {
          stroke-width: 1;
        }
      }
    `;
    
    element.appendChild(style);
    element.appendChild(this.svgElement);
    return element;
  }

  changeAttribute(attributeName: string, attributeValue: any) {
    switch (attributeName) {
      case 'color':
        if (this.svgElement) {
          this.svgElement.style.color = attributeValue;
        }
        return;
      case 'width':
        this.htmlElement.style.width = attributeValue;
        if (this.svgElement) {
          this.svgElement.style.width = attributeValue;
        }
        return;
      case 'height':
        this.htmlElement.style.height = attributeValue;
        if (this.svgElement) {
          this.svgElement.style.height = attributeValue;
        }
        return;
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}

