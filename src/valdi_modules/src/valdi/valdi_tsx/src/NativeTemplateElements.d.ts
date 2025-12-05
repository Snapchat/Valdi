import { AccessibilityPriority } from './Accessibility';
import { Asset } from './Asset';
import { AttributedText } from './AttributedText';
import { GeometricPath } from './GeometricPath';
import { ElementFrame } from './Geometry';
import {
  ContentSizeChangeEvent,
  DragEvent,
  EventTime,
  PinchEvent,
  RotateEvent,
  ScrollDragEndEvent,
  ScrollDragEndingEvent,
  ScrollEndEvent,
  ScrollEvent,
  ScrollOffset,
  TouchEvent,
} from './GestureEvents';
import { IFontProvider } from './IFontProvider';
import { IRenderedElementHolder } from './IRenderedElementHolder';
import { IScrollPerfLoggerBridge } from './IScrollPerfLoggerBridge';
import { IStyle } from './IStyle';
import { ImageFilter } from './ImageFilter';
import { NativeView } from './NativeView';

/* eslint-disable @typescript-eslint/naming-convention */

type Color = string;

export type CSSValue = string | number;

/**
 * The base type for all template Elements.
 */
export interface TemplateElement {}

export interface ContainerTemplateElement extends TemplateElement {
  children?: unknown;
}

interface LayoutAttributes {
  /**
   * Called whenever the calculated frame for the element has changed.
   */
  onLayout?: (frame: ElementFrame) => void;

  /**
   * Called whenever the visibility for the Element has changed.
   */
  onVisibilityChanged?: (isVisible: boolean, eventTime: EventTime) => void;

  /**
   * Called whenever the calculated viewport for the Element has changed.
   * The viewport represents the rectangle area within the element's frame
   * that is visible.
   */
  onViewportChanged?: (viewport: ElementFrame, frame: ElementFrame, eventTime: EventTime) => void;

  /**
   * Called after a layout pass has completed, if this node or any of its children
   * were updated.
   */
  onLayoutComplete?: () => void;

  /**
   * Whether the Layout starting from this Element should be lazy.
   * A lazy layout is disconnected from its parent layout, its children
   * cannot impact the layout of the parent. When setting this, the size
   * of this Layout must be computable without calculating the layout
   * of the children.
   * Setting lazyLayout to true will defer layout calculation for all the
   * children until this Layout is determined to be visible. This can help
   * with performance when the Layout is within a ScrollView.
   * @default: false
   */
  lazyLayout?: boolean;

  /**
   * If set, the node will be set as a lazyLayout, and the given measure callback
   * will be called whenever the node needs to be measured. The callback
   * should return a MeasuredSize tuple representing how big the node should be.
   * Updating the onMeasure function will trigger a layout invalidation, which
   * will cause the node to be measured again. The final resolved node size can be
   * known through the onLayout attribute.
   */
  onMeasure?: OnMeasureFunc;

  /**
   * If set, the node will use this value as the base width that the node
   * should measure at before the children are inserted. This is typically
   * used on nodes that represent an empty placeholder that are later
   * rendered with children.
   * @default: undefined
   */
  estimatedWidth?: number;

  /**
   * If set, the node will use this value as the base height that the node
   * should measure at before the children are inserted. This is typically
   * used on nodes that represent an empty placeholder that are later
   * rendered with children.
   * @default: undefined
   */
  estimatedHeight?: number;

  /**
   * Whether the backing native View instance should only be created if
   * the Element is visible within the viewport. Setting this
   * to true can help with performance when the View is within a ScrollView.
   * @default: true
   */
  limitToViewport?: boolean;

  /**
   * Shorthand for lazyComponent, lazyLayout and limitToViewport.
   * @default: false
   */
  lazy?: boolean;

  /**
   * Used to uniquely identify an element in the global valdi context
   */
  id?: string;

  /**
   * A key to uniquely identify the element.
   * If it is not provided, the framework will generate one
   * based on the index in which the element is being rendered.
   */
  key?: string;

  /**
   * Can be used to disable animations for this element and its children.
   * @default: true
   */
  animationsEnabled?: boolean;

  /**
   * Loosely, zIndex specifies the order in which siblings are layered on top of each other.
   * Higher zIndex will mean that an element will be rendered "on top" of its siblings.
   *
   * In practice, Valdi renders elements in the order in which it encounters them.
   * Specifying a zIndex will make the framework sort this element and its siblings before
   * rendering the view tree.
   */
  zIndex?: number;

  /**
   * Sets a CSS class to use from the associated CSS document.
   * You typically would use the style property directly instead.
   */
  class?: string;

  /**
   * Whether the calculated viewport for this element should not
   * be intersected with the viewport of the parent. This allows
   * a child to have a viewport bigger than its parent.
   */
  ignoreParentViewport?: boolean;

  // Size

  /**
   * Specifies the width of an element's content area.
   * - auto (default value) width for the element based on its content.
   * - points Defines the width in absolute points. Depending on other styles set on the element, this may or may not be the final dimension of the node.
   * - percentage Defines the width in percentage of its parent's width.
   */
  width?: CSSValue;

  /**
   * Specifies the height of an element's content area.
   * - auto (default value) width/height for the element based on its content.
   * - points Defines the width/height in absolute points. Depending on other styles set on the element, this may or may not be the final dimension of the node.
   * - percentage Defines the width or height in percentage of its parent's width or height, respectively.
   */
  height?: CSSValue;

  // Size constraints

  /**
   * Specifies the minimum width of an element's content area.
   * Can be specified as either absolute point values or as percentages of their parent's size.
   */
  minWidth?: CSSValue;

  /**
   * Specifies the minimum height of an element's content area.
   * Can be specified as either absolute point values or as percentages of their parent's size.
   */
  minHeight?: CSSValue;

  /**
   * Specifies the maximum width of an element's content area.
   * Can be specified as either absolute point values or as percentages of their parent's size.
   */
  maxWidth?: CSSValue;

  /**
   * Specifies the minimum height of an element's content area.
   * Can be specified as either absolute point values or as percentages of their parent's size.
   */
  maxHeight?: CSSValue;

  /**
   * Specifies the aspect ratio of an element's content area.
   * Defined as the ratio between the width and the height of a node
   * e.g. if a node has an aspect ratio of 2 then its width is twice the size of its height.
   */
  aspectRatio?: number;

  /**
   * The position type of an element defines how it is positioned within its parent.
   *
   * When set to "relative": By default an element is positioned relatively.
   *   - This means an element is positioned according to the normal flow of the layout,
   *   - Then offset relative to that position based on the values of top, right, bottom, and left.
   *   - The offset does not affect the position of any sibling or parent elements.
   *
   * When set to "absolute": When positioned absolutely an element doesn't take part in the normal layout flow.
   *   - It is instead laid out independent of its siblings.
   *   - The position is solely determined based on the top, right, bottom, and left values.
   *
   * The position values top, right, bottom, and left behave differently depending on the position type of the element.
   * For a relative element they offset the position of the element in the direction specified.
   * For absolute element though these properties specify the offset of the element's side from the same side on the parent.
   *
   * @default: "relative"
   */
  position?: 'relative' | 'absolute';
  /**
   * @see position
   */
  top?: CSSValue;
  /**
   * @see position
   */
  right?: CSSValue;
  /**
   * @see position
   */
  bottom?: CSSValue;
  /**
   * @see position
   */
  left?: CSSValue;

  /**
   * Margin affects the spacing around the outside of a node.
   * A node with margin will offset itself from the bounds of its parent but also offset the location of any siblings.
   * The margin of a node contributes to the total size of its parent if the parent is auto sized.
   */
  margin?: string | number;
  /**
   * @see margin
   */
  marginTop?: CSSValue;
  /**
   * @see margin
   */
  marginRight?: CSSValue;

  /**
   * @see margin
   */
  marginBottom?: CSSValue;
  /**
   * @see margin
   */
  marginLeft?: CSSValue;

  // Flexbox attributes

