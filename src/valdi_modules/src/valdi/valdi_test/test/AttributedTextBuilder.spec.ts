import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributedTextEntryType } from 'valdi_tsx/src/AttributedText';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import 'jasmine/src/jasmine';

describe('AtributedTextBuilder', () => {
  it('can be empty', () => {
    const output = new AttributedTextBuilder().build();
    expect(output).toEqual([]);
  });

  it('can pass multiple strings', () => {
    const output = new AttributedTextBuilder().appendText('Hello').appendText(' ').appendText('World').build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello',
      AttributedTextEntryType.Content,
      ' ',
      AttributedTextEntryType.Content,
      'World',
    ]);
  });

  it('can push font', () => {
    const output = new AttributedTextBuilder().append('Hello ').pushFont('title').append('World').pop().build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushFont,
      'title',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can push text decoration', () => {
    const output = new AttributedTextBuilder()
      .append('Hello ')
      .pushTextDecoration('dashed-underline')
      .append('World')
      .pop()
      .build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushTextDecoration,
      'dashed-underline',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can push dotted text decoration', () => {
    const output = new AttributedTextBuilder()
      .append('Hello ')
      .pushTextDecoration('dotted-underline')
      .append('World')
      .pop()
      .build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushTextDecoration,
      'dotted-underline',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can push color', () => {
    const output = new AttributedTextBuilder().append('Hello ').pushColor('red').append('World').pop().build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushColor,
      'red',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can append an inline view attachment', () => {
    const output = new AttributedTextBuilder().append('Before ').appendInlineView(1).append(' after').build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Before ',
      AttributedTextEntryType.InlineView,
      { childIndex: 1, verticalAlignment: AttributedTextInlineViewVerticalAlignment.Center },
      AttributedTextEntryType.Content,
      ' after',
    ]);
  });

  it('can append an inline image attachment', () => {
    const imageAttachment = { attachmentId: 'image-1', width: 12, height: 8, imageData: new Uint8Array([1, 2, 3]) };
    const output = new AttributedTextBuilder()
      .append('Before ')
      .appendInlineImage(imageAttachment)
      .append(' after')
      .build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Before ',
      AttributedTextEntryType.InlineImage,
      imageAttachment,
      AttributedTextEntryType.Content,
      ' after',
    ]);
  });

  it('can append an inline view attachment with vertical alignment', () => {
    const output = new AttributedTextBuilder()
      .append('Before ')
      .appendInlineView(2, AttributedTextInlineViewVerticalAlignment.Bottom)
      .append(' after')
      .build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Before ',
      AttributedTextEntryType.InlineView,
      { childIndex: 2, verticalAlignment: AttributedTextInlineViewVerticalAlignment.Bottom },
      AttributedTextEntryType.Content,
      ' after',
    ]);
  });

  it('can append an inline view attachment with baseline alignment', () => {
    const output = new AttributedTextBuilder()
      .append('Before ')
      .appendInlineView(2, AttributedTextInlineViewVerticalAlignment.Baseline)
      .append(' after')
      .build();

    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Before ',
      AttributedTextEntryType.InlineView,
      { childIndex: 2, verticalAlignment: AttributedTextInlineViewVerticalAlignment.Baseline },
      AttributedTextEntryType.Content,
      ' after',
    ]);
  });

  it('can push background color', () => {
    const output = new AttributedTextBuilder()
      .append('Hello ')
      .pushBackgroundColor('yellow')
      .append('World')
      .pop()
      .build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushBackgroundColor,
      'yellow',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can push background padding and border radius', () => {
    const output = new AttributedTextBuilder()
      .append('Hello ')
      .pushBackgroundPadding({ left: 1, top: 2, right: 3, bottom: 4 })
      .pushBackgroundBorderRadius('50%')
      .append('World')
      .pop()
      .pop()
      .build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushBackgroundPadding,
      { left: 1, top: 2, right: 3, bottom: 4 },
      AttributedTextEntryType.PushBackgroundBorderRadius,
      '50%',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can push background padding as a single number', () => {
    const output = new AttributedTextBuilder()
      .append('Hello ')
      .pushBackgroundPadding(4)
      .append('World')
      .pop()
      .build();
    expect(output).toEqual([
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushBackgroundPadding,
      4,
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can append string and attributes', () => {
    const output = new AttributedTextBuilder()
      .append('Hello ', {
        color: 'red',
        backgroundColor: 'yellow',
        backgroundPadding: 4,
        backgroundBorderRadius: 5,
      })
      .append('World', { color: 'green', font: 'title', textDecoration: 'underline' })
      .build();

    expect(output).toEqual([
      AttributedTextEntryType.PushColor,
      'red',
      AttributedTextEntryType.PushBackgroundColor,
      'yellow',
      AttributedTextEntryType.PushBackgroundPadding,
      4,
      AttributedTextEntryType.PushBackgroundBorderRadius,
      5,
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.PushColor,
      'green',
      AttributedTextEntryType.PushFont,
      'title',
      AttributedTextEntryType.PushTextDecoration,
      'underline',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can nest background color styles', () => {
    const output = new AttributedTextBuilder()
      .withStyle({ backgroundColor: 'yellow' }, b => {
        b.appendText('Hello ')
          .withStyle({ backgroundColor: 'orange' }, b => {
            b.append('World');
          })
          .appendText('!');
      })
      .build();

    expect(output).toEqual([
      AttributedTextEntryType.PushBackgroundColor,
      'yellow',
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushBackgroundColor,
      'orange',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Content,
      '!',
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can nest background padding and border radius styles', () => {
    const output = new AttributedTextBuilder()
      .withStyle({ backgroundPadding: { left: 1, right: 1 }, backgroundBorderRadius: 2 }, b => {
        b.appendText('Hello ')
          .withStyle({ backgroundPadding: { left: 3, right: 3 }, backgroundBorderRadius: 4 }, b => {
            b.append('World');
          })
          .appendText('!');
      })
      .build();

    expect(output).toEqual([
      AttributedTextEntryType.PushBackgroundPadding,
      { left: 1, right: 1 },
      AttributedTextEntryType.PushBackgroundBorderRadius,
      2,
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushBackgroundPadding,
      { left: 3, right: 3 },
      AttributedTextEntryType.PushBackgroundBorderRadius,
      4,
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Content,
      '!',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Pop,
    ]);
  });

  it('can nest styles', () => {
    const output = new AttributedTextBuilder()
      .withStyle({ font: 'bold' }, b => {
        b.appendText('Hello ')
          .withStyle({ font: 'italic' }, b => {
            b.append('World');
          })
          .appendText('!');
      })
      .build();

    expect(output).toEqual([
      AttributedTextEntryType.PushFont,
      'bold',
      AttributedTextEntryType.Content,
      'Hello ',
      AttributedTextEntryType.PushFont,
      'italic',
      AttributedTextEntryType.Content,
      'World',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Content,
      '!',
      AttributedTextEntryType.Pop,
    ]);
  });
});
