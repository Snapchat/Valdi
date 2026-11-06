import 'jasmine/src/jasmine';
import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { getTerminalApp, useInput } from '../src/App';
import { Box } from '../src/Box';
import { Static } from '../src/Static';
import { MemoryTerminal } from '../src/Terminal';
import { render } from '../src/TerminalRenderer';
import { Text } from '../src/Text';

interface GreetingViewModel {
  readonly name: string;
}

class Greeting extends Component<GreetingViewModel> {
  onRender(): void {
    <Box flexDirection='column'>
      <Box borderStyle='single' paddingX={1}>
        <Text bold value={`Hello ${this.viewModel.name}`} />
      </Box>
      <Text value='The quick brown fox jumps over the lazy dog' />
    </Box>;
  }
}

interface CounterState {
  readonly count: number;
  readonly log: ReadonlyArray<string>;
}

class Counter extends StatefulComponent<{}, CounterState> {
  state: CounterState = { count: 0, log: [] };

  onCreate(): void {
    useInput(this, (input, key) => {
      if (key.upArrow) {
        this.setState({ count: this.state.count + 1 });
      } else if (input === 'l') {
        this.setState({ log: [...this.state.log, `entry ${this.state.log.length + 1}`] });
      } else if (input === 'q') {
        getTerminalApp().exit();
      }
    });
  }

  onRender(): void {
    <Box flexDirection='column'>
      <Static items={this.state.log} render={(entry: string) => <Text value={entry} />} />
      <Text value={`count: ${this.state.count}`} />
    </Box>;
  }
}

class Row extends Component {
  onRender(): void {
    <Box width={12} justifyContent='space-between'>
      <Text value='left' />
      <Text value='right' />
    </Box>;
  }
}

describe('render', () => {
  it('lays out boxes and wraps text to the terminal width', async () => {
    const terminal = new MemoryTerminal({ columns: 20, rows: 10 });
    const instance = render(Greeting, { name: 'Valdi' }, { terminal });
    try {
      const frame = await instance.nextFrame();
      // The bordered box stretches to the column width, as Yoga's default alignItems is stretch.
      expect(frame).toEqual([
        '┌──────────────────┐',
        '│ Hello Valdi      │',
        '└──────────────────┘',
        'The quick brown fox',
        'jumps over the lazy',
        'dog',
      ]);
      expect(terminal.rawMode).toBe(true);
    } finally {
      instance.unmount();
    }
    expect(terminal.rawMode).toBe(false);
    await instance.waitUntilExit();
  });

  it('re-renders with a new view model', async () => {
    const terminal = new MemoryTerminal({ columns: 40, rows: 10 });
    const instance = render(Greeting, { name: 'one' }, { terminal });
    try {
      await instance.nextFrame();
      const next = instance.nextFrame();
      instance.rerender({ name: 'two' });
      const frame = await next;
      expect(frame[1]).toBe('│ Hello two                            │');
    } finally {
      instance.unmount();
    }
  });

  it('distributes space with justifyContent', async () => {
    const terminal = new MemoryTerminal({ columns: 40, rows: 10 });
    const instance = render(Row, {}, { terminal });
    try {
      const frame = await instance.nextFrame();
      expect(frame).toEqual(['left   right']);
    } finally {
      instance.unmount();
    }
  });

  it('delivers keyboard input and static output', async () => {
    const terminal = new MemoryTerminal({ columns: 40, rows: 10 });
    const instance = render(Counter, {}, { terminal });
    try {
      expect(await instance.nextFrame()).toEqual(['count: 0']);

      let next = instance.nextFrame();
      terminal.sendInput('\x1b[A');
      expect(await next).toEqual(['count: 1']);

      next = instance.nextFrame();
      terminal.sendInput('l');
      await next;
      expect(terminal.output).toContain('entry 1\n');

      next = instance.nextFrame();
      terminal.sendInput('l');
      await next;
      const occurrences = terminal.output.split('entry 1\n').length - 1;
      expect(occurrences).toBe(1);
      expect(terminal.output).toContain('entry 2\n');

      terminal.sendInput('q');
      await instance.waitUntilExit();
    } finally {
      instance.unmount();
    }
  });

  it('exits on ctrl+c by default', async () => {
    const terminal = new MemoryTerminal({ columns: 40, rows: 10 });
    const instance = render(Counter, {}, { terminal });
    await instance.nextFrame();
    terminal.sendInput('\x03');
    await instance.waitUntilExit();
    expect(terminal.rawMode).toBe(false);
  });

  it('re-lays out after a resize', async () => {
    const terminal = new MemoryTerminal({ columns: 60, rows: 10 });
    const instance = render(Greeting, { name: 'x' }, { terminal });
    try {
      const first = await instance.nextFrame();
      expect(first.length).toBe(4);
      const next = instance.nextFrame();
      terminal.resize(20, 10);
      const frame = await next;
      expect(frame.length).toBe(6);
    } finally {
      instance.unmount();
    }
  });

  it('writes the final frame once when stdout is not a tty', async () => {
    const terminal = new MemoryTerminal({ columns: 40, rows: 10, isOutputTTY: false, isInputTTY: false });
    const instance = render(Row, {}, { terminal });
    await instance.nextFrame();
    expect(terminal.output).toBe('');
    instance.unmount();
    expect(terminal.output).toBe('left   right\n');
  });
});