  /**
   * @see LayoutChildrenAttributes.alignItems
   *
   * Align self has the same options and effect as `alignItems` but instead of affecting the children within a container,
   * you can apply this property to a single child to change its alignment within its parent. `alignSelf` overrides any option set by the parent with `alignItems`.
   *
   */
  alignSelf?: LayoutAlignSelfProperty;

  /**
   * The `flexWrap` property is set on containers and controls what happens when children overflow the size of the container along the main axis.
   * By default children are forced into a single line (which can shrink elements).
   *
   * If wrapping is allowed items are wrapped into multiple lines along the main axis if needed. `wrap-reverse` behaves the same, but the order of the lines is reversed.
   * When wrapping lines `alignContent` can be used to specify how the lines are placed in the container.
   */
  flexWrap?: LayoutFlexWrapProperty;

  /**
   * Describes how any space within a container should be distributed among its children along the main axis.
   * After laying out its children, a container will distribute any remaining space according to the flex grow values specified by its children.
   * Accepts any floating point value >= 0. A container will distribute any remaining space among its children weighted by the child’s flex grow value.
   * @default: 0
   */
  flexGrow?: number;

  /**
   * Describes how to shrink children along the main axis in the case that the total size of the children overflow the size of the container on the main axis.
   * flex shrink is very similar to flex grow and can be thought of in the same way if any overflowing size is considered to be negative remaining space.
   * These two properties also work well together by allowing children to grow and shrink as needed.
   * Accepts any floating point value >= 0. A container will shrink its children weighted by the child’s flex shrink value.
   * @default: 0
   */
  flexShrink?: number;

  /**
   * `flexBasis` is an axis-independent way of providing the default size of an item along the main axis.
   * Setting the flex basis of a child is similar to setting the `width` of that child if its parent is a container with `flexDirection: row`
   * or setting the `height` of a child if its parent is a container with `flexDirection: column`.
   * The flex basis of an item is the default size of that item, the size of the item before any flex grow and flex shrink calculations are performed.
   */
  flexBasis?: LayoutFlexBasisProperty;

  /**
   * Choose the display mode:
   * - "flex" use flexbox layout system
   * - "none" do not display this element
   * @default: 'flex'
   */
  display?: 'flex' | 'none';

  /**
   * Specifies how the parent will react to its children overflowing its boundaries
   * - when "visible", overflowing children elements will stretch the parent container
   * - when "scroll", the parent container's size will not be affected by the children element's bounds
   * Also impact the way the children views are being measured:
   * - when "visible", children's measured bounds will be clamped to the parent's size
   * - when "scroll", children's measured bounds will be unclamped in the direction of the main axis (flexDirection)
   * Based on the measured bounds of the children, this will impact the positioning of all the siblings elements
   * (default for layout elements: "visible")
   * (default for scroll elements: "scroll")
   */
  overflow?: 'visible' | 'scroll';

  /**
   * Specify meta information that can then be used by accessibility technologies
   *
   * For example:
   * - To announce the type of page elements and help the user navigating the page
   * - By allowing the user to jump from an important control elements to another
   *
   * Possible values are:
   * - 'auto' means that the value is automatically chosen depending on the element
   * - 'view' means that the element has no particular semantic meaning, a regular view
   * - 'text' means that the element is static text
   * - 'button' means that the element is a button control (can be navigated to)
   * - 'image' means that the element is an image
   * - 'image-button' means that the element is a button with an image
   * - 'input' means that the element is an input control (can be navigated to)
   * - 'header' means that the element is an important header (can be navigated to)
   *
   * @default: 'auto'
   */
  accessibilityCategory?: LayoutAccessibilityCategory;

  /**
   * Specify the way the element should be behave during the navigation of the element hierarchy by accessibility technologies
   *
   * Possible values are:
   * - 'auto' means that the value is automatically chosen depending on the element
   * - 'passthrough' means that the element is not focusable, but its children may be accessed
   * - 'leaf' means that the element is fully focusable and interactive but none of its children are navigatable
   * - 'cover' means that the element is first fully focusable and interactive and afterward its children can also be accessed
   * - 'group' means that element may be announced but not focused and its children may also be accessed (for lists of data)
   * - 'ignored' means that the element and all its children will be ignored, unnavigatable and unfocusable
   *
   * @default: 'auto'
   */
  accessibilityNavigation?: LayoutAccessibilityNavigation;

  /**
   * Specify the local priority of the element (compared to its direct sibling)
   * for accessibility navigation sequential ordering.
   *
   * Note: all sibling elements are sorted by descending priority during accessibility navigation
   *
   * For example:
   * - You might want to increase the priority on element branches used for navigation menus
   * - So all navigation commands can be read before starting to list the main data on the page
   *
   * @default: 0
   */
  accessibilityPriority?: LayoutAccessibilityPriority;

  /**
   * Defines the information being displayed:
   * When an element is accessed by the accessibility technologies,
   * it is a good practice to set an accessibilityLabel,
   * so that people who use VoiceOver know what element they have selected.
   * VoiceOver/TalkBack will read this string when a user focuses or navigate the element
   *
   * @default: undefined
   */
  accessibilityLabel?: string;

  /**
   * Defines the purpose of this element:
   * This is an additional hint that helps users understand what will happen,
   * when they perform an action on the accessibility element,
   * when that result is not clear from the accessibility label
   *
   * @default: undefined
   */
  accessibilityHint?: string;

  /**
   * For elements that intrinsically contains information (such as textinputs or sliders)
   * Especially when it may be dynamic, we need to provide its current value.
   */
  accessibilityValue?: string;

  /**
   * For dynamic and interactive elements, indicate that the element is temporarily disabled
   * @default: false
   */
  accessibilityStateDisabled?: boolean;

  /**
   * For dynamic and interactive elements, set the current selection status
   * @default: false
   */
  accessibilityStateSelected?: boolean;

  /**
   * For dynamic and interactive elements, indicate that the element frequently updates its label, value or children
   * @default: false
   */
  accessibilityStateLiveRegion?: boolean;
}

interface LayoutChildrenAttributes {
  /**
   * Padding affects the size of the node it is applied to.
   * Padding will not add to the total size of an element if it has an explicit size set.
   * For auto sized nodes padding will increase the size of the node as well as offset the location of any children.
   */
  padding?: string | number;
  /**
   * @see padding
   */
  paddingTop?: CSSValue;
  /**
   * @see padding
   */
  paddingRight?: CSSValue;
  /**
   * @see padding
   */
  paddingBottom?: CSSValue;
  /**
   * @see padding
   */
  paddingLeft?: CSSValue;

  /**
   * Layout direction specifies the direction in which children and text in a hierarchy should be laid out.
   * Layout direction also affects what edge start and end refer to.
   * In right-to-left environments, this will be set to `rtl`.
   *
   * - inherit (DEFAULT): Use the parent's direction value, if unspecified: equivalent to "ltr" when device's locale is LTR, equivalent to "rtl" when device's locale is RTL
   * - ltr: Text and children and laid out from left to right. Margin and padding applied the start of an element are applied on the left side.
   * - rtl: Text and children and laid out from right to left. Margin and padding applied the start of an element are applied on the right side.
   * @default: "inherit"
   */
  direction?: LayoutDirectionProperty;

  /**
   * Flex direction controls the direction in which children of a node are laid out.
   * This is also referred to as the main axis. The main axis is the direction in which children are laid out.
   *
   * The cross axis the the axis perpendicular to the main axis, or the axis which wrapping lines are laid out in.
   *
   * - `column` (DEFAULT)  Align children from top to bottom. If wrapping is enabled then the next line will start to the left first item on the top of the container.
   * - `column-reverse` Align children from bottom to top. If wrapping is enabled then the next line will start to the left first item on the bottom of the container.
   * - `row` Align children from left to right. If wrapping is enabled then the next line will start under the first item on the left of the container.
   * - `row-reverse` Align children from right to left. If wrapping is enabled then the next line will start under the first item on the right of the container.
   *
   * @see LayoutAttributes.flexWrap
   * @default: "column"
   */
  flexDirection?: LayoutFlexDirectionProperty;

