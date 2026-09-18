import type { IRenderedElement } from '../IRenderedElement';
import type { RendererObserver } from '../IRenderer';
import type { Renderer } from '../Renderer';
import { DebugSettingKind, registerDebugSettingsGroup } from './DebugSettings';
import type { DebugSettingsRegistration } from './DebugSettings';

export enum NativeLayoutDirection {
  System = 'system',
  LeftToRight = 'ltr',
  RightToLeft = 'rtl',
}

interface RootDirectionState {
  appliedDirection: NativeLayoutDirection.LeftToRight | NativeLayoutDirection.RightToLeft;
  originalDirection: unknown;
}

interface RendererRegistration {
  readonly observer: RendererObserver;
  readonly rootDirections: Map<IRenderedElement, RootDirectionState>;
  renderCompletionScheduled: boolean;
}

export class NativeAppearanceDebugSettings {
  private readonly renderers = new Map<Renderer, RendererRegistration>();
  private readonly registration: DebugSettingsRegistration;
  private layoutDirection = NativeLayoutDirection.System;

  constructor() {
    this.registration = registerDebugSettingsGroup({
      categoryId: 'appearance',
      id: 'valdi.appearance',
      label: 'Appearance',
      settings: [
        {
          defaultValue: NativeLayoutDirection.System,
          description: 'Overrides layout direction for every mounted Valdi renderer.',
          get: () => this.layoutDirection,
          id: 'layout-direction',
          kind: DebugSettingKind.Select,
          label: 'Layout direction',
          options: [
            { label: 'System', value: NativeLayoutDirection.System },
            { label: 'Left to right', value: NativeLayoutDirection.LeftToRight },
            { label: 'Right to left', value: NativeLayoutDirection.RightToLeft },
          ],
          set: value => this.setLayoutDirection(value as NativeLayoutDirection),
        },
      ],
    });
  }

  addRenderer(renderer: Renderer): () => void {
    const previousRegistration = this.renderers.get(renderer);
    if (previousRegistration !== undefined) {
      renderer.removeObserver(previousRegistration.observer);
    }

    const observer: RendererObserver = {
      onRootElementWillEndRender: () => {
        this.applyLayoutDirection(renderer);
        this.schedulePostRenderApply(renderer);
      },
    };
    const rendererRegistration: RendererRegistration = {
      observer,
      renderCompletionScheduled: false,
      rootDirections: previousRegistration?.rootDirections ?? new Map<IRenderedElement, RootDirectionState>(),
    };
    this.renderers.set(renderer, rendererRegistration);
    renderer.addObserver(observer);
    this.applyLayoutDirection(renderer);

    return () => {
      if (this.renderers.get(renderer)?.observer === observer) {
        this.restoreLayoutDirections(renderer, rendererRegistration);
        this.renderers.delete(renderer);
        renderer.removeObserver(observer);
      }
    };
  }

  dispose(): void {
    this.registration.dispose();
    this.renderers.forEach((rendererRegistration, renderer) => {
      this.restoreLayoutDirections(renderer, rendererRegistration);
      renderer.removeObserver(rendererRegistration.observer);
    });
    this.renderers.clear();
  }

  private setLayoutDirection(layoutDirection: NativeLayoutDirection): void {
    this.layoutDirection = layoutDirection;
    this.renderers.forEach((_registration, renderer) => {
      this.applyLayoutDirection(renderer);
    });
  }

  private schedulePostRenderApply(renderer: Renderer): void {
    const registration = this.renderers.get(renderer);
    if (
      this.layoutDirection === NativeLayoutDirection.System ||
      registration === undefined ||
      registration.renderCompletionScheduled
    ) {
      return;
    }
    registration.renderCompletionScheduled = true;
    renderer.onRenderComplete(() => {
      if (this.renderers.get(renderer) !== registration) {
        return;
      }
      registration.renderCompletionScheduled = false;
      this.applyLayoutDirection(renderer);
    });
  }

  private applyLayoutDirection(renderer: Renderer): void {
    const registration = this.renderers.get(renderer);
    if (registration === undefined) {
      return;
    }
    const roots = renderer.getRootElements();
    this.pruneStaleRootDirections(roots, registration);
    const layoutDirection = this.layoutDirection;
    if (layoutDirection === NativeLayoutDirection.System) {
      this.restoreCurrentLayoutDirections(roots, registration);
      return;
    }

    roots.forEach(root => {
      const currentDirection = root.getAttribute('direction');
      const rootDirection = registration.rootDirections.get(root);
      if (rootDirection === undefined) {
        registration.rootDirections.set(root, {
          appliedDirection: layoutDirection,
          originalDirection: currentDirection,
        });
      } else {
        if (renderer.wasElementDirectionSetDuringCurrentRender(root)) {
          rootDirection.originalDirection = currentDirection;
        }
        rootDirection.appliedDirection = layoutDirection;
      }
      root.setAttribute('direction', layoutDirection);
    });
  }

  private restoreLayoutDirections(renderer: Renderer, registration: RendererRegistration): void {
    const roots = renderer.getRootElements();
    this.pruneStaleRootDirections(roots, registration);
    this.restoreCurrentLayoutDirections(roots, registration);
  }

  private pruneStaleRootDirections(roots: readonly IRenderedElement[], registration: RendererRegistration): void {
    const currentRoots = new Set(roots);
    registration.rootDirections.forEach((_rootDirection, root) => {
      if (currentRoots.has(root)) {
        return;
      }
      // Detached or destroyed roots are no longer safe mutation targets.
      registration.rootDirections.delete(root);
    });
  }

  private restoreCurrentLayoutDirections(roots: readonly IRenderedElement[], registration: RendererRegistration): void {
    roots.forEach(root => {
      const rootDirection = registration.rootDirections.get(root);
      if (rootDirection !== undefined) {
        root.setAttribute('direction', rootDirection.originalDirection);
        registration.rootDirections.delete(root);
      }
    });
  }
}
