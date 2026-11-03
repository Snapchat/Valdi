import 'jasmine/src/jasmine';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributedText, AttributedTextEntryType } from 'valdi_tsx/src/AttributedText';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import { ParsedAttributedText } from '../src/utils/parseAttributedText';

describe('parseAttributedText', () => {
  it('parses attributed text into styled parts and plain text', () => {
    const attributedText = new AttributedTextBuilder()
      .append('plain ')
      .pushColor('#111111')
      .append('colored')
      .pushFont('system-bold 18')
      .append(' bold')
      .pop()
      .append(' still colored')
      .pop()
      .build();

    const parsed = ParsedAttributedText.parse(attributedText);

    expect(parsed.toString()).toBe('plain colored bold still colored');
    expect(parsed.parts.map(part => part.content)).toEqual(['plain ', 'colored', ' bold', ' still colored']);
    expect(parsed.parts.map(part => part.style.color)).toEqual([undefined, '#111111', '#111111', '#111111']);
    expect(parsed.parts.map(part => part.style.font)).toEqual([undefined, undefined, 'system-bold 18', undefined]);
    expect(parsed.hasOnLayout).toBeFalse();
  });

  it('records whether any parsed part has an onLayout callback', () => {
    const parsed = ParsedAttributedText.parse(
      new AttributedTextBuilder()
        .append('plain')
        .append('measured', {
          onLayout() {},
        })
        .build(),
    );

    expect(parsed.hasOnLayout).toBeTrue();
  });

  it('parses inline view attachments without adding plain text content', () => {
    const attributedText = new AttributedTextBuilder()
      .append('before ')
      .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Baseline)
      .append(' after')
      .build();

    const parsed = ParsedAttributedText.parse(attributedText);

    expect(parsed.toString()).toBe('before  after');
    expect(parsed.parts.length).toBe(3);
    expect(parsed.parts[1].style.inlineView?.childIndex).toBe(0);
    expect(parsed.parts[1].style.inlineView?.verticalAlignment).toBe(
      AttributedTextInlineViewVerticalAlignment.Baseline,
    );
  });

  it('preserves normalized animation transform metadata on styled parts', () => {
    const attributedText = new AttributedTextBuilder()
      .append('animated', {
        animationTransform: {
          key: 'intro',
          opacity: 0,
          partPattern: '\\S+',
        },
      })
      .build();

    const parsed = ParsedAttributedText.parse(attributedText);
    const transform = parsed.parts[0].style.animationTransform;

    expect(transform?.key).toBe('intro');
    expect(transform?.opacity).toBe(0);
    expect(transform?.translationY).toBe(0);
    expect(transform?.scale).toBe(1);
    expect(transform?.duration).toBe(0.35);
    expect(transform?.timeOffsetBetweenParts).toBe(0);
    expect(transform?.groupIndex).toBe(0);
    expect(transform?.partIndexInGroup).toBe(0);
    expect(transform?.partPattern).toBe('\\S+');
  });

  it('logs invalid animation transform payloads and keeps the pushed style frame balanced', () => {
    const errorSpy = spyOn(console, 'error');
    const attributedText = [
      AttributedTextEntryType.PushColor,
      '#123456',
      AttributedTextEntryType.PushAnimationTransform,
      'invalid',
      AttributedTextEntryType.Content,
      'plain',
      AttributedTextEntryType.Pop,
      AttributedTextEntryType.Content,
      ' still colored',
      AttributedTextEntryType.Pop,
    ] as AttributedText;

    const parsed = ParsedAttributedText.parse(attributedText);

    expect(errorSpy).toHaveBeenCalled();
    expect(String(errorSpy.calls.mostRecent().args[0])).toContain('Invalid text animation transform');
    expect(errorSpy.calls.mostRecent().args[1]).toBe('invalid');
    expect(parsed.parts.map(part => part.style.animationTransform)).toEqual([undefined, undefined]);
    expect(parsed.parts.map(part => part.style.color)).toEqual(['#123456', '#123456']);
  });

  it('keeps animation part indexes continuous for parts in the same transform group', () => {
    const attributedText = new AttributedTextBuilder()
      .pushAnimationTransform({
        opacity: 0,
        timeOffsetBetweenParts: 0.1,
      })
      .append('one')
      .append('two')
      .pop()
      .build();

    const parsed = ParsedAttributedText.parse(attributedText);

    expect(parsed.parts[0].style.animationTransform?.groupIndex).toBe(0);
    expect(parsed.parts[0].style.animationTransform?.partIndexInGroup).toBe(0);
    expect(parsed.parts[1].style.animationTransform?.groupIndex).toBe(0);
    expect(parsed.parts[1].style.animationTransform?.partIndexInGroup).toBe(1);
  });
});
