import { Component } from 'valdi_core/src/Component';
import { parseColor } from './ansi';

export type FlexDirection = 'row' | 'column' | 'row-reverse' | 'column-reverse';
export type FlexWrap = 'nowrap' | 'wrap' | 'wrap-reverse';
export type AlignItems = 'flex-start' | 'center' | 'flex-end' | 'stretch';
export type AlignSelf = AlignItems | 'auto';
export type JustifyContent = 'flex-start' | 'center' | 'flex-end' | 'space-between' | 'space-around' | 'space-evenly';

/** The border glyph sets of the terminal backend. */
export type BorderStyle =
  | 'single'
  | 'double'
  | 'round'
  | 'bold'
  | 'singleDouble'
  | 'doubleSingle'
  | 'classic'
  | 'arrow';

/** Layout and decoration of a {@link Box}. Mirrors Ink's `<Box>` props; sizes are in terminal cells. */
export interface BoxProps {
  readonly width?: number | string;
  readonly height?: number | string;
  readonly minWidth?: number | string;
  readonly minHeight?: number | string;
  readonly maxWidth?: number | string;
  readonly maxHeight?: number | string;

  readonly padding?: number;
  readonly paddingX?: number;
  readonly paddingY?: number;
  readonly paddingTop?: number;
  readonly paddingBottom?: number;
  readonly paddingLeft?: number;
  readonly paddingRight?: number;

  readonly margin?: number;
  readonly marginX?: number;
  readonly marginY?: number;
  readonly marginTop?: number;
  readonly marginBottom?: number;
  readonly marginLeft?: number;
  readonly marginRight?: number;

  readonly gap?: number;
  readonly columnGap?: number;
  readonly rowGap?: number;

  readonly flexGrow?: number;
  readonly flexShrink?: number;
  readonly flexBasis?: number | string;
  readonly flexDirection?: FlexDirection;
  readonly flexWrap?: FlexWrap;
  readonly alignItems?: AlignItems;
  readonly alignSelf?: AlignSelf;
  readonly justifyContent?: JustifyContent;

  readonly display?: 'flex' | 'none';
  readonly overflow?: 'visible' | 'hidden';
  readonly position?: 'relative' | 'absolute';
  readonly top?: number;
  readonly right?: number;
  readonly bottom?: number;
  readonly left?: number;

  readonly backgroundColor?: string;

  /** Draws a one-cell border on the enabled sides. The border cells are part of the box's layout. */
  readonly borderStyle?: BorderStyle;
  readonly borderColor?: string;
  readonly borderDimColor?: boolean;
  readonly borderTop?: boolean;
  readonly borderBottom?: boolean;
  readonly borderLeft?: boolean;
  readonly borderRight?: boolean;
  readonly borderTopColor?: string;
  readonly borderBottomColor?: string;
  readonly borderLeftColor?: string;
  readonly borderRightColor?: string;
}

const VIEW_IOS_CLASS = 'SCValdiView';
const VIEW_ANDROID_CLASS = 'com.snap.valdi.views.ValdiView';

/**
 * A flexbox container. Children are slotted in.
 *
 * ```tsx
 * <Box borderStyle='round' padding={1} flexDirection='column'>
 *   <Text value='Hello' />
 * </Box>
 * ```
 */
export class Box extends Component<BoxProps> {
  onRender(): void {
    const vm = this.viewModel;
    const hasBorder = vm.borderStyle !== undefined;
    // Yoga lays out the border cells itself; disabled sides give theirs back.
    const sideWidth = (enabled: boolean | undefined): number | undefined =>
      hasBorder ? (enabled === false ? 0 : 1) : undefined;

    // custom-view with the view classes, because the border attributes of the terminal backend are
    // not part of the <view> typings.
    <custom-view
      iosClass={VIEW_IOS_CLASS}
      androidClass={VIEW_ANDROID_CLASS}
      width={vm.width}
      height={vm.height}
      minWidth={vm.minWidth}
      minHeight={vm.minHeight}
      maxWidth={vm.maxWidth}
      maxHeight={vm.maxHeight}
      paddingTop={vm.paddingTop ?? vm.paddingY ?? vm.padding}
      paddingBottom={vm.paddingBottom ?? vm.paddingY ?? vm.padding}
      paddingLeft={vm.paddingLeft ?? vm.paddingX ?? vm.padding}
      paddingRight={vm.paddingRight ?? vm.paddingX ?? vm.padding}
      marginTop={vm.marginTop ?? vm.marginY ?? vm.margin}
      marginBottom={vm.marginBottom ?? vm.marginY ?? vm.margin}
      marginLeft={vm.marginLeft ?? vm.marginX ?? vm.margin}
      marginRight={vm.marginRight ?? vm.marginX ?? vm.margin}
      gap={vm.gap}
      columnGap={vm.columnGap}
      rowGap={vm.rowGap}
      flexGrow={vm.flexGrow}
      flexShrink={vm.flexShrink ?? 1}
      flexBasis={vm.flexBasis}
      flexDirection={vm.flexDirection ?? 'row'}
      flexWrap={vm.flexWrap}
      alignItems={vm.alignItems}
      alignSelf={vm.alignSelf}
      justifyContent={vm.justifyContent}
      display={vm.display}
      overflow={vm.overflow}
      position={vm.position}
      top={vm.top}
      right={vm.right}
      bottom={vm.bottom}
      left={vm.left}
      backgroundColor={parseColor(vm.backgroundColor)}
      borderWidth={hasBorder ? 1 : undefined}
      borderTopWidth={sideWidth(vm.borderTop)}
      borderBottomWidth={sideWidth(vm.borderBottom)}
      borderLeftWidth={sideWidth(vm.borderLeft)}
      borderRightWidth={sideWidth(vm.borderRight)}
      borderStyle={vm.borderStyle}
      borderColor={parseColor(vm.borderColor)}
      borderDim={vm.borderDimColor}
      borderTop={vm.borderTop}
      borderBottom={vm.borderBottom}
      borderLeft={vm.borderLeft}
      borderRight={vm.borderRight}
      borderTopColor={parseColor(vm.borderTopColor)}
      borderBottomColor={parseColor(vm.borderBottomColor)}
      borderLeftColor={parseColor(vm.borderLeftColor)}
      borderRightColor={parseColor(vm.borderRightColor)}
    >
      <slot />
    </custom-view>;
  }
}
