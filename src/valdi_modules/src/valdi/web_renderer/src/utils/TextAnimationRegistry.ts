import type { AttributeApplierContext } from '../core/ElementClass';
import type { TextAnimationControllerRegistry } from './TextAnimationController';
import { TextAnimationGroupController, TextAnimationParticipant } from './TextAnimationController';

const TEXT_ANIMATION_PARTICIPANT_STATE_KEY = '__textAnimationParticipantState';
const GROUP_CONTROLLERS_BY_ELEMENT = new WeakMap<HTMLElement, TextAnimationGroupController>();
const PARTICIPANTS_BY_ELEMENT = new WeakMap<HTMLElement, TextAnimationParticipant>();

interface TextAnimationParticipantState {
  participant?: TextAnimationParticipant;
}

const TEXT_ANIMATION_CONTROLLER_REGISTRY: TextAnimationControllerRegistry = {
  hasTextAnimationGroup(element: HTMLElement): boolean {
    return GROUP_CONTROLLERS_BY_ELEMENT.has(element);
  },

  nearestTextAnimationGroup(element: HTMLElement): TextAnimationGroupController | undefined {
    let parent = element.parentElement;
    while (parent) {
      const group = GROUP_CONTROLLERS_BY_ELEMENT.get(parent);
      if (group) {
        return group;
      }
      parent = parent.parentElement;
    }
    return undefined;
  },

  textAnimationParticipantForElement(element: HTMLElement): TextAnimationParticipant | undefined {
    return PARTICIPANTS_BY_ELEMENT.get(element);
  },
};

export function registerTextAnimationGroup(element: HTMLElement): TextAnimationGroupController {
  const controller = new TextAnimationGroupController(element, TEXT_ANIMATION_CONTROLLER_REGISTRY);
  GROUP_CONTROLLERS_BY_ELEMENT.set(element, controller);
  return controller;
}

export function unregisterTextAnimationGroup(element: HTMLElement): void {
  GROUP_CONTROLLERS_BY_ELEMENT.get(element)?.destroy();
  GROUP_CONTROLLERS_BY_ELEMENT.delete(element);
}

export function setTextAnimationGroupFlushDurationThreshold(
  element: HTMLElement,
  flushDurationThreshold: number | undefined,
): void {
  GROUP_CONTROLLERS_BY_ELEMENT.get(element)?.setFlushDurationThreshold(flushDurationThreshold);
}

export function setTextAnimationGroupFlushMultiplier(element: HTMLElement, flushMultiplier: number | undefined): void {
  GROUP_CONTROLLERS_BY_ELEMENT.get(element)?.setFlushMultiplier(flushMultiplier);
}

export function registerTextAnimationParticipant(
  ownerElement: HTMLElement,
  container: HTMLElement,
  context: AttributeApplierContext,
): void {
  const state = textAnimationParticipantState(context);
  let participant = state.participant;
  if (!participant) {
    participant = new TextAnimationParticipant(ownerElement, TEXT_ANIMATION_CONTROLLER_REGISTRY);
    state.participant = participant;
    PARTICIPANTS_BY_ELEMENT.set(ownerElement, participant);
  }

  participant.setContainer(container);
  if (!participant.hasTextAnimationParts()) {
    unregisterTextAnimationParticipant(context);
    return;
  }

  participant.startFrameLoopIfNeeded();
}

export function unregisterTextAnimationParticipant(context: AttributeApplierContext): void {
  const state = context.getState<TextAnimationParticipantState>(TEXT_ANIMATION_PARTICIPANT_STATE_KEY);
  const participant = state?.participant;
  if (!participant) {
    return;
  }

  participant.destroy();
  PARTICIPANTS_BY_ELEMENT.delete(participant.ownerElement);
  state.participant = undefined;
}

function textAnimationParticipantState(context: AttributeApplierContext): TextAnimationParticipantState {
  let state = context.getState<TextAnimationParticipantState>(TEXT_ANIMATION_PARTICIPANT_STATE_KEY);
  if (state) {
    return state;
  }

  state = {};
  context.setState(TEXT_ANIMATION_PARTICIPANT_STATE_KEY, state);
  context.addCleanup(() => {
    state?.participant?.destroy();
    if (state?.participant) {
      PARTICIPANTS_BY_ELEMENT.delete(state.participant.ownerElement);
      state.participant = undefined;
    }
  });
  return state;
}
