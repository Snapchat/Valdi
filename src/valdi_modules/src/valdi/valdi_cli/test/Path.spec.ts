import 'jasmine/src/jasmine';
import { Path } from 'valdi_cli/src/Path';

describe('Path', () => {
  it('round-trips absolute and relative paths', () => {
    expect(Path.fromString('/etc').toString()).toBe('/etc');
    expect(Path.fromString('/etc/hosts').toString()).toBe('/etc/hosts');
    expect(Path.fromString('/').toString()).toBe('/');
    expect(Path.fromString('a/b').toString()).toBe('a/b');
    expect(Path.fromString('a/b/').toString()).toBe('a/b');
  });

  it('returns the parent of an absolute path', () => {
    expect(Path.fromString('/etc/hosts').parent().toString()).toBe('/etc');
    expect(Path.fromString('/etc').parent().toString()).toBe('/');
  });

  it('splits joined components', () => {
    const joined = Path.join('a', 'b/c');
    expect(joined.toString()).toBe('a/b/c');
    expect(joined.parent().toString()).toBe('a/b');
    expect(joined.basename()).toBe('c');
  });

  it('keeps joined absolute paths absolute', () => {
    const joined = Path.join('/a', 'b');
    expect(joined.toString()).toBe('/a/b');
    expect(Path.isAbsolute(joined.toString())).toBeTrue();
    expect(joined.parent().toString()).toBe('/a');
  });

  it('appends and normalizes', () => {
    expect(Path.fromString('/a/b').appending('../c', './d').normalize().toString()).toBe('/a/c/d');
    expect(Path.fromString('/a').appending('/b').toString()).toBe('/b');
  });
});