  /**
   * Justify content describes how to align children within the main axis of their container.
   * For example, you can use this property to center a child horizontally within a container with `flex direction` set to `row`
   * or vertically within a container with `flex direction` set to `column`.
   *
   * - flex-start (DEFAULT) Align children of a container to the start of the container's main axis.
   * - flex-end Align children of a container to the end of the container's main axis.
   * - center Align children of a container in the center of the container's main axis.
   * - space-between Evenly space of children across the container's main axis, distributing remaining space between the children.
   * - space-around Evenly space of children across the container's main axis, distributing remaining space around the children.
   *   Compared to `space-between` using `space-around` will result in space being distributed to the beginning of the first child and end of the last child.
   * - space-evenly Evenly distributed within the alignment container along the main axis. The spacing between each pair of adjacent items,
   *   the main-start edge and the first item, and the main-end edge and the last item, are all exactly the same.
   * @default: "flex-start"
   */
  justifyContent?: LayoutJustifyContentProperty;

  /**
   * Align content defines the distribution of lines along the cross-axis. This only has effect when items are wrapped to multiple lines using `flexWrap`.
   *
   * - flex-start (DEFAULT) Align wrapped lines to the start of the container's cross axis.
   * - flex-end Align wrapped lines to the end of the container's cross axis.
   * - stretch Stretch wrapped lines to match the height of the container's cross axis.
   * - center Align wrapped lines in the center of the container's cross axis.
   * - space-between Evenly space wrapped lines across the container's main axis, distributing remaining space between the lines.
   * - space-around Evenly space wrapped lines across the container's main axis, distributing remaining space around the lines. Compared to space between using space around will result in space being distributed to the beginning of the first lines and end of the last line.
   * @default: "flex-start"
   */
  alignContent?: LayoutAlignContentProperty;

  /**
   * Align items describes how to align children along the cross axis of their container. Align items is very similar to justify content but instead of applying to the main axis, align items applies to the cross axis.
   * - stretch (DEFAULT) Stretch children of a container to match the height of the container's cross axis.
   * - flex-start Align children of a container to the start of the container's cross axis.
   * - flex-end Align children of a container to the end of the container's cross axis.
   * - center Align children of a container in the center of the container's cross axis.
   * - baseline Align children of a container along a common baseline. Individual children can be set to be the reference baseline for their parents.
   * @default: "stretch"
   */
  alignItems?: LayoutAlignItemsProperty;

  /**
   * Whether the calculated viewport for the element
   * should be potentially extended by taking in account
   * the space of all the children.
   * By default, a child element outside the bounds of the parent
   * element is considered invisible. When this flag is true,
   * the bounds of the parent will be extended such that the children
   * are always visible.
   * This flag should be used rarely and in very specific circumstances.
   * @default: false
   */
  extendViewportWithChildren?: boolean;
}

// These types exist solely to provide contravariance between the different element types
type _Layout = { __nativeElementType?: 'Layout' };
type _View = { __nativeElementType?: 'View' };
type _ScrollView = { __nativeElementType?: 'ScrollView' };
type _ImageView = { __nativeElementType?: 'ImageView' };
type _VideoView = { __nativeElementType?: 'VideoView' };
type _Label = { __nativeElementType?: 'Label' };
type _TextField = { __nativeElementType?: 'TextField' };
type _TextView = { __nativeElementType?: 'TextView' };
type _BlurView = { __nativeElementType?: 'BlurView' };
type _SpinnerView = { __nativeElementType?: 'SpinnerView' };
type _ShapeView = { __nativeElementType?: 'ShapeView' };

// Need to omit ref because TypeScript 3.9 has stricter checking of intersection types
// See https://www.typescriptlang.org/docs/handbook/release-notes/typescript-3-9.html#stricter-checks-on-intersections-and-optional-properties
// and https://github.com/microsoft/TypeScript/pull/37195
type _Style<T> = IStyle<Omit<T, 'ref'>>;

type GestureHandler<Event> = (event: Event) => void;
type GesturePredicate<Event> = (event: Event) => boolean;

/**
 * Represents a node in the layout tree.
 *
 * Use a <Layout/> instead of <View/> whenever possible, since
 * these nodes will not be backed by a platform view when rendering.
 *
 * @NativeTemplateElement({layout: true, jsx: 'layout'})
 * */
export interface Layout extends _Layout, ContainerTemplateElement, LayoutAttributes, LayoutChildrenAttributes {
  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

/**
 * @NativeTemplateElement({slot: true})
 * */
export interface Slot extends TemplateElement {
  /**
   * A key to uniquely identify the element.
   * If it is not provided, the framework will generate one
   * based on the index in which the element is being rendered.
   */
  key?: string;
  /**
   * The unique name of the Slot.
   * If not provided, it would be considered to be named
   * 'default'.
   */
  name?: string;
  /**
   * Sets an element reference holder, which will keep track
   * of the elements rendered at the root of this slot.
   */
  ref?: IRenderedElementHolder<unknown>;
}

interface ViewAttributes {
  /**
   * Whether the backing native View instance can be re-used.
   * By default, View instances are re-used across screens.
   * By setting that property to false, the view instance will
   * be guaranteed to be new and not re-used from the view pool.
   * @default: true
   */
  allowReuse?: boolean;

  // Lifecycle

  /**
   * Called whenever the backing native view is created.
   */
  onViewCreate?: () => void;

  /**
   * Called whenever the backing native view is destroyed.
   */
  onViewDestroy?: () => void;

  /**
   * Called whenever the backing native view is created or destroyed.
   *
   * @param nativeView NativeView reference of the view that was created or undefined when the view was destroyed and not replaced with a new instance.
   * @deprecated use {@link IRenderedElement.getNativeNode}
   */
  onViewChange?: (nativeView: NativeView | undefined) => void;

  // Basic appearance

  /**
   * Sets the background of the view.
   *
   * Use `"color1"` to set a color.
   * Use `"linear-gradient(color1, color2, color3...)"` to set a gradient with evenly-spaced stops.
   * Use `"linear-gradient(color1 stop1, color2 stop2, color3 stop3...)"` to set a color with custom stops.
   */
  background?: string;

  /**
   * Sets the background color of the view.
   *
   * `undefined` sets a clear color.
   */
  backgroundColor?: Color;

  /**
   * Sets the opacity of the view.
   *
   * Accepts values: [0.0 - 1.0]
   *
   * Note: Making a view non-opaque will require layer blending.
   * */
  opacity?: number;

  /**
   * Enables clipping of the view's contents based on the view's borders.
   *
   * Warning: degrades performance, avoid using if possible.
   * @default: false
   */
  slowClipping?: boolean;

  /**
   * Short-hand for all the borderXXX attributes
   *
   * TODO: document
   */
  border?: string;

  /**
   * The borderWidth attribute sets the width of an element's border.
   * note: make sure to set the borderColor
   * @default: 0
   */
  borderWidth?: CSSValue;

  /**
   * The borderColor attribute sets the color of an element's four borders.
   * note: make sure to set the borderWidth for the border to be visible
   * @default: black
   */
  borderColor?: Color;

  /**
   * The borderRadius attribute defines the radius of the element's corners.
   * This attribute can have from one to four values.
   * Tip: This attribute allows you to add rounded corners to elements!
   * @example
   * ```
   * "5 10 0 20" // will specify the radius on a per-corner basis (top/left/right/bottom)
   * "10" // will set the radius for all corners at the same time
   * ```
   * @default: 0
   */
  borderRadius?: CSSValue;

  /**
   * Add a shadow to the view
   * Accepts strings with syntax: '{xOffset} {yOffset} {shadowOverflow} {color}'
   * All numbers values are interpreted as points values
   * @example
   * ```
   * boxShadow(0, 2, 10, 'rgba(0, 0, 0, 0.1)')
   * boxShadow(0, -2, 0, SemanticColor.Elevation.CELL_SHADOW)
   * ```
   */
  boxShadow?: string;

