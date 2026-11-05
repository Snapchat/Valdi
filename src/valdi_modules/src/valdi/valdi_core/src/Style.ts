import { IStyle } from 'valdi_tsx/src/IStyle';
import { ConsoleRepresentable } from './ConsoleRepresentable';
import { PropertyList } from './utils/PropertyList';

export type NativeStyle = number;

export type StyleToNativeFunc = (attributes: PropertyList) => NativeStyle;

export type StyleValue<T> = Style<T> | ReadonlyArray<Style<T>>;

/**
 * A Style object holds a bag of attributes together and can
 * serialize them inside a native single object to avoid marshalling.
 * Using Style will be more efficient than providing all attributes
 * one by one in JSX. You should typically create your Style objects
 * only once in your JS module and re-use it.
 */
export class Style<T> implements IStyle<T>, ConsoleRepresentable {
  readonly attributes: Omit<T, 'style'>;
  private native: NativeStyle | undefined;

  constructor(attributes: T) {
    this.attributes = attributes;
    this.native = undefined;
  }

  toNative(convertFunc: StyleToNativeFunc): NativeStyle {
    let native = this.native;
    if (native === undefined) {
      native = convertFunc(this.attributes);
      this.native = native;
    }

    return native;
  }

  /**
   * Creates a new Style object by merging the given attributes
   * with the attributes from this style object.
   * @param attributes
   */
  extend<T2>(attributes: T2): Style<Omit<T, 'style'> & T2> {
    if (attributes instanceof Style) {
      return this.extend(attributes.attributes);
    }
    const newAttributes = { ...this.attributes, ...attributes };
    return new Style(newAttributes);
  }

  toConsoleRepresentation(): any {
    return this.attributes;
  }

  /**
   * Creates a new Style object by merging the given Style
   * with this style object.
   * @param attributes
   */
  // No variadic template parameters support means we cannot make this type safe:
  // https://github.com/microsoft/TypeScript/issues/5453
  static merge<S1, S2>(style1: Style<S1>, style2: Style<S2>): Style<S1 & S2>;
  static merge<S1, S2, S3>(styles1: Style<S1>, style2: Style<S2>, style3: Style<S3>): Style<S1 & S2 & S3>;
  static merge<S1, S2, S3, S4>(
    style1: Style<S1>,
    style2: Style<S2>,
    style3: Style<S3>,
    style4: Style<S4>,
  ): Style<S1 & S2 & S3 & S4>;
  static merge(...styles: Style<any>[]): Style<any> {
    const attributes = styles.reduce((c, style) => {
      return { ...c, ...style.attributes };
    }, {});
    return new Style(attributes);
  }
}
