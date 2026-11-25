import 'jasmine/src/jasmine';
import { Component } from 'valdi_core/src/Component';
import { useInput } from '../src/App';
import { RenderInstance } from '../src/TerminalRenderer';
import { Text } from '../src/Text';
import { Key } from '../src/keys';
import { render, unmountAll } from './renderTracked';

class Live extends Component {
  onRender(): void {
    <Text value="live" />;
  }
}

interface Received {
  readonly input: string;
  readonly key: Key;
}

const received: Received[] = [];

class Recorder extends Component {
  onCreate(): void {
    useInput(this, (input, key) => received.push({ input, key }));
  }

  onRender(): void {
    <Text value="recording" />;
  }
}

function delay(ms: number): Promise<void> {
  return new Promise(resolve => {
    const timer = setTimeout(resolve, ms);
    return timer;
  });
}

async function waitFor(condition: () => boolean, description: string): Promise<void> {
  for (let attempt = 0; attempt < 200; attempt++) {
    if (condition()) {
      return;
    }
    await delay(10);
  }
  throw new Error(`Timed out waiting for ${description}`);
}

function renderLive(): RenderInstance<{}> {
  return render(Live, {}, { sink: 'memory', columns: 40, rows: 10 });
}

describe('console patching', () => {
  afterEach(unmountAll);

  it('keeps routing to the newer root when the older one unmounts first', async () => {
    const originalLog = console.log;
    const first = renderLive();
    const second = renderLive();
    await second.nextFrame();

    console.log('to second');
    await waitFor(() => second.output().includes('to second\n'), 'the log in the second root');
    expect(first.output()).not.toContain('to second');

    first.unmount();
    console.log('still second');
    await waitFor(() => second.output().includes('still second\n'), 'the log after the first root unmounted');

    second.unmount();
    expect(console.log).toBe(originalLog);
  });

  it('routes back to the older root when the newer one unmounts first', async () => {
    const originalLog = console.log;
    const first = renderLive();
    const second = renderLive();
    await second.nextFrame();

    second.unmount();
    console.log('back to first');
    await waitFor(() => first.output().includes('back to first\n'), 'the log in the first root');

    first.unmount();
    expect(console.log).toBe(originalLog);
  });

  it('leaves a patch installed after its own in place', () => {
    const originalLog = console.log;
    const instance = renderLive();
    const ours = console.log;
    const theirs = (...args: unknown[]): void => ours(...args);
    console.log = theirs;
    try {
      instance.unmount();
      expect(console.log).toBe(theirs);
    } finally {
      console.log = originalLog;
    }
  });
});

describe('keyboard input timing', () => {
  afterEach(() => {
    unmountAll();
    received.splice(0);
  });

  it('joins an escape sequence split across two reads', async () => {
    const instance = render(Recorder, {}, { sink: 'memory', columns: 40, rows: 10 });
    await instance.nextFrame();
    instance.feedInput('\x1b');
    instance.feedInput('[A');
    await waitFor(() => received.length > 0, 'the key press');
    await delay(100);
    expect(received.length).toBe(1);
    expect(received[0].key.upArrow).toBe(true);
    expect(received[0].key.escape).toBe(false);
  });

  it('delivers a lone ESC as Escape after the timeout', async () => {
    const instance = render(Recorder, {}, { sink: 'memory', columns: 40, rows: 10 });
    await instance.nextFrame();
    instance.feedInput('\x1b');
    await waitFor(() => received.length > 0, 'the Escape press');
    expect(received.length).toBe(1);
    expect(received[0].key.escape).toBe(true);
    expect(received[0].key.meta).toBe(false);
  });

  it('does not type the parameters of an unmapped key', async () => {
    const instance = render(Recorder, {}, { sink: 'memory', columns: 40, rows: 10 });
    await instance.nextFrame();
    instance.feedInput('\x1b[15~');
    instance.feedInput('\x1b[I');
    instance.feedInput('x');
    await waitFor(() => received.length > 0, 'the typed character');
    await delay(100);
    expect(received.map(press => press.input)).toEqual(['x']);
  });
});