  /**
   * [Web-Only]
   * Sets the mouse cursor to be displayed when hovering over the element.
   * This property is only effective on web platforms.
   * @example
   * ```
   * cursor="pointer"
   * cursor="default"
   * cursor="not-allowed"
   * ```
   * @default: "default"
   */
  cursor?: string;

  // Gestures

  /**
   * Set this to `false` to disable any user interactions
   * on this view and its children.
   * @default: true
   */
  touchEnabled?: boolean;

  /**
   * Determines if a given view can capture any touches
   */
  hitTest?: (event: TouchEvent) => boolean;

  /**
   * Event handler called on every touch event captured by this view (started, changed, ended)
   */
  onTouch?: (event: TouchEvent) => void;

  /**
   * Event handler called on the 'started' touch events captured by this view
   */
  onTouchStart?: (event: TouchEvent) => void;

  /**
   * Event handler called on the 'ended' touch events captured by this view
   */
  onTouchEnd?: (event: TouchEvent) => void;

  /**
   * Specifies the minimum duration, in seconds, for an onTouch event to trigger.
   * When not set, will be zero.
   */
  onTouchDelayDuration?: number;

  /**
   * Set to `true` to disable tap completely.
   * If set to `false`, onTapPredicate will still be evaluated, if provided.
   * @default false
   */
  onTapDisabled?: boolean;

  /**
   * The handler that will be called when the user performs a tap gesture on this view
   */
  onTap?: GestureHandler<TouchEvent>;

  /**
   * The predicate that will be used to decide whether the tap gesture should be recognized
   * Prefer using {@link onTapDisabled} if the decision can be made without {@link TouchEvent} data.
   */
  onTapPredicate?: GesturePredicate<TouchEvent>;

  /**
   * Set to `true` to disable double tap completely.
   * If set to `false`, onDoubleTapPredicate will still be evaluated, if provided.
   * @default false
   */
  onDoubleTapDisabled?: boolean;

  /**
   * The handler that will be called when the user performs a double tap gesture on this view
   */
  onDoubleTap?: GestureHandler<TouchEvent>;

  /**
   * The predicate that will be used to decide whether the double tap gesture should be recognized
   * Prefer using {@link onDoubleTapDisabled} if the decision can be made without {@link TouchEvent} data.
   */
  onDoubleTapPredicate?: GesturePredicate<TouchEvent>;

  /**
   * Specifies the minimum duration, in seconds, for a long press to trigger.
   * When not set, will use a platform provided default.
   */
  longPressDuration?: number;

  /**
   * Set to `true` to disable long press completely.
   * If set to `false`, onLongPressPredicate will still be evaluated, if provided.
   * @default false
   */
  onLongPressDisabled?: boolean;

  /**
   * The handler that will be called when the user performs a long press gesture on this view
   */
  onLongPress?: GestureHandler<TouchEvent>;

  /**
   * The predicate that will be used to decide whether the long press gesture should be recognized
   * Prefer using {@link onLongPressDisabled} if the decision can be made without {@link TouchEvent} data.
   */
  onLongPressPredicate?: GesturePredicate<TouchEvent>;

  /**
   * Set to `true` to disable drag completely.
   * If set to `false`, onDragPredicate will still be evaluated, if provided.
   * @default false
   */
  onDragDisabled?: boolean;

  /**
   * The handler that will be called when the user performs a dragging gesture that started on this view
   */
  onDrag?: GestureHandler<DragEvent>;

  /**
   * The predicate that will be used to decide whether the drag gesture should be recognized.
   * Prefer using {@link onDragDisabled} if the decision can be made without {@link DragEvent} data.
   */
  onDragPredicate?: GesturePredicate<DragEvent>;

  /**
   * Set to `true` to disable pinch completely.
   * If set to `false`, onPinchPredicate will still be evaluated, if provided.
   * @default false
   */
  onPinchDisabled?: boolean;

  /**
   * The handler that will be called when the user performs a pinch gesture on this view
   */
  onPinch?: GestureHandler<PinchEvent>;

  /**
   * The predicate that will be used to decide whether the pinch gesture should be recognized
   * Prefer using {@link onPinchDisabled} if the decision can be made without {@link PinchEvent} data.
   */
  onPinchPredicate?: GesturePredicate<PinchEvent>;

  /**
   * Set to `true` to disable rotate completely.
   * If set to `false`, onRotatePredicate will still be evaluated, if provided.
   * @default false
   */
  onRotateDisabled?: boolean;

  /**
   * The handler that will be called when the user performs a rotate gesture on this view
   */
  onRotate?: GestureHandler<RotateEvent>;

  /**
   * The predicate that will be used to decide whether the rotate gesture should be recognized
   * Prefer using {@link onRotateDisabled} if the decision can be made without {@link RotateEvent} data.
   */
  onRotatePredicate?: GesturePredicate<RotateEvent>;

  /**
   * Can be used to increase the view's touch target area.
   */
  touchAreaExtension?: number;
  /**
   * @see touchAreaExtension
   */
  touchAreaExtensionTop?: number;
  /**
   * @see touchAreaExtension
   */
  touchAreaExtensionRight?: number;
  /**
   * @see touchAreaExtension
   */
  touchAreaExtensionBottom?: number;
  /**
   * @see touchAreaExtension
   */
  touchAreaExtensionLeft?: number;

  // Transform attributes

  /**
   * Specifies the horizontal scale component of the affine transformation to be applied to the view.
   *
   * @see transform for the order in which the transformations are applied
   */
  scaleX?: number;

  /**
   * Specifies the vertical scale component of the affine transformation to be applied to the view.
   *
   * @see transform for the order in which the transformations are applied
   */
  scaleY?: number;

  /**
   * Specifies the rotation component in angle radians of the affine transformation to be applied to the view.
   *
   * @see transform for the order in which the transformations are applied
   */
  rotation?: number;

  /**
   * Specifies the horizontal translation component of the affine transformation to be applied to the view.
   *
   * NOTE: When the device is in RTL mode, the applied translationX value will be flipped
   *
   * @see transform for the order in which the transformations are applied
   */
  translationX?: number;

  /**
   * Specifies the vertical translation component of the affine transformation to be applied to the view.
   *
   * @see transform for the order in which the transformations are applied
   */
  translationY?: number;

  /**
   * Sets the view's accessibility identifier.
   * note: Commonly used to identify UI elements in UI tests.
   */
  accessibilityId?: string;

  /**
   * Forces the platform surface method canScroll to always return true for horizontal touch events
   * - This property can be used to ensure that any platform-specific gesture handlers can be ignored when a valdi module would capture the event
   */
  canAlwaysScrollHorizontal?: boolean;

  /**
   * Forces the platform surface method canScroll to always return true for horizontal touch events
   * - This property can be used to ensure that any platform-specific gesture handlers can be ignored when a valdi module would capture the event
   */
  canAlwaysScrollVertical?: boolean;

  /**
   * Set an opacity to use on mask. The opacity defines how much the mask
   * should "erase" pixels that match the maskPath. Opacity of 1 will make
   * all the pixels matching the path transparent.
   * Default to 1.
   */
  maskOpacity?: number;

  /**
   * Set a geometric path to use as a mask on the given. Pixels that are within the given
   * path will be turned transparent relative to the maskOpacity.
   */
  maskPath?: GeometricPath;

