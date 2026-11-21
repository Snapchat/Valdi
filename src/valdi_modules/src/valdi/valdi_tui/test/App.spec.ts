import 'jasmine/src/jasmine';
import { FocusManager } from '../src/App';
import { parseInput } from '../src/keys';

describe('FocusManager', () => {
  function makeManager(): FocusManager {
    const manager = new FocusManager();
    manager.register({ id: 'a' });
    manager.register({ id: 'b' });
    manager.register({ id: 'c' });
    return manager;
  }

  it('moves focus forward with tab', () => {
    const manager = makeManager();
    manager.handleKey(parseInput('\t')[0]);
    expect(manager.activeId).toBe('a');
    manager.handleKey(parseInput('\t')[0]);
    expect(manager.activeId).toBe('b');
  });

  it('moves focus to the previous element with shift+tab', () => {
    const manager = makeManager();
    manager.focus('b');
    expect(manager.handleKey(parseInput('\x1b[Z')[0])).toBe(true);
    expect(manager.activeId).toBe('a');
    manager.handleKey(parseInput('\x1b[Z')[0]);
    expect(manager.activeId).toBe('c');
  });
});
