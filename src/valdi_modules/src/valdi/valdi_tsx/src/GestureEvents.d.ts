import { Size, Point } from './Geometry';

export const enum TouchEventState {
  Started = 0,
  Changed = 1,
  Ended = 2,
}

/**
 * Represents an event time in seconds given by the runtime.
 */
export type EventTime = number;

export interface GestureEvent {
  /**
   * The time in seconds when the event triggered
   */
  eventTime?: EventTime;
}

export interface Pointer extends Point {
  pointerId: number;
}

export interface TouchEvent extends GestureEvent, Point {
  /**
   * The state of the touch event
   */
  state: TouchEventState;

  /**
   * X Position from within the element
   */
  x: number;
  /**
   * Y position from within the element
   */
  y: number;
  /**
   * X position from within the root element
   */
  absoluteX: number;
  /**
   * Y position from within the root element
   */
  absoluteY: number;
  /**
   * Count of active pointers
   */
  pointerCount: number;
  /**
   * Locations of active pointers
   */
  pointerLocations: Pointer[];
}

export interface HoverEvent extends GestureEvent, Point {
  /**
   * Whether the pointer is now over the element
   */
  hovered: boolean;

  /** True on an exit with unavailable coordinates, such as when the view is removed. */
  detached?: true;

  /**
   * X Position from within the element
   */
  x: number;
  /**
   * Y position from within the element
   */
  y: number;
  /**
   * X position from within the root element
   */
  absoluteX: number;
  /**
   * Y position from within the root element
   */
  absoluteY: number;
}

export interface DragEvent extends TouchEvent {
  /**
   * Delta X from when the event started
   */
  deltaX: number;

  /**
   * Delta Y from when the event started
   */
  deltaY: number;

  /**
   * Current X velocity
   */
  velocityX: number;

  /**
   * Current Y velocity
   */
  velocityY: number;
}

export interface PinchEvent extends TouchEvent {
  /**
   * Delta scale from when the event started
   */
  scale: number;
}

export interface RotateEvent extends TouchEvent {
  /**
   * Delta rotation from when the event started, radians
   */
  rotation: number;
}

export interface ScrollOffset {
  /**
   * current scroll contentOffsetX (may be clamped to the edge of the scrollable content in android)
   */
  x: number;
  /**
   * current scroll contentOffsetY (may be clamped to the edge of the scrollable content in android)
   */
  y: number;
}

export interface ScrollVelocity {
  /**
   * X velocity at the moment the touch was released, in points per second.
   */
  velocityX: number;

  /**
   * Y velocity at the moment the touch was released, in points per second.
   */
  velocityY: number;
}

export interface ScrollOffsetWithOverscroll extends ScrollOffset {
  /**
   * Current horizontal tension of the edge effects applied if user is trying to scroll but the contentOffsetX is clamped
   */
  overscrollTensionX?: number;
  /**
   * Current vertical tension of the edge effects applied if user is trying to scroll but the contentOffsetY is clamped
   */
  overscrollTensionY?: number;
}

export interface ScrollEvent extends GestureEvent, ScrollOffsetWithOverscroll, ScrollVelocity {}

export interface ScrollEndEvent extends GestureEvent, ScrollOffsetWithOverscroll, ScrollVelocity {}

export interface ScrollDragEndingEvent extends GestureEvent, ScrollOffsetWithOverscroll, ScrollVelocity {}

export interface ScrollDragEndEvent extends GestureEvent, ScrollOffsetWithOverscroll, ScrollVelocity {}

export interface ContentSizeChangeEvent extends Size {}
