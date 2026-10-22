import 'jasmine/src/jasmine';
import { ArgumentsParser } from 'valdi_cli/src/ArgumentsParser';

describe('ArgumentsParser', () => {
  it('applies the parser to array arguments', () => {
    const parser = new ArgumentsParser('test', ['test', '--n', '1', '--n', '2']);
    const numbers = parser.addArray('--n', 'numbers', true, v => Number.parseInt(v, 10));

    parser.parse();

    expect(numbers.value).toEqual([1, 2]);
  });

  it('does not consume the program arguments when parsing', () => {
    const parser = new ArgumentsParser('test', ['test', '--name', 'value']);
    const name = parser.addString('--name', 'name', true);

    parser.parse();

    expect(name.value).toBe('value');
    expect(() => parser.parse()).toThrowError(/passed multiple times/);
  });

  it('parses number arguments', () => {
    const parser = new ArgumentsParser('test', ['test', '--count', '1.5']);
    const count = parser.addNumber('--count', 'count', true);

    parser.parse();

    expect(count.value).toBe(1.5);
  });

  it('rejects number arguments that are not numbers', () => {
    for (const input of ['abc', '12abc', '']) {
      const parser = new ArgumentsParser('test', ['test', '--count', input]);
      parser.addNumber('--count', 'count', true);

      expect(() => parser.parse()).toThrowError(/Expected a number/);
    }
  });
});
