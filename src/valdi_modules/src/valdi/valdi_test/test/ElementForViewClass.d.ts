import { AnimatedImage, ImageView, Label, Layout, ScrollView, ShapeView, SpinnerView, TextField, TextView, View } from "valdi_tsx/src/NativeTemplateElements";
import { IRenderedElementViewClass } from "./IRenderedElementViewClass";

type Mapping = {
    [IRenderedElementViewClass.View]: View;
    [IRenderedElementViewClass.Layout]: Layout;
    [IRenderedElementViewClass.Label]: Label;
    [IRenderedElementViewClass.Image]: ImageView;
    [IRenderedElementViewClass.Spinner]: SpinnerView;
    [IRenderedElementViewClass.TextField]: TextField;
    [IRenderedElementViewClass.TextView]: TextView;
    [IRenderedElementViewClass.ScrollView]: ScrollView;
    [IRenderedElementViewClass.Shape]: ShapeView;
    [IRenderedElementViewClass.AnimatedImage]: AnimatedImage;
}

export type ElementForViewClass<T extends IRenderedElementViewClass> = Mapping[T];