// valdi-tsx skill reference — compile check for core component patterns.
// This file must compile cleanly against the Valdi TypeScript compiler.
// Run: bzl build //ai-skills/skills/valdi-tsx/tests:tests

import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { createProviderComponentWithKeyName } from 'valdi_core/src/provider/createProvider';
import { ProvidersValuesViewModel, withProviders } from 'valdi_core/src/provider/withProviders';
import { Label, Layout, View } from 'valdi_tsx/src/NativeTemplateElements';

// ─── Module-level styles (NOT inside onRender) ──────────────────────────────

const styles = {
  card: new Style<View>({
    backgroundColor: '#fff',
    borderRadius: 8,
    padding: 12,
    flexDirection: 'column',
  }),
  spacer: new Style<Layout>({ height: 8 }),
  title: new Style<Label>({
    color: '#000',
    font: systemFont(16),  // Label uses font string, NOT fontSize
  }),
};

// ─── Stateless component ─────────────────────────────────────────────────────

interface GreetingViewModel {
  name: string;
}

class Greeting extends Component<GreetingViewModel> {
  onRender(): void {
    // onRender returns void — no return statement
    <label value={`Hello, ${this.viewModel.name}`} />;
  }
}

// ─── Stateful component with full lifecycle ───────────────────────────────────

interface CounterViewModel {
  label: string;
}

interface CounterState {
  count: number;
  inputText: string;
}

class Counter extends StatefulComponent<CounterViewModel, CounterState> {
  state: CounterState = { count: 0, inputText: '' };

  onCreate(): void {
    // Timer auto-cancels on destroy
    this.setTimeoutDisposable(() => {
      this.setState({ count: 1 });
    }, 5000);
  }

  onViewModelUpdate(previous?: CounterViewModel): void {
    if (this.viewModel.label !== previous?.label) {
      this.setState({ count: 0 });
    }
  }

  onDestroy(): void {
    // cleanup
  }

  // Class arrow functions — never inline lambdas in JSX props
  private handleIncrement = (_event: unknown): void => {
    this.setState({ count: this.state.count + 1 });
  };

  private handleTextChange = (event: { text: string }): void => {
    this.setState({ inputText: event.text });
  };

  onRender(): void {
    <view style={styles.card}>
      <label value={`${this.viewModel.label}: ${this.state.count}`} />;
      <layout style={styles.spacer} />;
      <textfield
        value={this.state.inputText}
        onChange={this.handleTextChange}
      />;
      <label value="Increment" onTap={this.handleIncrement} />;
    </view>;
  }
}

// ─── List rendering with forEach ─────────────────────────────────────────────

interface ItemViewModel {
  id: string;
  title: string;
}

interface ItemListViewModel {
  items: ItemViewModel[];
}

class ItemList extends Component<ItemListViewModel> {
  onRender(): void {
    <scroll>
      {this.viewModel.items.forEach(item => {
        // Stable key from data ID — not array index
        <view key={item.id}>
          <label value={item.title} />;
        </view>;
      })}
    </scroll>;
  }
}

// ─── Platform detection ──────────────────────────────────────────────────────

interface MediaPlayerViewModel {
  videoUrl: string;
}

class MediaPlayer extends Component<MediaPlayerViewModel> {
  onRender(): void {
    <view>
      {Device.isIOS() && <label value="iOS player" />}
      {Device.isAndroid() && <label value="Android player" />}
      {Device.isDesktop() && <label value="macOS player" />}
    </view>;
  }
}

// ─── Provider pattern ────────────────────────────────────────────────────────

class ThemeService {
  primaryColor = '#FFFC00';
  fontSize = 16;
}

const ThemeProvider = createProviderComponentWithKeyName<ThemeService>('ThemeProvider');

interface ThemedViewModel extends ProvidersValuesViewModel<[ThemeService]> {
  text: string;
}

class ThemedLabel extends Component<ThemedViewModel> {
  onRender(): void {
    const [theme] = this.viewModel.providersValues;
    <label value={this.viewModel.text} />;
    void theme; // reference theme to satisfy compiler
  }
}

const ThemedLabelWithProvider = withProviders(ThemeProvider)(ThemedLabel);

class AppRoot extends Component {
  private theme = new ThemeService();

  onRender(): void {
    <ThemeProvider value={this.theme}>
      <ThemedLabelWithProvider text="Hello" />;
    </ThemeProvider>;
  }
}

// ─── @ExportModel ViewModel: only primitives, no type aliases ────────────────

type Direction = 'UP' | 'DOWN' | 'LEFT' | 'RIGHT';

/**
 * @ViewModel
 * @ExportModel
 */
interface GameViewModel {}  // Don't put Direction here — it can't be exported

interface GameState {
  direction: Direction;     // Type aliases are fine in State (not exported)
  score: number;
}

class GameComponent extends StatefulComponent<GameViewModel, GameState> {
  state: GameState = { direction: 'RIGHT', score: 0 };

  onRender(): void {
    <label value={`Direction: ${this.state.direction}, Score: ${this.state.score}`} />;
  }
}

// Suppress unused-variable warnings for top-level declarations
void Counter;
void ItemList;
void Greeting;
void MediaPlayer;
void AppRoot;
void GameComponent;
