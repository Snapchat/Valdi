import { isAttributedText, renderAttributedText } from '../utils/parseAttributedText';
import { WebValdiLayout } from './WebValdiLayout';

export class WebValdiLabel extends WebValdiLayout {
  public type = 'label';

  createHtmlElement() {
    const element = document.createElement('span');

    Object.assign(element.style, {
      backgroundColor: 'transparent',
      border: '0 solid black',
      boxSizing: 'border-box',
      color: 'black',
      display: 'inline',
      listStyle: 'none',
      margin: 0,
      padding: 0,
      position: 'relative',
      textAlign: 'start',
      textDecoration: 'none',
      whiteSpace: 'pre-wrap',
      wordWrap: 'break-word',
      fontFamily: 'sans-serif',
      font: 'AvenirNext-DemiBold',
      pointerEvents: 'auto',
    });

    return element;
  }

  changeAttribute(attributeName: string, attributeValue: any): void {
    switch (attributeName) {
      case 'value':
        if (isAttributedText(attributeValue)) {
          this.htmlElement.textContent = '';
          this.htmlElement.appendChild(renderAttributedText(attributeValue));
        } else {
          this.htmlElement.textContent = attributeValue;
        }
        return;
      case 'numberOfLines':
        // Not yet implemented
        // If this falls through to the super call it'll break things.
        return;
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}
