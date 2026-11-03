import 'jasmine/src/jasmine';

import { ValdiWebRendererDelegate } from '../src/ValdiWebRendererDelegate';
import type { ViewNodeTree } from '../src/core/ViewNodeTree';

describe('ValdiWebRendererDelegate', () => {
  it('commits layouts and visibility without animation frames on hidden browser pages', async () => {
    const previousDocument = (globalThis as { document?: Document }).document;
    (globalThis as { document?: Document }).document = { visibilityState: 'hidden' } as Document;
    const events: string[] = [];
    const tree = {
      drainScheduledLayoutObserverRefresh(): void {
        events.push('layout');
      },
      drainScheduledVisibilityRefresh(force: boolean): void {
        events.push(`visibility:${String(force)}`);
      },
    } as ViewNodeTree;

    try {
      const delegate = new ValdiWebRendererDelegate({} as HTMLElement, tree);
      delegate.onNextLayoutComplete(() => events.push('completed'));

      expect(events).toEqual([]);
      await Promise.resolve();
      await Promise.resolve();

      expect(events).toEqual(['layout', 'visibility:true', 'layout', 'visibility:true', 'completed']);
    } finally {
      if (previousDocument === undefined) {
        delete (globalThis as { document?: Document }).document;
      } else {
        (globalThis as { document?: Document }).document = previousDocument;
      }
    }
  });
});