  /**
   * Optionally set filterTouchesWhenObscured for payment sensitive button on Android.
   * It's not used for iOS.
   *
   * https://developer.android.com/reference/android/view/View#setFilterTouchesWhenObscured(boolean)
   */
  filterTouchesWhenObscured?: boolean;
}

interface CommonView extends TemplateElement, ViewAttributes, LayoutAttributes, LayoutChildrenAttributes {}

/**
 * Represents a node in the layout tree.
 *
 * Use a <Layout/> instead of <View/> whenever possible, since
 * these nodes will not be backed by a platform view when rendering.
 *
 * @NativeTemplateElement({ios: 'SCValdiView', android: 'com.snap.valdi.views.ValdiView', jsx: 'view'})
 * */
export interface View extends _View, CommonView, ContainerTemplateElement {
  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

interface LeafView extends TemplateElement, ViewAttributes, LayoutAttributes {}

export const enum EditTextUnfocusReason {
  Unknown = 0,
  ReturnKeyPress = 1,
  DismissKeyPress = 2,
}

interface EditTextEvent {
  text: string;
  selectionStart: number;
  selectionEnd: number;
}

interface EditTextBeginEvent extends EditTextEvent {}

interface EditTextEndEvent extends EditTextEvent {
  reason: EditTextUnfocusReason;
}

/**
 * Shared by textfield, textview
 */
export interface CommonEditTextInterface extends LeafView, CommonTextAttributes {
  /**
   * [iOS-Only]
   * Setting the tintColor will change the color of the editing caret and drag handles
   * of the text field.
   */
  tintColor?: Color;

  /**
   * Sets the foreground color of the placeholder text.
   */
  placeholderColor?: Color;

  /**
   * The string that is displayed when there is no other text in the text field.
   */
  placeholder?: string;

  /**
   * Callback called when the text value is about to change
   * The event parameter contains the current text value of the text field before any change
   * Must return the new text value expected to replace the last text value
   */
  onWillChange?: (event: EditTextEvent) => EditTextEvent | undefined;

  /**
   * Callback that will get called whenever the user changes the text value of the
   * text field.
   * The event parameter contains the current text value of the text field.
   * Note: there usually is 2 ways of handling a text input value
   * - 1) The parent constantly overwrite the value attribute when user type (detected through onChange)
   * - 2) The parent sets the value to undefined and just reads changes through onChange
   * @see value
   */
  onChange?: (event: EditTextEvent) => void;

  /**
   * Callback that will get called when the user starts editing the textfield
   * (i.e. when the text field becomes focused)
   * The event parameter contains the current text value of the text field.
   */
  onEditBegin?: (event: EditTextBeginEvent) => void;

  /**
   * Callback that will get called when the user stops editing the textfield
   * (i.e. when the text field stops being focused)
   * The event parameter contains the current text value of the text field.
   * The event parameter will also contain the expected reason for the end of the typing
   */
  onEditEnd?: (event: EditTextEndEvent) => void;

  /**
   * Callback called when the user taps the return key
   * The event parameter contains the current text value of the text field.
   */
  onReturn?: (event: EditTextEvent) => void;

  /**
   * Callback called when the user press the delete key on the keyboard
   * The event parameter contains the current text value of the text field before characters are deleted.
   * note: on android some soft-keyboard may only send this when pressing delete on an empty textfield
   */
  onWillDelete?: (event: EditTextEvent) => void;

  /**
   * Set whether the text input is editable or not
   * @default: true
   */
  enabled?: boolean;

  /**
   * Set the text alignment within typing box of the text input
   * @default: "left"
   */
  textAlign?: TextFieldTextAlign;

  /**
   * Determines at what times the Shift key is automatically pressed,
   * thereby making the typed character a capital letter.
   * @default: "sentences"
   */
  autocapitalization?: TextFieldAutocapitalization;

  /**
   * Determines whether autocorrection is enabled or disabled during typing
   * @default: "default"
   */
  autocorrection?: TextFieldAutocorrection;

  /**
   * Determines the maximum number of characters allowed to be entered into the text field.
   * note: an undefined or negative value means there is no limit on the allowed number of characters
   * @default: undefined
   */
  characterLimit?: number;

  /**
   * Whether to select the contents of the textfield when it becomes focused
   * @default: false
   */
  selectTextOnFocus?: boolean;

  /**
   * Dismiss keyboard when the return key is pressed.
   * Is enabled by default
   * (default for textfield elements: true)
   * (default for textview elements: false)
   */
  closesWhenReturnKeyPressed?: boolean;

  /**
   * [iOS-Only]
   * Force keyboard appearance to dark/light (ignoring the current system appearance)
   */
  keyboardAppearance?: TextFieldKeyboardAppearance;

  /**
   * Selection for the text field
   * - first index for start of selection
   * - second index for end of selection
   * - set both to the same to select at a single position
   */
  selection?: [number, number];

  /**
   * Callback called when the selection is changed
   * The event parameter contains the current text value of the text field and the selected indexes
   */
  onSelectionChange?: (event: EditTextEvent) => void;

  /**
   * Whether to enable inline predictions for the text input.
   * note: This is only relevant on iOS. No-op if used on Android.
   * @default: false
   */
  enableInlinePredictions?: boolean;
}

/**
 * Represents an editable TextField.
 *
 * @NativeTemplateElement({ios: 'SCValdiTextField', android: 'com.snap.valdi.views.ValditText', jsx: 'textfield'})
 * */
export interface TextField extends _TextField, CommonEditTextInterface {
  /**
   * The content type identifies what keyboard keys
   * and capabilities are available on the input and which ones appear by default.
   * @default: 'default'
   */
  contentType?: TextFieldContentType;

  /**
   * Setting this property to a different key type changes the visible title of the Return key.
   * note: This might not actually be text, e.g. on Android 10 setting this to 'search' will make
   * the return key display a search glass icon
   * @default: 'done'
   */
  returnKeyText?: TextFieldReturnKeyText;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<TextField | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

/**
 * The textfield also have programmatic-only fields
 * (should not be used in TSX tags, only programmatic attribute manipulation)
 */
export interface TextFieldInteractive extends TextField {
  /**
   * Can be programmatically set to change the current focus state of the textfield
   * Changing this to true will make this textfield become the currently edited one.
   * Example:
   *  - on iOS this will make the view request/resign the first responder
   *  - on Android this will make the view become focused/unfocused and open/close the keyboard
   */
  focused?: boolean;
}

// @NativeTemplateElement({ios: 'SCValdiTextView', android: 'com.snap.valdi.views.ValditTextMultiline', jsx: 'textview'})
export interface TextView extends _TextView, CommonEditTextInterface {
  /**
   * Setting this property to a different key type changes the visible title of the Return key.
   * Setting this property will also impact the behavior of the return key.
   * "linereturn" will let users add line returns, but any other value will be constrained to single line text
   * consider using this attribute in combination with "closesWhenReturnKeyPressed"
   * @default: "linereturn"
   */
  returnType?: TextViewReturnType;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<TextView | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;

  /**
   * Set the text gravity/alignment vertically within the text view
   * @default: "center"
   */
  textGravity?: TextViewTextGravity;

  /**
   * Set the color for the background effect of the text view
   * This background is drawn behind each line fragment of the text, wrapping each line together making a coheasive
   * background that follows the text shape
   * @default: clear
   */
  backgroundEffectColor?: Color;

  /**
   * Set the background effect border radius
   * @default: 0
   */
  backgroundEffectBorderRadius?: number;

  /**
   * Set the background effect padding
   * This padding is applied only around the exterior edge of the text as a whole
   * (It does not apply to the space between lines of text)
   * @default: 0
   */
  backgroundEffectPadding?: number;
}

/**
 * The textview also have programmatic-only fields
 * (should not be used in TSX tags, only programmatic attribute manipulation)
 */
export interface TextViewInteractive extends TextView {
  /**
   * Can be programmatically set to change the current focus state of the textview
   * Changing this to true will make this textview become the currently edited one.
   * Example:
   *  - on iOS this will make the view request/resign the first responder
   *  - on Android this will make the view become focused/unfocused and open/close the keyboard
   */
  focused?: boolean;
}

export type ImageOnAssetLoadCallback = (success: boolean, errorMessage?: string) => void;
export type ImageOnImageDecodedCallback = (width: number, height: number) => void;

// @NativeTemplateElement({ios: 'SCValdiImageView', android: 'com.snap.valdi.views.ValdiImageView', jsx: 'image'})
export interface ImageView extends _ImageView, LeafView {
  /**
   * Specify the image asset or url to be rendered within the image's bounds
   * @default: undefined
   */
  src?: string | Asset;

