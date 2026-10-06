import 'jasmine/src/jasmine';
import { parseFontStyle } from '../src/styles/parseFontStyle';

describe('parseFontStyle', () => {
  it('derives a numeric weight from the PostScript face name', () => {
    expect(parseFontStyle('AvenirNext-DemiBold 16').fontWeight).toBe('600');
    expect(parseFontStyle('AvenirNext-Medium 14').fontWeight).toBe('500');
    expect(parseFontStyle('AvenirNext-Bold 16').fontWeight).toBe('700');
    expect(parseFontStyle('AvenirNext-Heavy 28').fontWeight).toBe('800');
    expect(parseFontStyle('AvenirNext-Regular 16').fontWeight).toBe('400');
  });

  it('falls back past the face name to the display name and a generic family', () => {
    expect(parseFontStyle('AvenirNext-DemiBold 16').fontFamily).toBe('AvenirNext-DemiBold, "Avenir Next", sans-serif');
  });

  it('keeps the size in px', () => {
    expect(parseFontStyle('AvenirNext-Medium 16').fontSize).toBe('16px');
  });

  it('emits no size when the font string has none', () => {
    // WebValdiLabel defaults to a bare 'Montserrat-SemiBold'; this used to yield
    // the literal 'undefinedpx'.
    expect(parseFontStyle('Montserrat-SemiBold').fontSize).toBe('');
    expect(parseFontStyle('Montserrat-SemiBold').fontWeight).toBe('600');
  });

  it('ignores the dynamic-type style and max size tokens', () => {
    const withStyle = parseFontStyle('AvenirNext-Medium 16 Body');
    const withMaxSize = parseFontStyle('AvenirNext-Medium 16 Body 24');
    const bare = parseFontStyle('AvenirNext-Medium 16');

    expect(withStyle).toEqual(bare);
    expect(withMaxSize).toEqual(bare);
    // 'Body' is not a CSS font-weight; emitting it made the browser fall back to 400.
    expect(withStyle.fontWeight).toBe('500');
  });

  it('resets the weight for a family that does not encode one', () => {
    const parsed = parseFontStyle('Helvetica 12');

    // Empty rather than absent: styles are Object.assign-ed, so an absent key
    // would keep the previous font's weight.
    expect(parsed.fontWeight).toBe('');
    expect(parsed.fontFamily).toBe('Helvetica, sans-serif');
  });

  it('does not split a hyphenated family whose tail is not a weight', () => {
    expect(parseFontStyle('Noto-Sans 12').fontFamily).toBe('Noto-Sans, sans-serif');
  });

  it('keeps a literal CSS weight', () => {
    // The web-facing shape, e.g. composer_example's NavDemoPage3.
    const parsed = parseFontStyle('sans-serif 32 bold');

    expect(parsed.fontFamily).toBe('sans-serif');
    expect(parsed.fontSize).toBe('32px');
    expect(parsed.fontWeight).toBe('bold');
    expect(parseFontStyle('Helvetica 12 600').fontWeight).toBe('600');
    expect(parseFontStyle('Helvetica 12 Bold').fontWeight).toBe('bold');
  });

  it('prefers a literal weight over the one in the face name', () => {
    expect(parseFontStyle('AvenirNext-Medium 16 bold').fontWeight).toBe('bold');
  });

  it('does not read the dynamic-type max size as a weight', () => {
    // 'Body 400' is a style name and a max size; 400 is also a valid CSS weight.
    expect(parseFontStyle('AvenirNext-Medium 16 Body 400').fontWeight).toBe('500');
  });

  it('keeps a multi-word family intact', () => {
    const parsed = parseFontStyle('Times New Roman 16');

    expect(parsed.fontFamily).toBe('"Times New Roman", sans-serif');
    expect(parsed.fontSize).toBe('16px');
    expect(parseFontStyle('Times New Roman 16 bold').fontWeight).toBe('bold');
  });

  it('strips a trailing weight from a size-less font string', () => {
    const parsed = parseFontStyle('sans-serif bold');

    expect(parsed.fontFamily).toBe('sans-serif');
    expect(parsed.fontWeight).toBe('bold');
    expect(parsed.fontSize).toBe('');
  });

  it('tolerates stray whitespace', () => {
    expect(parseFontStyle('  AvenirNext-Bold   16  ')).toEqual(parseFontStyle('AvenirNext-Bold 16'));
  });

  it('emits an empty family rather than an invalid one for an empty string', () => {
    // 'font-family: , sans-serif' is invalid, so CSS drops the declaration and
    // the element keeps the previous font — the opposite of what empty means here.
    expect(parseFontStyle('')).toEqual({ fontFamily: '', fontSize: '', fontWeight: '' });
    expect(parseFontStyle('   ').fontFamily).toBe('');
  });

  it('does not repeat the generic fallback', () => {
    expect(parseFontStyle('sans-serif 16').fontFamily).toBe('sans-serif');
  });
});
