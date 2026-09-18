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
      // Native VALDI labels are single-line unless numberOfLines opts into
      // wrapping; pre-wrap makes short labels (e.g. CTA pills) wrap on web.
      whiteSpace: 'nowrap',
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
        if (typeof attributeValue === 'number' && attributeValue > 0) {
          Object.assign(this.htmlElement.style, {
            display: '-webkit-box',
            WebkitLineClamp: String(attributeValue),
            WebkitBoxOrient: 'vertical',
            overflow: 'hidden',
            whiteSpace: 'pre-wrap',
            wordWrap: 'break-word',
          });
        } else if (typeof attributeValue === 'number') {
          // Native VALDI treats numberOfLines <= 0 as unlimited multiline
          // (TextViewHelper maps <= 0 to Int.MAX_VALUE), so wrap without clamping.
          Object.assign(this.htmlElement.style, {
            display: 'inline',
            WebkitLineClamp: '',
            WebkitBoxOrient: '',
            overflow: 'visible',
            whiteSpace: 'pre-wrap',
            wordWrap: 'break-word',
          });
        } else {
          // Unset: restore the single-line label default.
          Object.assign(this.htmlElement.style, {
            display: 'inline',
            WebkitLineClamp: '',
            WebkitBoxOrient: '',
            overflow: 'visible',
            whiteSpace: 'nowrap',
            wordWrap: '',
          });
        }
        return;
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}