  /**
   * Define how the image should be resized within the image view's bounds
   * useful when the aspect ratio of the image bounds may be different than the image asset content's aspect ratio
   * Common values:
   *  - "fill": stretch the image to fill the image view's bounds
   *  - "contain": conserve aspect ratio and scale the image to fit within the image view's bounds. Can leave blank space around the image within the view.
   *  - "cover": conserve aspect ratio and scale the image to fill the image view's bounds. Can result in parts of the image being cropped in the image's view.
   *  - "none": conserve aspect ratio and doesn't try to scale to fit in the image view's bounds (just rendered in the center)
   * @default: 'fill'
   */
  objectFit?: ImageObjectFit;

  /**
   * Called when the loaded image asset has either successfully loaded or failed to load
   */
  onAssetLoad?: ImageOnAssetLoadCallback;

  /**
   * Called when the image has been loaded and we have the dimensions.
   * NOTE: dimensions returned are of the raw image which may be different than the view
   */
  onImageDecoded?: ImageOnImageDecodedCallback;

  /**
   * Apply a color tint on every pixel of the image
   * @default: undefined
   */
  tint?: Color;

  /**
   * When the current layout is in RTL, we horizontally-mirror-flip the image's content
   * @default: false
   */
  flipOnRtl?: boolean;

  /**
   * Scale horizontally the content of the image within the image's bounds
   * @default: 1
   */
  contentScaleX?: number;
  /**
   * Scale vertically the content of the image within the image's bounds
   * @default: 1
   */
  contentScaleY?: number;

  /**
   * Rotate the content of the image in radians
   * @default: 0
   */
  contentRotation?: number;

  /**
   * A post processing filter to apply on the image.
   */
  filter?: ImageFilter;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<ImageView | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

// @NativeTemplateElement({ios: 'SCValdiVideoView', android: 'com.snap.valdi.views.ValdiVideoView', jsx: 'video'})
export interface VideoView extends _VideoView, LeafView {
  /**
   * Specify the image asset or url to be rendered within the image's bounds
   * @default: undefined
   */
  src?: string | Asset;

  /**
   * Float between 1 and 0
   */
  volume?: number;

  /**
   * 0 is paused
   * 1 is playing
   * defaults to paused
   */
  playbackRate?: number;

  /**
   * In milliseconds
   */
  seekToTime?: number;

  /**
   * A callback to be called when the video is loaded.
   * Hands back the length of the video in milliseconds.
   */
  onVideoLoaded?: (duration: number) => void;

  /**
   * Callback for when the video begins playing.
   */
  onBeginPlaying?: () => void;

  /**
   * Callback to be called when there's an error.
   */
  onError?: (error: string) => void;

  /**
   * Callback to be called when the video completes.
   */
  onCompleted?: () => void;

  /**
   * Callback called when the video progress updates.
   * time is in milliseconds
   * duration is in milliseconds
   * Frequency may differ from platform to platform.
   */
  onProgressUpdated?: (time: number, duration: number) => void;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<ImageView | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

/**
 * Shared by label, button, textfield, textview
 */
export interface CommonTextAttributes {
  /**
   * The text value of the label/text field/view.
   *
   * Use the `AttributedTextBuilder` class to set a value composed of multiple
   * strings with different text attributes.
   *
   */
  value?: LabelValue;

  /**
   * Set the font used to render the text characters
   * This attribute must be a string with 4 parts separated by space:
   * - 1) required: the name of the font (including the font's weight)
   * - 2) required: the size of the font
   * - 3) optional: the scaling type (or 'unscaled' for no scaling)
   * - 4) optional: the maximum size of the font after scaling
   * @example: 'AvenirNext-Bold 16 unscaled 16'
   * @default: 'system 12'
   */
  font?: string;
  /**
   * Set the rendering color for the text characters
   * @default: black
   */
  color?: Color;
  /**
   * Set the Gradient color for the text string. Overrides color setting
   *
   * Use `"linear-gradient(color1, color2, color3...)"` to set a gradient with evenly-spaced stops.
   * Use `"linear-gradient(color1 stop1, color2 stop2, color3 stop3...)"` to set a color with custom stops.
   */
  textGradient?: string;

  /**
   * Add a shadow to the text
   * Accepts strings with syntax: '{color} {radius} {opacity} {offsetX} {offsetY}'
   * All numbers values are interpreted as points values
   *
   * NOTE: TextShadow and BoxShadow are mutually exclusive, you can have one per label.
   *
   * @example
   * ```
   * 'rgba(0, 0, 0, 0.1) 1 1 1 1'
   * `${SemanticColor.Elevation.CELL_SHADOW} 2 0.5 0 0`
   * ```
   */
  textShadow?: string;
}

/**
 * Shared by label, button
 */
export interface CommonLabel extends CommonTextAttributes {
  /**
   * This property controls the maximum number of lines
   * to use in order to fit the label’s text into its bounding rectangle.
   * To remove any maximum limit, and use as many lines as needed, set the value of this property to 0.
   * @default: 1
   */
  numberOfLines?: number;

  /**
   * Set the horizontal alignment behavior for the label's text
   * note: it will automatically invert in RTL locales:
   * - in RTL locales, "left" will align text to the right of the label's bounds
   * - in RTL locales, "right" will align text to the left of the label's bounds
   * @default: left
   */
  textAlign?: LabelTextAlign;

  /**
   * Optionally adds a visual decoration effect to the label's text
   * @default: undefined
   */
  textDecoration?: LabelTextDecoration;

  /**
   * Rendering size of each line of the label, this value is a ratio of the font height.
   * If the lineHeight ratio is above 1, spacing is added on top of each line of the text
   * @default: 1
   * Example: A value of 2 will double the height of each line
   */
  lineHeight?: number;

  /**
   * Extra spacing added at the end of each character, in points
   * note: negative values will shrink the space between characters, in points
   * @default: 0
   */
  letterSpacing?: number;
}

// @NativeTemplateElement({ios: 'SCValdiLabel', android: 'android.widget.TextView', jsx: 'label'})
export interface Label extends _Label, LeafView, CommonLabel {
  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<Label | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;

  /**
   * Normally, the label draws the text with the font you specify in the font property.
   * If this property is "true", and the text in the text property exceeds the label’s bounding rectangle,
   * the label reduces the font size until the text fits or it has scaled the font down to the minimum font size.
   * If you change it to "true", be sure that you also set an appropriate minimum font scale
   * by modifying the minimumScaleFactor property.
   * notes:
   * - This autoshrinking behavior is mostly intended for use with a single-line label when numberOfLines == 1.
   * - This will also work when for multiline using a numberOfLines > 0 but the behavior will be much less predictable
   * - Specifying the label's width/height/minHeight will make the auto-shrink behavior much more predictable
   * - This is because there is multiple theoretically valid text sizes when label's bounds are not strictly well defined
   * @default: false
   * @see minimumScaleFactor
   */
  adjustsFontSizeToFitWidth?: boolean;

  /**
   * If the adjustsFontSizeToFitWidth is "true", use this property to specify the smallest multiplier
   * for the current font size that yields an acceptable font size for the label’s text.
   * @default: 0
   * @see adjustsFontSizeToFitWidth
   */
  minimumScaleFactor?: number;

  /**
   * Controls how hidden text overflow content is signaled to users. It can be clipped or display
   * an ellipsis.
   * @default: 'ellipsis'
   */
  textOverflow?: 'ellipsis' | 'clip';
}

// @NativeTemplateElement({ios: 'SCValdiScrollView', android: 'com.snap.valdi.views.ValdiScrollView', jsx: 'scroll'})
export interface ScrollView extends _ScrollView, CommonView, ContainerTemplateElement {
  /**
   * Called when the content offset of the scrollview view changed
   */
  onScroll?: (event: ScrollEvent) => void;

  /**
   * Called when the content offset of the scrollview has settled
   */
  onScrollEnd?: (event: ScrollEndEvent) => void;

