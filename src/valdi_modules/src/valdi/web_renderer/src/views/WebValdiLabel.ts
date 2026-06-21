import { isAttributedText, renderAttributedText } from '../utils/parseAttributedText';
import { applyFontString, applyTextDecoration, cssLength, textShadowCssValue } from '../utils/textStyle';
import { WebValdiLayout } from './WebValdiLayout';

export class WebValdiLabel extends WebValdiLayout {
  public type = 'label';
  private _lineHeight?: string;
  private _lineHeightMultiple?: number;

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

  private updateLineHeight() {
    if (this._lineHeight !== undefined) {
      this.htmlElement.style.lineHeight = this._lineHeight;
    } else if (this._lineHeightMultiple !== undefined) {
      this.htmlElement.style.lineHeight = String(this._lineHeightMultiple);
    } else {
      this.htmlElement.style.lineHeight = '';
    }
  }

  changeAttribute(attributeName: string, attributeValue: any): void {
    switch (attributeName) {
      case 'value':
        if (isAttributedText(attributeValue)) {
          this.htmlElement.replaceChildren(
            renderAttributedText(attributeValue, {
              getInlineChild: index => this.children[index]?.htmlElement,
            }),
          );
        } else {
          this.htmlElement.textContent = attributeValue;
        }
        return;
      case 'font':
        applyFontString(this.htmlElement, String(attributeValue));
        return;
      case 'lineHeight':
        this._lineHeight =
          attributeValue === undefined || attributeValue === null ? undefined : cssLength(attributeValue);
        this.updateLineHeight();
        return;
      case 'lineHeightMultiple':
        this._lineHeightMultiple =
          attributeValue === undefined || attributeValue === null ? undefined : Number(attributeValue);
        this.updateLineHeight();
        return;
      case 'textAlign':
        this.htmlElement.style.textAlign = attributeValue === 'justified' ? 'justify' : attributeValue;
        return;
      case 'textDecoration':
        applyTextDecoration(this.htmlElement, attributeValue);
        return;
      case 'textShadow':
        this.htmlElement.style.textShadow = textShadowCssValue(attributeValue) ?? '';
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
