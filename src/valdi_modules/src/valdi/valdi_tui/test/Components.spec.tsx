import 'jasmine/src/jasmine';
import { Component } from 'valdi_core/src/Component';
import { Box } from '../src/Box';
import { RenderInstance, render } from '../src/TerminalRenderer';
import { Text } from '../src/Text';
import { Transform } from '../src/Transform';
import { ColorLevel } from '../src/ansi';

type Lines = ReadonlyArray<string>;

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

class Truncated extends Component {
  onRender(): void {
    <Box width={10}>
      <Text wrap="truncate-end" value="hello wonderful world" />
    </Box>;
  }
}

class OpenTop extends Component {
  onRender(): void {
    <Box borderStyle="round" borderTop={false} width={6}>
      <Text value="ab" />
    </Box>;
  }
}

// The root element must be a Box: the root view is a plain view, which cannot take a label's text.
const STATUS_VALUE = ['Status: ', { text: 'ok', color: 'green', bold: true }];

function shout(line: string): string {
  return line.toUpperCase();
}

class Spans extends Component {
  onRender(): void {
    <Box>
      <Text value={STATUS_VALUE} />
    </Box>;
  }
}

class Shouting extends Component {
  onRender(): void {
    <Box>
      <Transform transform={shout} value="quiet" />
    </Box>;
  }
}

class Logging extends Component {
  onRender(): void {
    <Box>
      <Text value="live" />
    </Box>;
  }
}

describe('components', () => {
  it('truncates text instead of wrapping', async () => {
    const instance = render(Truncated, {}, { sink: 'memory', columns: 40, rows: 10 });
    try {
      expect(await frameMatching(instance, isSettled)).toEqual(['hello won…']);
    } finally {
      instance.unmount();
    }
  });

  it('omits disabled border sides and gives their cells back to the layout', async () => {
    const instance = render(OpenTop, {}, { sink: 'memory', columns: 40, rows: 10 });
    try {
      expect(await frameMatching(instance, isSettled)).toEqual(['│ab  │', '╰────╯']);
    } finally {
      instance.unmount();
    }
  });

  it('renders spans inline and styles them at the sink color level', async () => {
    const instance = render(Spans, {}, { sink: 'memory', columns: 40, rows: 10, colorLevel: ColorLevel.TrueColor });
    try {
      expect(await frameMatching(instance, isSettled)).toEqual(['Status: ok']);
      const output = instance.output();
      expect(output).toContain('\x1b[1;38;2;0;205;0mok');
    } finally {
      instance.unmount();
    }
  });

  it('transforms the text of its value', async () => {
    const instance = render(Shouting, {}, { sink: 'memory', columns: 40, rows: 10 });
    try {
      expect(await frameMatching(instance, isSettled)).toEqual(['QUIET']);
    } finally {
      instance.unmount();
    }
  });

  it('writes console output above the live frame', async () => {
    const instance = render(Logging, {}, { sink: 'memory', columns: 40, rows: 10 });
    try {
      await frameMatching(instance, isSettled);
      const next = instance.nextFrame();
      console.log('hello', 'there');
      await next;
      expect(instance.output()).toContain('hello there\n');
    } finally {
      instance.unmount();
    }
  });
});
