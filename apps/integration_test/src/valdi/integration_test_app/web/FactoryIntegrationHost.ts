import { createViewFactory } from 'web_renderer/src/ViewFactory';
import { ViewElementClass } from 'web_renderer/src/elements/ViewElementClass';

class IntegrationFactoryElementClass extends ViewElementClass {
  constructor() {
    super(
      'integration-factory-view',
      {
        factoryText: {
          apply(element: HTMLElement, value: unknown): void {
            (element.lastElementChild as HTMLElement).textContent = String(value);
          },
          reset(element: HTMLElement): void {
            (element.lastElementChild as HTMLElement).textContent = '';
          },
        },
      },
      {},
    );
  }

  protected onCreateElement(): HTMLElement {
    const element = super.onCreateElement();
    element.dataset.factoryHost = 'web';
    element.style.alignItems = 'center';
    element.style.flexDirection = 'row';

    const icon = document.createElement('div');
    icon.style.flex = '0 0 68px';
    icon.style.height = '92px';
    icon.style.position = 'relative';

    const hexagon = document.createElement('div');
    hexagon.style.backgroundColor = '#4F46E5';
    hexagon.style.clipPath = 'polygon(50% 0%, 100% 25%, 100% 75%, 50% 100%, 0% 75%, 0% 25%)';
    hexagon.style.height = '52px';
    hexagon.style.left = '22px';
    hexagon.style.position = 'absolute';
    hexagon.style.top = '20px';
    hexagon.style.width = '44px';
    icon.appendChild(hexagon);

    const sparkle = document.createElement('div');
    sparkle.style.backgroundColor = '#FFFFFF';
    sparkle.style.clipPath =
      'polygon(50% 0%, 65.625% 34.375%, 100% 50%, 65.625% 65.625%, 50% 100%, 34.375% 65.625%, 0% 50%, 34.375% 34.375%)';
    sparkle.style.height = '32px';
    sparkle.style.left = '28px';
    sparkle.style.position = 'absolute';
    sparkle.style.top = '30px';
    sparkle.style.width = '32px';
    icon.appendChild(sparkle);

    const label = document.createElement('span');
    label.style.color = '#172554';
    label.style.font = '600 16px system-ui';
    label.style.marginLeft = '14px';

    element.append(icon, label);
    return element;
  }
}

export function createIntegrationViewFactory(): unknown {
  return createViewFactory(new IntegrationFactoryElementClass());
}
