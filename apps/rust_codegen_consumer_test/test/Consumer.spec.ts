import 'jasmine/src/jasmine';
import { ping } from 'rust_codegen_multifile_test/src/Secondary';

describe('a TypeScript consumer of Rust modules', () => {
  it('loads a transitive native module', () => {
    expect(ping(42)).toBe(42);
  });
});
