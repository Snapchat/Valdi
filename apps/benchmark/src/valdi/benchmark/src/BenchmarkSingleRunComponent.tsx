import { StatefulComponent } from 'valdi_core/src/Component';
import { IComponent } from 'valdi_core/src/IComponent';
import { IComponentRenderObserver } from 'valdi_core/src/IComponentRenderObserver';
import { onIdleInterruptible } from 'valdi_core/src/utils/OnIdle';

export type BenchmarkCompleteVerifierCallback = (component: IComponent) => boolean;
export type BenchmarkCompleteCallback = () => void;

export interface BenchmarkResult {}

export interface BenchmarkRunner {
  start(): void;
  incrementRunCount(): void;
  measureAsync(metricName: string): () => void;
  stop(): BenchmarkResult;
}

export interface BenchmarkSingleRunComponentViewModel {
  benchmarkRunner: BenchmarkRunner;

  /**
   * Verify that the component fully finished rendering.
   * @param component
   */
  verifyCompleted: BenchmarkCompleteVerifierCallback;

  /**
   * Called whenever the component has fully finished rendering.
   * @param result
   */
  onComplete: BenchmarkCompleteCallback;
}

interface State {
  didSettleOnce: boolean;
}

/**
 * A component which takes care of inflating a component through its slot,
 * and calls an `onComplete` callback with timing informations about how
 * long it took for the app to fully settle after the rendering.
 * For more accurate measures, the benchmark component waits until an initial settle before
 * starting to render the component.
 */
export class BenchmarkSingleRunComponent
  extends StatefulComponent<BenchmarkSingleRunComponentViewModel, State>
  implements IComponentRenderObserver
{
  state: State = { didSettleOnce: false };

  private onIdleSequence = 0;
  private benchmarkedComponent?: IComponent;
  private onSettleCompletion?: () => void;
  private didComplete = false;

  onCreate(): void {
    this.viewModel.benchmarkRunner.incrementRunCount();
    this.onSettleCompletion = this.viewModel.benchmarkRunner.measureAsync('Settle');

    this.renderer.addObserver(this);

    onIdleInterruptible(() => {
      this.setState({ didSettleOnce: true });
    });
  }

  onDestroy(): void {
    this.renderer.removeObserver(this);
  }

  onRender(): void {
    if (!this.state.didSettleOnce) {
      return;
    }

    const onDone = this.viewModel.benchmarkRunner.measureAsync('Render Component');

    <slot />;

    const emittedComponents = this.renderer.getComponentChildren(this);
    if (emittedComponents.length !== 1) {
      throw new Error(
        `Only one component should be inflated inside the slot of BenchmarkComponent, we got ${emittedComponents.length}`,
      );
    }

    const benchmarkedComponent = emittedComponents[0];
    this.benchmarkedComponent = benchmarkedComponent;

    this.scheduleOnRenderEnd(onDone);
  }

  onComponentWillRerender(component: IComponent): void {
    if (!this.isComponentIsBenchmarkedComponentSubtree(component)) {
      return;
    }
    const onDone = this.viewModel.benchmarkRunner.measureAsync('Render Component');

    this.scheduleOnRenderEnd(onDone);
  }

  private isComponentIsBenchmarkedComponentSubtree(component: IComponent): boolean {
    const benchmarkedComponent = this.benchmarkedComponent;
    if (!benchmarkedComponent) {
      return false;
    }

    let current: IComponent | undefined = component;
    while (current) {
      if (current === benchmarkedComponent) {
        return true;
      }

      current = this.renderer.getComponentParent(current);
    }

    return false;
  }

  private scheduleOnRenderEnd(onDone: () => void): void {
    this.renderer.onRenderComplete(() => {
      onDone();

      const sequence = ++this.onIdleSequence;
      onIdleInterruptible(() => {
        this.onMainThreadIdle(sequence);
      });
    });
  }

  private onMainThreadIdle(sequence: number): void {
    if (this.isDestroyed() || sequence !== this.onIdleSequence) {
      return;
    }

    if (this.viewModel.verifyCompleted(this.benchmarkedComponent!)) {
      this.notifyBenchmarkCompleted();
    }
  }

  private notifyBenchmarkCompleted(): void {
    if (this.didComplete) {
      return;
    }

    this.didComplete = true;
    const settleCompletion = this.onSettleCompletion;
    this.onSettleCompletion = undefined;
    if (settleCompletion) {
      settleCompletion();
    }

    this.viewModel.onComplete();
  }
}
