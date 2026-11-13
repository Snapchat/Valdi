import 'jasmine/src/jasmine';
import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { getTerminalApp, useInput } from '../src/App';
import { Box } from '../src/Box';
import { Static } from '../src/Static';
import { getTerminalBackend } from '../src/TerminalBackend';
import { RenderInstance, render as renderUntracked, wantsRawMode } from '../src/TerminalRenderer';
import { Text } from '../src/Text';
import { render, unmountAll } from './renderTracked';

interface GreetingViewModel {
  readonly name: string;
}

class Greeting extends Component<GreetingViewModel> {
  onRender(): void {
    <Box flexDirection="column">
      <Box borderStyle="single" paddingX={1}>
        <Text bold value={`Hello ${this.viewModel.name}`} />
      </Box>
      <Text value="The quick brown fox jumps over the lazy dog" />
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
    <Box flexDirection="column">
      <Static items={this.state.log} render={this.renderEntry} />
      <Text value={`count: ${this.state.count}`} />
    </Box>;
  }

  private readonly renderEntry = (entry: string): void => {
    <Text value={entry} />;
  };
}

class Row extends Component {
  onRender(): void {
    <Box width={12} justifyContent="space-between">
      <Text value="left" />
      <Text value="right" />
    </Box>;
  }
}

class Throwing extends Component {
  onRender(): void {
    throw new Error('render failed');
  }
}

type Lines = ReadonlyArray<string>;

// Frames can be painted between a change and its final layout, so a test waits for the frame it
// expects instead of taking whichever comes next. Call it before making the change.
async function frameMatching(instance: RenderInstance<object>, accept: (frame: Lines) => boolean): Promise<Lines> {
  for (let attempt = 0; attempt < 10; attempt++) {
    const frame = await instance.nextFrame();
    if (accept(frame)) {
      return frame;
    }
  }
  throw new Error('No frame matched the expectation');
}

const isSettled = (frame: Lines): boolean => frame.length > 0;

describe('wantsRawMode', () => {
  it('needs a terminal on both stdin and stdout', () => {
    expect(wantsRawMode({ isInputTTY: true, isOutputTTY: true })).toBe(true);
    expect(wantsRawMode({ isInputTTY: true, isOutputTTY: false })).toBe(false);
    expect(wantsRawMode({ isInputTTY: false, isOutputTTY: true })).toBe(false);
    expect(wantsRawMode({ isInputTTY: false, isOutputTTY: false })).toBe(false);
  });
});

