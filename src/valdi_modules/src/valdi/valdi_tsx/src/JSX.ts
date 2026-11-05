import { IComponentBase } from './IComponentBase';
import { IRenderedComponentHolder } from './IRenderedComponentHolder';
import { IStyle } from './IStyle';
import {
  AnimatedImage,
  BlurView,
  ContainerTemplateElement,
  GlassView,
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

// Input-only: element interfaces keep a single IStyle so getAttribute('style') stays typed.
type WithStyleArray<T extends { style?: IStyle<any> }> = Omit<T, 'style'> & {
  style?: T['style'] | ReadonlyArray<NonNullable<T['style']> | undefined>;
};

declare global {
  export namespace JSX {
    export interface IntrinsicElements {
      view: WithStyleArray<View>;
      layout: WithStyleArray<Layout>;
      scroll: WithStyleArray<ScrollView>;
      shape: WithStyleArray<ShapeView>;
      label: WithStyleArray<Label>;
      image: WithStyleArray<ImageView>;
      video: WithStyleArray<VideoView>;
      webview: WithStyleArray<WebViewElement>;
      textfield: WithStyleArray<TextField>;
      textview: WithStyleArray<TextView>;
      textanimationgroup: WithStyleArray<TextAnimationGroup>;
      blur: WithStyleArray<BlurView>;
      glass: WithStyleArray<GlassView>;
      slot: Slot;
      slotted: Slotted;
      spinner: WithStyleArray<SpinnerView>;
      animatedimage: WithStyleArray<AnimatedImage>;
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
