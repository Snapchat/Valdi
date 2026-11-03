import 'jasmine/src/jasmine';
import {
  UncaughtErrorHandlerResult,
  getCatchAllUncaughtErrorHandler,
  setCatchAllUncaughtErrorHandler,
} from '../src/UncaughtErrorHandler';

describe('UncaughtErrorHandler', () => {
  describe('getCatchAllUncaughtErrorHandler', () => {
    const initial = getCatchAllUncaughtErrorHandler();

    afterEach(() => {
      setCatchAllUncaughtErrorHandler(initial);
    });

    it('returns the handler that was set', () => {
      const handler = () => UncaughtErrorHandlerResult.IGNORE;
      setCatchAllUncaughtErrorHandler(handler);
      expect(getCatchAllUncaughtErrorHandler()).toBe(handler);
    });

    it('returns undefined once the handler is unset', () => {
      setCatchAllUncaughtErrorHandler(() => UncaughtErrorHandlerResult.IGNORE);
      setCatchAllUncaughtErrorHandler(undefined);
      expect(getCatchAllUncaughtErrorHandler()).toBeUndefined();
    });
  });
});
