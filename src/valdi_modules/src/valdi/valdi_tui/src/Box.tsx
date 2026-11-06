import { Component } from 'valdi_core/src/Component';
import { TUI_ATTRIBUTES, TUI_BOX_CLASS } from './Painter';
import { BorderStyle } from './borders';

export type FlexDirection = 'row' | 'column' | 'row-reverse' | 'column-reverse';
export type FlexWrap = 'nowrap' | 'wrap' | 'wrap-reverse';
export type AlignItems = 'flex-start' | 'center' | 'flex-end' | 'stretch';
export type AlignSelf = AlignItems | 'auto';
export type JustifyContent = 'flex-start' | 'center' | 'flex-end' | 'space-between' | 'space-around' | 'space-evenly';

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

  /** Draws a one-cell border on the enabled sides. Border cells are added to the padding. */
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

function side(
  specific: number | undefined,
  axis: number | undefined,
  all: number | undefined,
  border: number,
): number | undefined {
  const value = specific ?? axis ?? all;
  if (value === undefined && border === 0) {
    return undefined;
  }
  return (value ?? 0) + border;
}

/**
 * A flexbox container, the terminal equivalent of `<view>`. Children are slotted in.
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
    const borderTop = hasBorder && vm.borderTop !== false ? 1 : 0;
    const borderBottom = hasBorder && vm.borderBottom !== false ? 1 : 0;
    const borderLeft = hasBorder && vm.borderLeft !== false ? 1 : 0;
    const borderRight = hasBorder && vm.borderRight !== false ? 1 : 0;

    <custom-view
      iosClass={TUI_BOX_CLASS}
      androidClass={TUI_BOX_CLASS}
      width={vm.width}
      height={vm.height}
      minWidth={vm.minWidth}
      minHeight={vm.minHeight}
      maxWidth={vm.maxWidth}
      maxHeight={vm.maxHeight}
      paddingTop={side(vm.paddingTop, vm.paddingY, vm.padding, borderTop)}
      paddingBottom={side(vm.paddingBottom, vm.paddingY, vm.padding, borderBottom)}
      paddingLeft={side(vm.paddingLeft, vm.paddingX, vm.padding, borderLeft)}
      paddingRight={side(vm.paddingRight, vm.paddingX, vm.padding, borderRight)}
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
      backgroundColor={vm.backgroundColor}
      tuiBorderStyle={vm.borderStyle}
      tuiBorderColor={vm.borderColor}
      tuiBorderDimColor={vm.borderDimColor}
      tuiBorderTop={vm.borderTop}
      tuiBorderBottom={vm.borderBottom}
      tuiBorderLeft={vm.borderLeft}
      tuiBorderRight={vm.borderRight}
      tuiBorderTopColor={vm.borderTopColor}
      tuiBorderBottomColor={vm.borderBottomColor}
      tuiBorderLeftColor={vm.borderLeftColor}
      tuiBorderRightColor={vm.borderRightColor}
    >
      <slot />
    </custom-view>;
  }
}

// Referenced so the attribute names stay in one place even though JSX needs literals.
void TUI_ATTRIBUTES;
