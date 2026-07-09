import { AnimatedImage, GlassView, ImageView, Label, Layout, ScrollView, ShapeView, SpinnerView, TextField, TextView, View, WebViewElement } from "valdi_tsx/src/NativeTemplateElements";
import { IRenderedElementViewClass } from "./IRenderedElementViewClass";

type Mapping = {
    [IRenderedElementViewClass.View]: View;
    [IRenderedElementViewClass.Layout]: Layout;
    [IRenderedElementViewClass.Label]: Label;
    [IRenderedElementViewClass.Image]: ImageView;
    [IRenderedElementViewClass.WebView]: WebViewElement;
    [IRenderedElementViewClass.Spinner]: SpinnerView;
    [IRenderedElementViewClass.TextField]: TextField;
    [IRenderedElementViewClass.TextView]: TextView;
    [IRenderedElementViewClass.ScrollView]: ScrollView;
    [IRenderedElementViewClass.Shape]: ShapeView;
    [IRenderedElementViewClass.AnimatedImage]: AnimatedImage;
    [IRenderedElementViewClass.Glass]: GlassView;
}

export type ElementForViewClass<T extends IRenderedElementViewClass> = Mapping[T];
