function exists<T>(value: T | null | undefined): T {
  if (!value) {
    throw 'Expected a non-null, non-undefined value';
  }
  return value;
}

import { formatParameterizedString, LegacyVueComponent } from 'valdi_core/src/Valdi';
import { IComponent } from 'valdi_core/src/IComponent';
import { IRenderedVirtualNode } from 'valdi_core/src/IRenderedVirtualNode';
import { IRenderer } from 'valdi_core/src/IRenderer';
import { IRendererEventListener } from 'valdi_core/src/IRendererEventListener';
import { classNames } from 'valdi_core/src/utils/ClassNames';
import 'jasmine/src/jasmine';

type TestViewModel = {
  value: number;
};

type TestViewState = {
  value: number;
};

class TestRenderer implements IRenderer {
  contextId: string = '';

  renderComponent(component: IComponent, properties: any | undefined): void {
    return;
  }

  isComponentAlive(component: IComponent): boolean {
    return true;
  }

  getRootComponent(): IComponent | undefined {
    return undefined;
  }

  getComponentChildren(component: IComponent): IComponent[] {
    return [];
  }

  getComponentParent(component: IComponent): IComponent | undefined {
    return undefined;
  }

  getCurrentComponentInstance(): IComponent | undefined {
    return undefined;
  }

  getComponentKey(component: IComponent): string {
    return '';
  }

  getComponentRootElements() {
    return [];
  }

  getComponentVirtualNode(component: IComponent): IRenderedVirtualNode {
    throw new Error('not implemented');
  }

  getRootVirtualNode() {
    return undefined;
  }

  getElementForId() {
    return undefined;
  }

  registerComponentDisposable() {
    return () => {};
  }

  batchUpdates(): void {}

  animate() {
    return 1;
  }

  cancelAnimation(token: number) {}

  onRenderComplete() {}

  onNextRenderComplete() {}

  addObserver() {}

  removeObserver() {}

  onUncaughtError() {}

  onLayoutComplete() {}

  setEventListener(eventListener: IRendererEventListener): void {}
  getEventListener(): IRendererEventListener | undefined {
    return undefined;
  }
}

class TestComponent extends LegacyVueComponent<TestViewModel, TestViewState> {
  state = <TestViewState>{
    value: 0,
  };

  renderCount = 0;
  onCreateCount = 0;
  onViewModelUpdateCount = 0;
  blockUpdates = false;

  onCreate() {
    const viewModel = exists(this.viewModel);

    this.onCreateCount++;
    // eslint-disable-next-line @snap/valdi/mutate-state-without-set-state
    this.state.value = viewModel.value;
  }

  onViewModelUpdate(): void {
    this.onViewModelUpdateCount++;
  }

  render() {
    super.render();
    this.renderCount++;
  }
}
LegacyVueComponent.setup(TestComponent, [], () => {});

describe('formatParameterizedString', () => {
  it('should fail', () => {
    expect(() => formatParameterizedString('hello $0')).toThrow('not enough args provided for string: hello $0');
  });
  it('should work', () => {
    expect(formatParameterizedString('hello')).toEqual('hello');
    expect(formatParameterizedString('hello $0, $1', 'world', 'friend')).toEqual('hello world, friend');
  });
});

describe('Component', () => {
  it('should incrementally update', done => {
    const tc = new TestComponent(new TestRenderer(), { value: 10 }, undefined);
    tc.state = { value: 0 };
    let count = 0;
    // Ensure that state updates, while asynchronous receive partial states to update from.
    for (let i = 0; i < 10; i++) {
      tc.setState({ value: i + 1 }, () => {
        count++;
        if (count === 10) {
          expect(tc.state.value).toEqual(10);
          done();
        }
      });
    }
  });

  it('should update after setState', done => {
    const tc = new TestComponent(new TestRenderer(), { value: 1 }, undefined);
    tc.render();

    const viewModel = exists(tc.viewModel);
    expect(viewModel.value).toEqual(1);
    expect(tc.renderCount).toEqual(1);

    tc.render();

    expect(tc.renderCount).toEqual(2);

    tc.setState({ value: 2 }, () => {
      expect(tc.renderCount).toEqual(3);
      expect(tc.onViewModelUpdateCount).toEqual(0);
      expect(tc.state.value).toEqual(2);

      // Apply same state
      tc.setState({ value: 2 }, () => {
        expect(tc.renderCount).toEqual(3);
        expect(tc.onViewModelUpdateCount).toEqual(0);
        expect(tc.state.value).toEqual(2);
        done();
      });
    });
  });

  it('should handle state changes when updated', done => {
    const tc = new TestComponent(new TestRenderer(), { value: 1 }, undefined);

    tc.onViewModelUpdate();
    tc.setState({ value: 1 }, () => {
      done();
    });
  });

  it('shouldComputeClassNames', done => {
    expect(classNames('hello')).toEqual('hello');
    expect(classNames('hello', 'goodbye')).toEqual('hello goodbye');
    expect(classNames('hello', { goodbye: false })).toEqual('hello');
    expect(classNames('hello', { goodbye: true })).toEqual('hello goodbye');
    expect(classNames({ hello: false }, { goodbye: true })).toEqual('goodbye');
    expect(classNames({ hello: false }, { goodbye: false })).toEqual('');
    expect(classNames(['hello', 'goodbye'])).toEqual('hello goodbye');
    expect(classNames(['hello', 'goodbye'], { nope: false })).toEqual('hello goodbye');
    expect(classNames(['hello', 'goodbye'], { nope: true })).toEqual('hello goodbye nope');

    done();
  });
});
