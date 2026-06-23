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
];
