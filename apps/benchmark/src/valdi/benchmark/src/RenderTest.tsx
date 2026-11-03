import { Component } from 'valdi_core/src/Component';
import { IComponent } from 'valdi_core/src/IComponent';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageStatefulComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { Label, ScrollView } from 'valdi_tsx/src/NativeTemplateElements';
import { BenchmarkRunner, BenchmarkSingleRunComponent } from './BenchmarkSingleRunComponent';
import { now } from './cpp';

export interface ViewModel {}
export interface ComponentContext {}

class TestPage extends Component {
  onRender(): void {
    <scroll height="100%" width="100%" backgroundColor="lightblue">
      {[...Array(1000).keys()].map(i => {
        <label value={`Hello! ${i}`} />;
      })}
    </scroll>;
  }
}

@NavigationPage(module)
export class RenderTest extends NavigationPageStatefulComponent<ViewModel, ComponentContext> {
  state = {};

  benchmarkRunner: BenchmarkRunner = {
    start: () => {},
    incrementRunCount: () => {},
    measureAsync: (metricName: string) => {
      const start = now();
      return () => {
        const newState: Record<string, string> = { ...this.state, [metricName]: (now() - start).toFixed(2) };
        this.setState(newState);
      };
    },
    stop: () => {
      return {};
    },
  };

  onRender(): void {
    <view backgroundColor="white" flexDirection="column">
      <BenchmarkSingleRunComponent
        benchmarkRunner={this.benchmarkRunner}
        onComplete={this.onComplete}
        verifyCompleted={this.verifyCompleted}
      >
        <TestPage />;
      </BenchmarkSingleRunComponent>
      <label
        style={styles.bottom}
        value={`${Object.entries(this.state)
          .map(([key, value]) => `${key}:${value}`)
          .join(', ')} ms`}
      />
    </view>;
  }

  private verifyCompleted = (component: IComponent): boolean => {
    return !!component;
  };

  private onComplete = (): void => {};
}

const styles = {
  scroll: new Style<ScrollView>({
    alignItems: 'center',
    height: '100%',
  }),

  title: new Style<Label>({
    color: 'black',
    accessibilityCategory: 'header',
    width: '100%',
  }),

  bottom: new Style<Label>({
    marginTop: 'auto',
    marginLeft: '40',
    marginBottom: '10',
    font: systemFont(9),
    color: 'red',
  }),
};
