import { IComponentBase } from './IComponentBase';
import { IRenderedComponentHolder } from './IRenderedComponentHolder';
import {
  AnimatedImage,
  BlurView,
  ContainerTemplateElement,
  ImageView,
  VideoView,
  WebViewElement,
  Label,
  Layout,
  ScrollView,
  ShapeView,
  Slot,
  SpinnerView,
  TextAnimationGroup,
  TextField,
  TextView,
  View,
} from './NativeTemplateElements';
import { ViewFactory } from './ViewFactory';

/* eslint-disable @typescript-eslint/naming-convention */

export interface CustomView extends View {
  iosClass: string;
  androidClass: string;
}

export interface DeferredCustomView extends View {
  viewFactory: ViewFactory;
}

interface Slotted extends ContainerTemplateElement {
  slot?: string;
}

declare global {
  export namespace JSX {
    export interface IntrinsicElements {
      view: View;
      layout: Layout;
      scroll: ScrollView;
      shape: ShapeView;
      label: Label;
      image: ImageView;
      video: VideoView;
      webview: WebViewElement;
      textfield: TextField;
      textview: TextView;
      textanimationgroup: TextAnimationGroup;
      blur: BlurView;
      slot: Slot;
      slotted: Slotted;
      spinner: SpinnerView;
      animatedimage: AnimatedImage;
      'custom-view': CustomView | DeferredCustomView | { [key: string]: unknown };
    }

    interface ElementChildrenAttribute {
      children?: unknown;
    }

    // IntrinsicAttributes is used to specify attributes available
    // for _all_ JSX elements
    export interface IntrinsicAttributes {
      children?: unknown;
    }

    export interface ElementAttributesProperty {
      viewModel: unknown;
    }

    export interface ElementClass extends IComponentBase {}

    // IntrinsicClassAttributes is used to specify attributes available
    // for class-based JSX elements
    export interface IntrinsicClassAttributes<ComponentT extends IComponentBase> {
      key?: string;
      ref?: IRenderedComponentHolder<ComponentT, unknown>;
      context?: ComponentT['context'];
    }
  }
}
