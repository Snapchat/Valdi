export interface SnapshotTestCase {
  name: string;
  width: number;
  height: number;
  render: () => void;
}

import { BasicLayout } from './BasicLayout';
import { TextRendering } from './TextRendering';
import { NestedViews } from './NestedViews';
import { FlexBasisVariants } from './FlexBasisVariants';
import { AbsolutePositioning } from './AbsolutePositioning';
import { CustomMeasure } from './CustomMeasure';
import { MinMaxConstraints } from './MinMaxConstraints';
import { AspectRatioLayout } from './AspectRatioLayout';
import { PercentageSizing } from './PercentageSizing';
import { AlignSelfOverrides } from './AlignSelfOverrides';
import { NegativeMargins } from './NegativeMargins';
import { ReverseDirections } from './ReverseDirections';
import { AlignContentWrap } from './AlignContentWrap';
import { DisplayNone } from './DisplayNone';
import { TextLineHeight } from './TextLineHeight';
import { TextAlignment } from './TextAlignment';
import { TextTruncation } from './TextTruncation';
import { TextDecoration } from './TextDecoration';
import { TextLetterSpacing } from './TextLetterSpacing';
import { TextMultiline } from './TextMultiline';
import { TextShadow } from './TextShadow';
import { TextCombined } from './TextCombined';
import { InlineImageLayout } from './InlineImageLayout';
import { EmojiRendering } from './EmojiRendering';
import { MaskImage } from './MaskImage';
import { TextFieldRendering } from './TextFieldRendering';
import { BorderStyles } from './BorderStyles';
import { TransformTranslate } from './TransformTranslate';
import { TransformScale } from './TransformScale';
import { TransformRotate } from './TransformRotate';
import { TransformComposed } from './TransformComposed';
import { TransformOrigin } from './TransformOrigin';
import { LayoutDirectionRtl } from './LayoutDirectionRtl';
import { TransformDirectionTranslate } from './TransformDirectionTranslate';
import { ColorPaletteThemes, ColorPaletteNestedOverride } from './ColorPalette';

export const testCases: SnapshotTestCase[] = [
  { name: 'BasicLayout', width: 200, height: 150, render: () => { <BasicLayout />; } },
  { name: 'TextRendering', width: 200, height: 100, render: () => { <TextRendering />; } },
  { name: 'NestedViews', width: 200, height: 200, render: () => { <NestedViews />; } },
  { name: 'FlexBasisVariants', width: 200, height: 200, render: () => { <FlexBasisVariants />; } },
  { name: 'AbsolutePositioning', width: 200, height: 200, render: () => { <AbsolutePositioning />; } },
  { name: 'CustomMeasure', width: 200, height: 300, render: () => { <CustomMeasure />; } },
  { name: 'MinMaxConstraints', width: 200, height: 220, render: () => { <MinMaxConstraints />; } },
  { name: 'AspectRatioLayout', width: 200, height: 200, render: () => { <AspectRatioLayout />; } },
  { name: 'PercentageSizing', width: 200, height: 230, render: () => { <PercentageSizing />; } },
  { name: 'AlignSelfOverrides', width: 200, height: 220, render: () => { <AlignSelfOverrides />; } },
  { name: 'NegativeMargins', width: 200, height: 220, render: () => { <NegativeMargins />; } },
  { name: 'ReverseDirections', width: 200, height: 230, render: () => { <ReverseDirections />; } },
  { name: 'AlignContentWrap', width: 200, height: 280, render: () => { <AlignContentWrap />; } },
  { name: 'DisplayNone', width: 200, height: 250, render: () => { <DisplayNone />; } },
  { name: 'TextLineHeight', width: 300, height: 350, render: () => { <TextLineHeight />; } },
  { name: 'TextAlignment', width: 250, height: 380, render: () => { <TextAlignment />; } },
  { name: 'TextTruncation', width: 250, height: 300, render: () => { <TextTruncation />; } },
  { name: 'TextDecoration', width: 250, height: 280, render: () => { <TextDecoration />; } },
  { name: 'TextLetterSpacing', width: 280, height: 320, render: () => { <TextLetterSpacing />; } },
  { name: 'TextMultiline', width: 250, height: 380, render: () => { <TextMultiline />; } },
  { name: 'TextShadow', width: 250, height: 300, render: () => { <TextShadow />; } },
  { name: 'TextCombined', width: 280, height: 420, render: () => { <TextCombined />; } },
  { name: 'InlineImageLayout', width: 280, height: 350, render: () => { <InlineImageLayout />; } },
  { name: 'EmojiRendering', width: 280, height: 340, render: () => { <EmojiRendering />; } },
  { name: 'MaskImage', width: 280, height: 360, render: () => { <MaskImage />; } },
  { name: 'TextFieldRendering', width: 280, height: 320, render: () => { <TextFieldRendering />; } },
  { name: 'BorderStyles', width: 220, height: 340, render: () => { <BorderStyles />; } },
  { name: 'TransformTranslate', width: 200, height: 200, render: () => { <TransformTranslate />; } },
  { name: 'TransformScale', width: 220, height: 200, render: () => { <TransformScale />; } },
  { name: 'TransformRotate', width: 200, height: 220, render: () => { <TransformRotate />; } },
  { name: 'TransformComposed', width: 200, height: 200, render: () => { <TransformComposed />; } },
  { name: 'TransformOrigin', width: 200, height: 240, render: () => { <TransformOrigin />; } },
  { name: 'LayoutDirectionRtl', width: 200, height: 140, render: () => { <LayoutDirectionRtl />; } },
  { name: 'TransformDirectionTranslate', width: 200, height: 260, render: () => { <TransformDirectionTranslate />; } },
  { name: 'ColorPaletteThemes', width: 320, height: 164, render: () => { <ColorPaletteThemes />; } },
  { name: 'ColorPaletteNestedOverride', width: 220, height: 220, render: () => { <ColorPaletteNestedOverride />; } },
];
