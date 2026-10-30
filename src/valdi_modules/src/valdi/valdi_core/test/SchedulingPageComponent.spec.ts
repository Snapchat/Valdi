import 'jasmine/src/jasmine';
import { IRenderer } from '../src/IRenderer';
import { SchedulingPageComponent, SchedulingStrategy } from '../src/SchedulingPageComponent';

// Renderer completion callbacks aren't cancelled on destroy, so this stub queues them and lets the
// test destroy the component before they fire, as a quick re-render does.
class DeferringRenderer {
  readonly renderCompletions: Array<() => void> = [];
  readonly layoutCompletions: Array<() => void> = [];
  readonly destroyed = new Set<unknown>();

  onRenderComplete(callback: () => void): void {
    this.renderCompletions.push(callback);
  }

  onLayoutComplete(callback: () => void): void {
    this.layoutCompletions.push(callback);
  }

  isComponentAlive(component: unknown): boolean {
    return !this.destroyed.has(component);
  }

  registerComponentDisposable(component: unknown): void {
    if (this.destroyed.has(component)) {
      throw new Error('Resolved destroyed Component');
    }
  }

  flush(): void {
    for (const callback of this.renderCompletions.splice(0)) {
      callback();
    }
    for (const callback of this.layoutCompletions.splice(0)) {
      callback();
    }
  }
}

class TestPage extends SchedulingPageComponent {
  criticalCount = 0;
  nonCriticalCount = 0;

  onCreateCritical(): void {
    this.criticalCount++;
  }

  onCreateNonCritical(): void {
    this.nonCriticalCount++;
    this.registerDisposable(() => {});
  }
}

function makePage(strategy: SchedulingStrategy): { page: TestPage; renderer: DeferringRenderer } {
  const renderer = new DeferringRenderer();
  const page = new TestPage(renderer as unknown as IRenderer, {}, undefined);
  page.schedulingStrategy = strategy;
  return { page, renderer };
}

describe('SchedulingPageComponent', () => {
  for (const strategy of [SchedulingStrategy.RenderComplete, SchedulingStrategy.LayoutComplete]) {
    describe(`with strategy ${strategy}`, () => {
      it('runs onCreateNonCritical once the deferred callback fires', () => {
        const { page, renderer } = makePage(strategy);

        page.onCreate();
        expect(page.criticalCount).toBe(1);
        expect(page.nonCriticalCount).toBe(0);

        renderer.flush();
        expect(page.nonCriticalCount).toBe(1);
      });

      it('skips onCreateNonCritical when destroyed before the deferred callback fires', () => {
        const { page, renderer } = makePage(strategy);

        page.onCreate();
        page.onDestroy();
        renderer.destroyed.add(page);

        expect(() => renderer.flush()).not.toThrow();
        expect(page.nonCriticalCount).toBe(0);
      });
    });
  }
});
