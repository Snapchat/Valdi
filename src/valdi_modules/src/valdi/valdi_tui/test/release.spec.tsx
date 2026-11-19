import 'jasmine/src/jasmine';
import { Component } from 'valdi_core/src/Component';
import { Box } from '../src/Box';
import { TerminalBackend, TerminalRootNative, getTerminalBackend } from '../src/TerminalBackend';
import { render as renderUntracked } from '../src/TerminalRenderer';
import { Text } from '../src/Text';
import { render, unmountAll } from './renderTracked';

class Throwing extends Component {
  onRender(): void {
    throw new Error('render failed');
  }
}

class PartiallyThrowing extends Component {
  onRender(): void {
    <Box flexDirection="column">
      <Text value="partial" />
      <Throwing />
    </Box>;
  }
}

interface GreetingViewModel {
  readonly name: string;
}

class Greeting extends Component<GreetingViewModel> {
  onRender(): void {
    <Text value={`Hello ${this.viewModel.name}`} />;
  }
}

type MutableBackend = { -readonly [K in keyof TerminalBackend]: TerminalBackend[K] };

/** Renders `PartiallyThrowing` and returns what the native root wrote and whether it was finished. */
function renderThrowing(isTTY: boolean): { output: string; finished: boolean } {
  const backend = getTerminalBackend() as MutableBackend;
  const originalCreate = backend.createRoot;
  const originalFinish = backend.finishRoot;
  let native: TerminalRootNative | undefined;
  let finished = false;
  backend.createRoot = options => {
    const root = originalCreate(options);
    native = root.native;
    return root;
  };
  backend.finishRoot = root => {
    finished = true;
    originalFinish(root);
  };
  let thrown: unknown;
  try {
    renderUntracked(PartiallyThrowing, {}, { sink: 'memory', columns: 40, rows: 10, isTTY });
  } catch (error) {
    thrown = error;
  } finally {
    backend.createRoot = originalCreate;
    backend.finishRoot = originalFinish;
  }
  expect(thrown instanceof Error).toBe(true);
  expect(native).toBeDefined();
  return { output: native !== undefined ? backend.getOutput(native) : '', finished };
}

describe('release', () => {
  afterEach(unmountAll);

  it('writes nothing for a render that throws when stdout is not a tty', () => {
    const { output, finished } = renderThrowing(false);
    expect(finished).toBe(false);
    expect(output).toBe('');
  });

  it('erases the partial frame of a render that throws and shows the cursor without a final newline', () => {
    const { output, finished } = renderThrowing(true);
    expect(finished).toBe(false);
    // The partial frame may or may not have been painted before the error, depending on when the main thread
    // got to it. Either nothing was written, or the frame was erased and the cursor shown again.
    if (output === '') {
      return;
    }
    const ERASE_LINE = '\x1b[2K';
    const SHOW_CURSOR = '\x1b[?25h';
    expect(output.endsWith(SHOW_CURSOR)).withContext(JSON.stringify(output)).toBe(true);
    const tail = output.slice(output.lastIndexOf('partial'));
    expect(tail.includes(ERASE_LINE)).toBe(true);
    expect(tail.slice(tail.lastIndexOf(ERASE_LINE)).includes('partial')).toBe(false);
  });

  it('keeps lastFrame() on the final frame painted by unmount()', async () => {
    const instance = render(Greeting, { name: 'first' }, { sink: 'memory', columns: 40, rows: 10 });
    for (let attempt = 0; attempt < 10; attempt++) {
      const frame = await instance.nextFrame();
      if (frame.join('\n').includes('Hello first')) {
        break;
      }
    }
    instance.rerender({ name: 'second' });
    instance.unmount();
    expect((instance.lastFrame() ?? []).join('\n')).toContain('Hello second');
  });
});