  /**
   * Called when the user start dragging the scroll content
   */
  onDragStart?: (event: ScrollEvent) => void;

  /**
   * Called synchronously when the ScrollView will end dragging.
   * The called function can return a scroll offset that should be
   * used to replace the content offset where the scroll view should
   * settle.
   */
  onDragEnding?: (event: ScrollDragEndingEvent) => ScrollOffset | undefined;

  /**
   * Called when the ScrollView ended dragging. This will be called right
   * after onDragEnding() is called, and will be called asynchronously
   * by default unlike onDragEnding. The scroll view might still be animating
   * its scroll at this point.
   */
  onDragEnd?: (event: ScrollDragEndEvent) => void;

  /**
   * Called whenever the content size of the scroll view has changed
   */
  onContentSizeChange?: (event: ContentSizeChangeEvent) => void;

  /**
   * Configure whether or not there should be a visual effect when the user tries to scroll past the scroll boundaries:
   * - on iOS: this visually looks like letting the user "bounce" on the edge of the scroll view
   * - on Android: this looks like a glow or squish effect that follow the finger or a bounce if using skia
   * @default: true
   */
  bounces?: boolean;

  /**
   * Allow bouncing by dragging even when the the content offset is already at the minimum
   * @default: true
   */
  bouncesFromDragAtStart?: boolean;
  /**
   * Allow bouncing by dragging even when the the content offset is already at the maximum
   * @default: true
   */
  bouncesFromDragAtEnd?: boolean;

  /**
   * Allow bouncing even when the vertical content size is smaller than the scroll itself
   * @default: false
   */
  bouncesVerticalWithSmallContent?: boolean;
  /**
   * Allow bouncing even when the horizontal content size is smaller than the scroll itself
   * @default: false
   */
  bouncesHorizontalWithSmallContent?: boolean;

  /**
   * Cancels all active touches gestures (from onTouch) of the scroll view's children
   * when dragging the scroll view content
   * @default: true
   */
  cancelsTouchesOnScroll?: boolean;

  /**
   * If the keyboard is open, close it when we start scrolling
   * @default: false
   */
  dismissKeyboardOnDrag?: boolean;

  /**
   * When dismissKeyboardOnDrag is `true` this changes the behavior of when the keyboard is dismissed:
   *
   *  - immediate : keyboard is dismissed as soon as scrolling begins
   *  - touch-exit-below : keyboard is dismissed when the scroll drag gesture touches leave the lower boundary of the <scroll> bounds
   *  - touch-exit-above : keyboard is dismissed when the scroll drag gesture touches leave the upper boundary of the <scroll> bounds
   *
   * @default: 'immediate'
   */
  dismissKeyboardOnDragMode?: 'immediate' | 'touch-exit-below' | 'touch-exit-above';

  /**
   * When enabled, the scroll content offset will always settle on a multiple of the scrollview's size
   * @default: false
   */
  pagingEnabled?: boolean;

  /**
   * When enabled, the scroll view will allow horizontal scrolling instead of vertical
   * Note: this attributes replaces flexDirection
   *  - When enabled, children will be layout as if flexDirection="row" was set
   *  - When disabled, children will be layout as if flexDirection="column" was set
   * @default: false
   */
  horizontal?: boolean;

  /**
   * FlexDirection is not available on scroll views, it can be set through the "horizontal" attribute
   * Or just by adding a child layout element as the child of the scroll element
   */
  flexDirection?: never;

  /**
   * Shows the scroll indicator when scrolling vertically
   * @default: true
   */
  showsVerticalScrollIndicator?: boolean;
  /**
   * Shows the scroll indicator when scrolling horizontally
   * @default: true
   */
  showsHorizontalScrollIndicator?: boolean;

  /**
   * When enabled, the scroll view can be scrolled by the user, otherwise it cannot be dragged manually
   * @default: true
   */
  scrollEnabled?: boolean;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<ScrollView | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;

  /**
   * Set this to enable scroll performance logging with given attribution parameters.
   * To get a value of this type @see AttributedCallsite.forScrollPerfLogging
   */
  scrollPerfLoggerBridge?: IScrollPerfLoggerBridge;

  /**
   * Enables circular scroll by specifying the ratio between the calculated content size
   * of the scroll element to the size of a single scrollable page. For circular scroll to
   * work properly, the children elements of the scroll element must be re-rendered at
   * least twice, in which case `circularRatio` should be set to 2.
   * @default: 0, scroll is not circular
   */
  circularRatio?: number;

  /**
   * Create a fade effect on the content of the scroll view when scrolling is possible
   * This attribute can control the size of the effect
   * @default: 0
   */
  fadingEdgeLength?: number;

  /**
   * [iOS-Only]
   * Defines the rate at which the scroll view decelerates after
   * a fling gesture.
   * @default: 'normal'
   */
  decelerationRate?: 'normal' | 'fast';

  /**
   * Can be used to extend the visible viewport of the scroll element.
   * When scrolling, this will cause child elements to be rendered
   * potentially earlier, if the value is positive, or later, if the
   * value is negative.
   *
   * @default: 0
   */
  viewportExtensionTop?: number;

  /**
   * Can be used to extend the visible viewport of the scroll element.
   * When scrolling, this will cause child elements to be rendered
   * potentially earlier, if the value is positive, or later, if the
   * value is negative.
   *
   * @default: 0
   */
  viewportExtensionRight?: number;

  /**
   * Can be used to extend the visible viewport of the scroll element.
   * When scrolling, this will cause child elements to be rendered
   * potentially earlier, if the value is positive, or later, if the
   * value is negative.
   *
   * @default: 0
   */
  viewportExtensionBottom?: number;

  /**
   * Can be used to extend the visible viewport of the scroll element.
   * When scrolling, this will cause child elements to be rendered
   * potentially earlier, if the value is positive, or later, if the
   * value is negative.
   *
   * @default: 0
   */
  viewportExtensionLeft?: number;
}

/**
 * The scrollview also have programmatic-only fields
 * (should not be used in TSX tags, only programmatic attribute manipulation)
 */
export interface ScrollViewInteractive extends ScrollView {
  /**
   * Can be programmatically set to change the current horizontal scroll offset of the scroll view
   */
  contentOffsetX?: number;
  /**
   * Can be programmatically set to change the current vertical scroll offset of the scroll view
   */
  contentOffsetY?: number;
  /**
   * When set to true, any programmatic change to the contentOffset will be animated, otherwise it will not
   */
  contentOffsetAnimated?: boolean;

  /**
   * When set, the scrollable content width of the scroll view will be based on this value for operations like snapping
   * and scrolling. This will by pass the automatic measurement.
   * Setting this value will skip waiting for the measure step and allow operations such as scrolling to occurr faster.
   */
  staticContentWidth?: number;

  /**
   * When set, the scrollable content height of the scroll view will be based on this value for operations like snapping
   * and scrolling. This will by pass the automatic measurement.
   * Setting this value will skip waiting for the measure step and allow operations such as scrolling to occurr faster.
   */
  staticContentHeight?: number;
}

// @NativeTemplateElement({ios: 'SCValdiSpinnerView', android: 'com.snap.valdi.views.ValiSpinnerView', jsx: 'spinner'})
export interface SpinnerView extends _SpinnerView, LeafView {
  /**
   * Color of the spinning shape
   */
  color?: Color;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<SpinnerView | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

export type BlurStyle =
  | 'light'
  | 'dark'
  | 'extraLight'
  | 'regular'
  | 'prominent'
  | 'systemUltraThinMaterial'
  | 'systemThinMaterial'
  | 'systemMaterial'
  | 'systemThickMaterial'
  | 'systemChromeMaterial'
  | 'systemUltraThinMaterialLight'
  | 'systemThinMaterialLight'
  | 'systemMaterialLight'
  | 'systemThickMaterialLight'
  | 'systemChromeMaterialLight'
  | 'systemUltraThinMaterialDark'
  | 'systemThinMaterialDark'
  | 'systemMaterialDark'
  | 'systemThickMaterialDark'
  | 'systemChromeMaterialDark';

// FIXME: we don't have a BlurView on Android?
// @NativeTemplateElement({ios: 'SCValdiBlurView', android: 'com.snap.valdi.views.ValdiView', jsx: 'blur'})
export interface BlurView extends _BlurView, ViewAttributes, LayoutAttributes, ContainerTemplateElement {
  blurStyle?: BlurStyle;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<BlurView | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

export type ShapeStrokeCap = 'butt' | 'round' | 'square';
export type ShapeStrokeJoin = 'bevel' | 'miter' | 'round';

// @NativeTemplateElement({ios: 'SCValdiShapeView', android: 'com.snap.valdi.views.ShapeView', jsx: 'shape'})
export interface ShapeView extends _ShapeView, LeafView {
  // TODO: document

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<ShapeView | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;

