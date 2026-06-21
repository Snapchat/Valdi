import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { IWebViewNativeController } from 'valdi_tsx/src/NativeTemplateElements';
import 'jasmine/src/jasmine';
import { createComponent, makeComponentTest } from './JSXTestUtils';

describe('JSX', () => {
  it('can render a basic component', async () => {
    class BasicComponent extends Component {
      onRender() {
        <layout>
          <view backgroundColor='red'>
            <label value='Hello world' />
            <image />
          </view>
        </layout>;
      }
    }

    const component = createComponent(BasicComponent);
    const rootNode = await component.getRenderedNode();
    expect(rootNode.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiView',
          attributes: {
            backgroundColor: 'red',
          },
          children: [
            {
              viewClass: 'SCValdiLabel',
              attributes: {
                value: 'Hello world',
              },
            },
            {
              viewClass: 'SCValdiImageView',
              attributes: {},
            },
          ],
        },
      ],
    });
  });

  it('can render a webview element', async () => {
    class WebViewComponent extends Component {
      private controller: IWebViewNativeController = {};

      onRender() {
        <webview controller={this.controller} />;
      }
    }

    const component = createComponent(WebViewComponent);
    const componentNode = component.getComponent().renderer.getComponentVirtualNode(component.getComponent());
    const webViewElement = componentNode.children[0].element!;

    expect(webViewElement.viewClass).toEqual('SCValdiWebView');
    expect(webViewElement.getAttribute('controller')).toBeDefined();
  });

  it('can render a component with view model', async () => {
    interface ViewModel {
      text: string;
    }

    class BasicComponentWithViewModel extends Component<ViewModel> {
      onRender() {
        <view>
          <label value={this.viewModel.text} />
        </view>;
      }
    }

    const component = createComponent(BasicComponentWithViewModel, { text: 'nice' });
    const rootNode = await component.getRenderedNode();

    expect(rootNode.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'nice',
          },
        },
      ],
    });
  });

  it('can update view model', async () => {
    interface ViewModel {
      text: string;
    }

    class BasicComponentWithViewModel extends Component<ViewModel> {
      onRender() {
        <view>
          <label value={this.viewModel.text} />;
        </view>;
      }
    }

    const component = createComponent(BasicComponentWithViewModel, { text: '1' });

    const rootNode = await component.getRenderedNode();

    expect(rootNode.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: '1',
          },
        },
      ],
    });

    component.setViewModel({ text: '2' });

    const rootNode2 = await component.getRenderedNode();

    expect(rootNode2.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: '2',
          },
        },
      ],
    });
  });

  it('can render child component', async () => {
    class ChildComponent extends Component {
      onRender() {
        <layout>
          <view />
          <label />
        </layout>;
      }
    }

    class ParentComponent extends Component {
      onRender() {
        <view>
          <ChildComponent />
        </view>;
      }
    }

    const component = createComponent(ParentComponent);

    const rootNode = await component.getRenderedNode();

    expect(rootNode.simplify(['viewClass'])).toEqual({
      viewClass: 'SCValdiView',
      children: [
        {
          viewClass: 'Layout',
          children: [
            {
              viewClass: 'SCValdiView',
            },
            {
              viewClass: 'SCValdiLabel',
            },
          ],
        },
      ],
    });
  });

  it('can pass view model to child', async () => {
    interface ChildViewModel {
      text: string;
    }
    class ChildComponent extends Component<ChildViewModel> {
      onRender() {
        <label value={this.viewModel.text} />;
      }
    }

    class ParentComponent extends Component {
      onRender() {
        <view>
          <ChildComponent text={'good'} />
        </view>;
      }
    }

    const component = createComponent(ParentComponent);

    const rootNode = await component.getRenderedNode();

    expect(rootNode.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'good',
          },
        },
      ],
    });
  });

  it('can render for each', async () => {
    interface ViewModel {
      values: string[];
    }
    class ForEachComponent extends Component<ViewModel> {
      renderCards() {
        for (const value of this.viewModel.values) {
          <label value={value} />;
        }
      }

      onRender() {
        <layout>{this.renderCards()}</layout>;
      }
    }

    const component = createComponent(ForEachComponent, {
      values: ['1', '2', '3'],
    });

    const rootNode = await component.getRenderedNode();

    expect(rootNode.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: '1',
          },
        },
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: '2',
          },
        },
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: '3',
          },
        },
      ],
    });
  });

  it('can pass component key', async () => {
    let componentNumber = 0;

    class Child extends Component<{}> {
      private id = 0;

      onCreate() {
        this.id = ++componentNumber;
      }

      onRender() {
        <label value={`Id ${this.id}`} />;
      }
    }

    interface ViewModel {
      keys: string[];
    }

    class Parent extends Component<ViewModel> {
      onRender() {
        <view>
          {this.viewModel.keys.forEach(key => {
            <Child key={key} />;
          })}
        </view>;
      }
    }

    const component = createComponent(Parent, { keys: ['One', 'Two', 'Three'] });

    let rootNode = await component.getRenderedNode();

    expect(rootNode.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'Id 1',
          },
        },
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'Id 2',
          },
        },
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'Id 3',
          },
        },
      ],
    });

    component.setViewModel({ keys: ['Two', 'Three', 'One'] });
    rootNode = await component.getRenderedNode();

    expect(rootNode.simplify(['viewClass', 'attributes'])).toEqual({
      viewClass: 'SCValdiView',
      attributes: {},
      children: [
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'Id 2',
          },
        },
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'Id 3',
          },
        },
        {
          viewClass: 'SCValdiLabel',
          attributes: {
            value: 'Id 1',
          },
        },
      ],
    });
  });

  describe('makeComponentTest', () => {
    it('should throw error if component render throws error', async () => {
      class BasicComponent extends StatefulComponent<{ onComplete: () => void }, {}, { error: boolean }> {
        state = { error: false };

        onCreate(): void {
          this.setTimeoutDisposable(() => {
            this.setState({ error: true });
            this.viewModel.onComplete();
          }, 0);
        }
        onRender() {
          if (this.state.error) {
            throw new Error('Error');
          }

          <layout />;
        }
      }

      const test = makeComponentTest(async driver => {
        await new Promise<void>((resolve, reject) => {
          try {
            driver.renderComponent(BasicComponent, { onComplete: resolve }, {});
          } catch (e) {
            reject(e);
          }
        });
      });

      await expectAsync(test()).toBeRejected();
    });
  });
});
