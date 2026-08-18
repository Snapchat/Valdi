import { AnyElementClass } from '../core/ElementClass';
import { BlurElementClass } from './BlurElementClass';
import { CustomViewElementClass } from './CustomViewElementClass';
import { DatePickerElementClass } from './DatePickerElementClass';
import { GlassElementClass } from './GlassElementClass';
import { ImageElementClass } from './ImageElementClass';
import { LabelElementClass } from './LabelElementClass';
import { LayoutElementClass } from './LayoutElementClass';
import { ScrollElementClass } from './ScrollElementClass';
import { ShapeElementClass } from './ShapeElementClass';
import { SpinnerElementClass } from './SpinnerElementClass';
import { TextAnimationGroupElementClass } from './TextAnimationGroupElementClass';
import { TextFieldElementClass } from './TextFieldElementClass';
import { TextViewElementClass } from './TextViewElementClass';
import { VideoElementClass } from './VideoElementClass';
import { ViewElementClass } from './ViewElementClass';
import { WebViewElementClass } from './WebViewElementClass';

const layoutClass = new LayoutElementClass('layout', {}, {});
const viewClass = new ViewElementClass('view', {}, {});
const labelClass = new LabelElementClass(viewClass);
const scrollClass = new ScrollElementClass(viewClass);
const imageClass = new ImageElementClass(viewClass);
const textFieldClass = new TextFieldElementClass(labelClass);
const textViewClass = new TextViewElementClass(textFieldClass);
const videoClass = new VideoElementClass(viewClass);
const spinnerClass = new SpinnerElementClass(viewClass);
const textAnimationGroupClass = new TextAnimationGroupElementClass(layoutClass);
const customViewClass = new CustomViewElementClass(viewClass);
const shapeClass = new ShapeElementClass(viewClass);
const blurClass = new BlurElementClass(viewClass);
const glassClass = new GlassElementClass(viewClass);
const webViewClass = new WebViewElementClass(viewClass);
const datePickerClass = new DatePickerElementClass(viewClass);

const elementClassesByName = new Map<string, AnyElementClass>([
  ['layout', layoutClass],
  ['Layout', layoutClass],
  ['view', viewClass],
  ['SCValdiView', viewClass],
  ['textanimationgroup', textAnimationGroupClass],
  ['SCValdiTextAnimationGroup', textAnimationGroupClass],
  ['label', labelClass],
  ['SCValdiLabel', labelClass],
  ['scroll', scrollClass],
  ['SCValdiScrollView', scrollClass],
  ['image', imageClass],
  ['animatedimage', imageClass],
  ['SCValdiAnimatedContentView', imageClass],
  ['SCValdiImageView', imageClass],
  ['textfield', textFieldClass],
  ['SCValdiTextField', textFieldClass],
  ['textview', textViewClass],
  ['SCValdiTextView', textViewClass],
  ['video', videoClass],
  ['SCValdiVideoView', videoClass],
  ['spinner', spinnerClass],
  ['SCValdiSpinnerView', spinnerClass],
  ['custom-view', customViewClass],
  ['shape', shapeClass],
  ['SCValdiShapeView', shapeClass],
  ['blur', blurClass],
  ['SCValdiBlurView', blurClass],
  ['glass', glassClass],
  ['SCValdiGlassView', glassClass],
  ['webview', webViewClass],
  ['SCValdiWebView', webViewClass],
  ['SCValdiDatePicker', datePickerClass],
]);

export function getElementClassForViewClass(viewClassName: string): AnyElementClass | undefined {
  return elementClassesByName.get(viewClassName);
}

export function registerElementClassAlias(alias: string, elementClass: AnyElementClass): void {
  elementClassesByName.set(alias, elementClass);
}
