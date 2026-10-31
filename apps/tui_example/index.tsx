import { StatefulComponent } from 'valdi_core/src/Component';
import { getStandaloneRuntime } from 'valdi_standalone/src/ValdiStandalone';
import { Box } from 'valdi_tui/src/Box';
import { Newline } from 'valdi_tui/src/Newline';
import { Spacer } from 'valdi_tui/src/Spacer';
import { Static } from 'valdi_tui/src/Static';
import { Text } from 'valdi_tui/src/Text';
import { Transform } from 'valdi_tui/src/Transform';
import { getTerminalApp, useInput } from 'valdi_tui/src/App';
import { render } from 'valdi_tui/src/TerminalRenderer';

const SPINNER_FRAMES = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏'];

interface Task {
  readonly title: string;
  readonly detail: string;
  readonly done: boolean;
}

const TASKS: ReadonlyArray<Task> = [
  { title: 'Build runtime', detail: 'Compiles the C++ runtime with QuickJS and the Yoga layout engine.', done: true },
  { title: 'Compile module', detail: 'Turns TypeScript components into a .valdimodule bundle.', done: true },
  {
    title: 'Render frame',
    detail: 'Measures text in terminal cells, lays out the tree with flexbox and paints ANSI output.',
    done: false,
  },
  { title: 'Ship it', detail: 'Type a note and press Enter to log it. Esc quits.', done: false },
];

interface LogEntry {
  readonly id: number;
  readonly text: string;
}

interface DemoViewModel {
  readonly title: string;
}

interface DemoState {
  readonly tick: number;
  readonly selected: number;
  readonly input: string;
  readonly log: ReadonlyArray<LogEntry>;
}

class Demo extends StatefulComponent<DemoViewModel, DemoState> {
  state: DemoState = { tick: 0, selected: 0, input: '', log: [{ id: 0, text: 'ready.' }] };
  private nextLogId = 1;

  onCreate(): void {
    const timer = setInterval(() => this.setState({ tick: this.state.tick + 1 }), 120);
    this.registerDisposable(() => clearInterval(timer));

    useInput(this, (input, key) => {
      if (key.escape) {
        getTerminalApp().exit();
      } else if (key.upArrow) {
        this.setState({ selected: (this.state.selected + TASKS.length - 1) % TASKS.length });
      } else if (key.downArrow) {
        this.setState({ selected: (this.state.selected + 1) % TASKS.length });
      } else if (key.return) {
        if (this.state.input) {
          this.appendLog(`note: ${this.state.input}`);
          this.setState({ input: '' });
        } else {
          this.appendLog(`selected "${TASKS[this.state.selected]!.title}"`);
        }
      } else if (key.backspace || key.delete) {
        this.setState({ input: this.state.input.slice(0, -1) });
      } else if (input && !key.ctrl && !key.meta) {
        this.setState({ input: this.state.input + input });
      }
    });
  }

  private appendLog(text: string): void {
    this.setState({ log: [...this.state.log, { id: this.nextLogId++, text }] });
  }

  onRender(): void {
    const { tick, selected, input, log } = this.state;
    const spinner = SPINNER_FRAMES[tick % SPINNER_FRAMES.length]!;
    const task = TASKS[selected]!;

    <Box flexDirection='column' width={80}>
      <Static items={log} render={(entry: LogEntry) => <Text dimColor value={`• ${entry.text}`} />} />

      <Box borderStyle='round' borderColor='cyan' paddingX={1} justifyContent='space-between'>
        <Text bold value={this.viewModel.title} />
        <Text value={[{ text: spinner, color: 'yellow' }, ' ', { text: `tick ${tick}`, dimColor: true }]} />
      </Box>

      <Box gap={1}>
        <Box flexDirection='column' borderStyle='single' paddingX={1} width={24} flexShrink={0}>
          <Text underline value='Tasks' />
          <Newline />
          {TASKS.map((item, index) => (
            <Text
              key={item.title}
              inverse={index === selected}
              color={item.done ? 'green' : undefined}
              value={`${item.done ? '✔' : ' '} ${item.title}`}
            />
          ))}
        </Box>
        <Box flexDirection='column' borderStyle='single' borderDimColor paddingX={1} flexGrow={1}>
          <Transform transform={(line: string) => line.toUpperCase()}>
            <Text bold value={task.title} />
          </Transform>
          <Newline />
          <Text value={task.detail} />
          <Spacer />
          <Text dimColor value='↑/↓ move · type a note · enter log · esc quit' />
        </Box>
      </Box>

      <Box borderStyle='round' borderDimColor paddingX={1}>
        <Text value={[{ text: '> ', color: 'cyan' }, input, { text: '▌', color: 'cyan' }]} />
      </Box>
    </Box>;
  }
}

const standalone = getStandaloneRuntime();
const once = standalone.arguments.indexOf('--once') >= 0;

const instance = render(Demo, { title: 'Valdi TUI example' });

if (once) {
  // Smoke-test mode: print the first frame and leave.
  void instance.nextFrame().then(() => instance.unmount());
}

void instance.waitUntilExit().then(
  () => standalone.exit(0),
  error => {
    console.error(error);
    standalone.exit(1);
  },
);