  /**
   * The GeometricPath object representing the path to draw.
   */
  path?: GeometricPath;

  /**
   * Defines the thickness of the shape's stroked path
   */
  strokeWidth?: number;

  /**
   * The color that the stroked path is drawn with.
   */
  strokeColor?: Color;

  /**
   * The color that the shape's enclosed area is filled with.
   */
  fillColor?: Color;

  /**
   * The stroke cap specifies the shape of the endpoints of an open path when stroked.
   */
  strokeCap?: ShapeStrokeCap;

  /**
   * The stroke join specifies the shape of the joints between connected segments of a stroked path.
   */
  strokeJoin?: ShapeStrokeJoin;

  /**
   * The relative location at which to begin stroking the path.
   * Default value is 0. Animatable.
   */
  strokeStart?: number;

  /**
   * The relative location at which to stop stroking the path.
   * Default value is 1. Animatable.
   */
  strokeEnd?: number;
}

export interface AnimatedImageOnProgressEvent {
  /**
   * The current time in seconds
   */
  time: number;
  /**
   * The duration of the animation in seconds
   */
  duration: number;
}

export type AnimatedImageOnProgressCallback = (event: AnimatedImageOnProgressEvent) => void;

// @NativeTemplateElement({ios: 'SCValdiAnimatedContentView', android: 'com.snap.valdi.views.AnimatedImageView', jsx: 'animatedimage'})
export interface AnimatedImage extends LeafView {
  /**
   * Specify the image asset or url to be rendered within the view's bounds
   * @default: undefined, nothing rendered by default
   */
  src?: string | Asset;

  /**
   * Specify whether the image animation should loop back to the beginning
   * when reaching the end.
   */
  loop?: boolean;

  /**
   * Sets the speed ratio at which the image animation should run, 0 is paused, 1 means
   * the animation runs at normal speed, 0.5 at half speed, -1 the animation will run
   * in reverse.
   * Default is 0.
   */
  advanceRate?: number;

  /**
   * Called when the loaded animatedimage asset has either successfully loaded or failed to load
   */
  onAssetLoad?: ImageOnAssetLoadCallback;

  /**
   * Called when the animatedimage has been loaded and we have the dimensions.
   * NOTE: dimensions returned are of the raw image which may be different than the view
   */
  onImageDecoded?: ImageOnImageDecodedCallback;

  /**
   * Called when the animation progresses
   */
  onProgress?: AnimatedImageOnProgressCallback;

  /**
   * Set the current time in seconds for the image animation as an offset from animationStartTime.
   */
  currentTime?: number;

  /**
   * Set the start time in seconds for the image animation
   * Animation will be clamped between start and end time
   * @default: 0
   */
  animationStartTime?: number;

  /**
   * Set the end time in seconds for the image animation
   * Animation will be clamped between start and end time
   * @default: animation duration
   */
  animationEndTime?: number;

  /**
   * Set a font provider that the lottie element will use to resolve fonts
   * within its scenes.
   */
  fontProvider?: IFontProvider;

  /**
   * Define how the animatedimage should be resized within the image view's bounds
   * useful when the aspect ratio of the animatedimage bounds may be different than the asset content's aspect ratio.
   * Note that the default is different from the image element.
   * Common values:
   *  - "fill": stretch the image to fill the image view's bounds
   *  - "contain": conserve aspect ratio and scale the image to fit within the image view's bounds. Can leave blank space around the image within the view.
   *  - "cover": conserve aspect ratio and scale the image to fill the image view's bounds. Can result in parts of the image being cropped in the image's view.
   *  - "none": conserve aspect ratio and doesn't try to scale to fit in the image view's bounds (just rendered in the center)
   * @default: 'contain'
   */
  objectFit?: ImageObjectFit;

  /**
   * Styling object allows to set multiple attribute at once
   */
  style?: _Style<AnimatedImage | View | Layout>;

  /**
   * Sets an element reference holder, which will keep track
   * of the rendered elements.
   */
  ref?: IRenderedElementHolder<this>;
}

// Layout attributes types
type LayoutDirectionProperty = 'inherit' | 'ltr' | 'rtl';

type LayoutFlexDirectionProperty = 'column' | 'column-reverse' | 'row' | 'row-reverse';
type LayoutFlexWrapProperty = 'no-wrap' | 'wrap' | 'wrap-reverse';

type LayoutAlignProperty =
  | 'auto'
  | 'flex-start'
  | 'center'
  | 'flex-end'
  | 'stretch'
  | 'baseline'
  | 'space-between'
  | 'space-around';

type LayoutAlignContentProperty = LayoutAlignProperty;
type LayoutAlignItemsProperty = LayoutAlignProperty;
type LayoutAlignSelfProperty = LayoutAlignProperty;

type LayoutJustifyContentProperty =
  | 'flex-start'
  | 'center'
  | 'flex-end'
  | 'space-between'
  | 'space-around'
  | 'space-evenly';

type LayoutFlexBasisProperty = CSSValue;

export type LayoutAccessibilityCategory =
  | 'auto'
  | 'view'
  | 'text'
  | 'button'
  | 'image'
  | 'image-button'
  | 'input'
  | 'header'
  | 'link'
  | 'checkbox'
  | 'radio'
  | 'keyboard-key';
export type LayoutAccessibilityNavigation = 'auto' | 'passthrough' | 'leaf' | 'cover' | 'group' | 'ignored';
export type LayoutAccessibilityPriority = number | AccessibilityPriority;

// Label attributes types
export type LabelValue = string | AttributedText;
export type LabelTextDecoration = 'none' | 'strikethrough' | 'underline';
export type LabelTextAlign = 'left' | 'right' | 'center' | 'justified';
export type LabelFontWeight = 'light' | 'normal' | 'medium' | 'demi-bold' | 'bold' | 'black';
export type LabelFontStyle = 'normal' | 'italic';

// Image attributes types
export type ImageObjectFit = 'fill' | 'contain' | 'cover' | 'none';

// TextField attributes types
export type TextFieldAutocapitalization = 'sentences' | 'words' | 'characters' | 'none';
export type TextFieldAutocorrection = 'default' | 'none';
export type TextFieldTextAlign = 'left' | 'center' | 'right';
export type TextFieldReturnKeyText = 'done' | 'go' | 'join' | 'next' | 'search' | 'send' | 'continue';
export type TextFieldKeyboardAppearance = 'default' | 'dark' | 'light';
export type TextFieldContentType =
  | 'default'
  | 'phoneNumber'
  | 'email'
  | 'password'
  | 'url'
  | 'number'
  | 'numberDecimal'
  | 'numberDecimalSigned'
  | 'passwordNumber'
  | 'passwordVisible'
  | 'noSuggestions';

// TextView attributes types
export type TextViewReturnType = 'linereturn' | TextFieldReturnKeyText;
export type TextViewTextGravity = 'top' | 'center' | 'bottom';

export type MeasuredSize = [number, number];

export const enum MeasureMode {
  Unspecified = 0,
  Exactly = 1,
  AtMost = 2,
}

export type OnMeasureFunc = (
  width: number,
  widthMode: MeasureMode,
  height: number,
  heightMode: MeasureMode,
) => MeasuredSize;
