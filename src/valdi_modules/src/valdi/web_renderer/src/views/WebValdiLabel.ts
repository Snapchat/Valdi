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
      font: 'Montserrat-SemiBold',
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
        if (attributeValue && attributeValue > 0) {
          Object.assign(this.htmlElement.style, {
            display: '-webkit-box',
            WebkitLineClamp: String(attributeValue),
            WebkitBoxOrient: 'vertical',
            overflow: 'hidden',
          });
        } else {
          Object.assign(this.htmlElement.style, {
            display: 'inline',
            WebkitLineClamp: '',
            WebkitBoxOrient: '',
            overflow: 'visible',
          });
        }
        return;
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}