describe('render', () => {
  afterEach(unmountAll);

  it('lays out boxes and wraps text to the terminal width', async () => {
    const instance = render(Greeting, { name: 'Valdi' }, { sink: 'memory', columns: 20, rows: 10 });
    try {
      const frame = await frameMatching(instance, isSettled);
      // The bordered box stretches to the column width, as Yoga's default alignItems is stretch.
      expect(frame).toEqual([
        '┌──────────────────┐',
        '│ Hello Valdi      │',
        '└──────────────────┘',
        'The quick brown fox',
        'jumps over the lazy',
        'dog',
      ]);
    } finally {
      instance.unmount();
    }
    await instance.waitUntilExit();
  });

  it('re-renders with a new view model', async () => {
    const instance = render(Greeting, { name: 'one' }, { sink: 'memory', columns: 40, rows: 10 });
    try {
      await frameMatching(instance, frame => frame[1]?.includes('Hello one') === true);
      const next = frameMatching(instance, frame => frame[1]?.includes('Hello two') === true);
      instance.rerender({ name: 'two' });
      const frame = await next;
      expect(frame[1]).toBe('│ Hello two                            │');
    } finally {
      instance.unmount();
    }
  });

  it('distributes space with justifyContent', async () => {
    const instance = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    try {
      const frame = await frameMatching(instance, isSettled);
      expect(frame).toEqual(['left   right']);
    } finally {
      instance.unmount();
    }
  });

  it('delivers keyboard input and static output', async () => {
    const instance = render(Counter, {}, { sink: 'memory', columns: 40, rows: 10 });
    try {
      expect(await frameMatching(instance, isSettled)).toEqual(['count: 0']);

      let next = frameMatching(instance, frame => frame[0] === 'count: 1');
      instance.feedInput('\x1b[A');
      expect(await next).toEqual(['count: 1']);

      next = frameMatching(instance, () => instance.output().includes('entry 1\n'));
      instance.feedInput('l');
      await next;
      expect(instance.output()).toContain('entry 1\n');

      next = frameMatching(instance, () => instance.output().includes('entry 2\n'));
      instance.feedInput('l');
      await next;
      const occurrences = instance.output().split('entry 1\n').length - 1;
      expect(occurrences).toBe(1);
      expect(instance.output()).toContain('entry 2\n');

      instance.feedInput('q');
      await instance.waitUntilExit();
    } finally {
      instance.unmount();
    }
  });

  it('exits on ctrl+c by default', async () => {
    const instance = render(Counter, {}, { sink: 'memory', columns: 40, rows: 10 });
    await frameMatching(instance, isSettled);
    instance.feedInput('\x03');
    await instance.waitUntilExit();
  });

  it('re-lays out after a resize', async () => {
    const instance = render(Greeting, { name: 'x' }, { sink: 'memory', columns: 60, rows: 10 });
    try {
      const first = await frameMatching(instance, isSettled);
      expect(first.length).toBe(4);
      const next = frameMatching(instance, frame => frame.length === 6);
      instance.setSize(20, 10);
      const frame = await next;
      expect(frame.length).toBe(6);
    } finally {
      instance.unmount();
    }
  });

  it('writes the final frame once when stdout is not a tty', async () => {
    const instance = render(Row, {}, { sink: 'memory', columns: 40, rows: 10, isTTY: false });
    await frameMatching(instance, isSettled);
    expect(instance.output()).toBe('');
    instance.unmount();
    expect(instance.output()).toBe('left   right\n');
  });

  it('delivers the first frame already laid out', async () => {
    const instance = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    expect(await instance.nextFrame()).toEqual(['left   right']);
  });

  it('keeps the newest application active when an older one unmounts', () => {
    const first = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    const second = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    expect(getTerminalApp()).toBe(second.app);
    first.unmount();
    expect(getTerminalApp()).toBe(second.app);
    second.unmount();
    expect(() => getTerminalApp()).toThrow();
  });

  it('restores the previous application when the newest one unmounts', () => {
    const first = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    const second = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    second.unmount();
    expect(getTerminalApp()).toBe(first.app);
  });

  it('rejects nextFrame() after unmount', async () => {
    const instance = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    await frameMatching(instance, isSettled);
    instance.unmount();
    let rejection: unknown;
    try {
      await instance.nextFrame();
    } catch (error) {
      rejection = error;
    }
    expect(rejection instanceof Error).toBe(true);
  });

  it('resolves a pending nextFrame() with the last frame when unmounting', async () => {
    const instance = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    await frameMatching(instance, isSettled);
    const pending = instance.nextFrame();
    instance.unmount();
    expect(await pending).toEqual(['left   right']);
  });

  it('keeps input and resizes of one render away from another', async () => {
    const first = render(Counter, {}, { sink: 'memory', columns: 40, rows: 10 });
    const second = render(Counter, {}, { sink: 'memory', columns: 40, rows: 10 });
    await frameMatching(first, isSettled);
    await frameMatching(second, isSettled);

    const next = frameMatching(first, frame => frame[0] === 'count: 1');
    first.feedInput('\x1b[A');
    await next;
    expect(second.lastFrame()).toEqual(['count: 0']);

    first.setSize(20, 5);
    expect(first.app.terminal.columns).toBe(20);
    expect(second.app.terminal.columns).toBe(40);
  });

  it('finishes unmounting and rejects waitUntilExit when a release step throws', async () => {
    const instance = render(Row, {}, { sink: 'memory', columns: 40, rows: 10 });
    await frameMatching(instance, isSettled);
    const backend = getTerminalBackend() as { finishRoot: (native: unknown) => void };
    const originalFinish = backend.finishRoot;
    backend.finishRoot = () => {
      throw new Error('finish failed');
    };
    let thrown: unknown;
    try {
      instance.unmount();
    } catch (error) {
      thrown = error;
    } finally {
      backend.finishRoot = originalFinish;
    }
    expect(thrown instanceof Error).toBe(true);
    expect(() => getTerminalApp()).toThrow();
    let rejection: unknown;
    try {
      await instance.waitUntilExit();
    } catch (error) {
      rejection = error;
    }
    expect(rejection).toBe(thrown);
  });

  describe('when the root component throws', () => {
    interface TimerGlobals {
      setInterval: (...args: unknown[]) => unknown;
      clearInterval: (id: unknown) => void;
    }

    it('undoes the keep-alive, console patch and active application, and rethrows', () => {
      const globals = globalThis as unknown as TimerGlobals;
      const originalSet = globals.setInterval;
      const originalClear = globals.clearInterval;
      const pending = new Set<unknown>();
      globals.setInterval = (...args: unknown[]) => {
        const id = originalSet(...args);
        pending.add(id);
        return id;
      };
      globals.clearInterval = (id: unknown) => {
        pending.delete(id);
        originalClear(id);
      };
      const originalLog = console.log;
      let thrown: unknown;
      try {
        renderUntracked(Throwing, {}, { sink: 'memory', columns: 40, rows: 10 });
      } catch (error) {
        thrown = error;
      } finally {
        globals.setInterval = originalSet;
        globals.clearInterval = originalClear;
      }
      for (const id of pending) {
        originalClear(id);
      }
      expect(thrown instanceof Error).toBe(true);
      expect(console.log).toBe(originalLog);
      expect(() => getTerminalApp()).toThrow();
      expect(pending.size).toBe(0);
    });
  });
});
