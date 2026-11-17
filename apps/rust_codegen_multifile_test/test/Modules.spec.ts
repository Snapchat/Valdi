import 'jasmine/src/jasmine';
import { ping } from '../src/Secondary';
import { pong } from '../src/Tertiary';

describe('multi-file Rust generated modules', () => {
  it('loads multiple registered Rust modules', () => {
    expect(ping(42)).toBe(42);
    expect(pong(42)).toBe(42);
  });
});
