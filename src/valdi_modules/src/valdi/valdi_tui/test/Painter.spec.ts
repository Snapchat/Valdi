import 'jasmine/src/jasmine';
import { ColorLevel } from '../src/ansi';
import { PaintNode, TUI_BOX_CLASS, TUI_STATIC_CLASS, TUI_TEXT_CLASS, paintTree } from '../src/Painter';

function text(x: number, y: number, width: number, height: number, value: string, extra: object = {}): PaintNode {
  return {
    viewClass: TUI_TEXT_CLASS,
    frame: { x, y, width, height },
    attributes: { tuiRuns: [{ text: value }], ...extra },
    children: [],
  };
}

function box(frame: PaintNode['frame'], attributes: object, children: PaintNode[]): PaintNode {
  return { viewClass: TUI_BOX_CLASS, frame, attributes, children };
}

describe('paintTree', () => {
  it('draws borders and nested text in parent coordinates', () => {
    const tree = box({ x: 0, y: 0, width: 9, height: 3 }, { tuiBorderStyle: 'single' }, [text(1, 1, 7, 1, 'hello')]);
    const { screen } = paintTree(tree, 9, 3);
    expect(screen.toLines(ColorLevel.None)).toEqual(['┌───────┐', '│hello  │', '└───────┘']);
  });

  it('supports partial borders and round style', () => {
    const tree = box({ x: 0, y: 0, width: 5, height: 2 }, { tuiBorderStyle: 'round', tuiBorderBottom: false }, []);
    expect(paintTree(tree, 5, 2).screen.toLines(ColorLevel.None)).toEqual(['╭───╮', '│   │']);
  });

  it('wraps text to its frame', () => {
    const { screen } = paintTree(text(0, 0, 5, 2, 'hello world'), 5, 2);
    expect(screen.toLines(ColorLevel.None)).toEqual(['hello', 'world']);
  });

  it('clips children when overflow is hidden', () => {
    const tree = box({ x: 0, y: 0, width: 3, height: 1 }, { overflow: 'hidden' }, [text(0, 0, 10, 1, 'abcdef')]);
    expect(paintTree(tree, 10, 1).screen.toLines(ColorLevel.None)).toEqual(['abc']);
  });

  it('renders static content separately', () => {
    const staticNode: PaintNode = {
      viewClass: TUI_STATIC_CLASS,
      frame: { x: 0, y: 0, width: 10, height: 2 },
      attributes: {},
      children: [text(0, 0, 10, 1, 'one'), text(0, 1, 10, 1, 'two')],
    };
    const tree = box({ x: 0, y: 0, width: 10, height: 1 }, {}, [staticNode, text(0, 0, 10, 1, 'live')]);
    const painted = paintTree(tree, 10, 1);
    expect(painted.screen.toLines(ColorLevel.None)).toEqual(['live']);
    expect(painted.staticScreens.length).toBe(1);
    expect(painted.staticScreens[0]!.toLines(ColorLevel.None)).toEqual(['one', 'two']);
  });

  it('applies transforms to output lines', () => {
    const tree = box({ x: 0, y: 0, width: 5, height: 1 }, { tuiTransform: (line: string) => line.toUpperCase() }, [
      text(0, 0, 5, 1, 'abc'),
    ]);
    expect(paintTree(tree, 5, 1).screen.toLines(ColorLevel.None)).toEqual(['ABC']);
  });

  it('paints backgrounds and skips display none', () => {
    const tree = box({ x: 0, y: 0, width: 3, height: 1 }, { backgroundColor: 'blue' }, [
      box({ x: 0, y: 0, width: 3, height: 1 }, { display: 'none', backgroundColor: 'red' }, []),
    ]);
    expect(paintTree(tree, 3, 1).screen.toLines(ColorLevel.Basic)).toEqual(['\x1b[44m   \x1b[0m']);
  });

  it('renders plain labels from their value', () => {
    const label: PaintNode = {
      viewClass: 'SCValdiLabel',
      frame: { x: 0, y: 0, width: 5, height: 1 },
      attributes: { value: 'label', color: 'red' },
      children: [],
    };
    expect(paintTree(label, 5, 1).screen.toLines(ColorLevel.Basic)).toEqual(['\x1b[31mlabel\x1b[0m']);
  });
});
