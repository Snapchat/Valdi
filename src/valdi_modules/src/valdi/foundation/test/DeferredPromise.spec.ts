import { DeferredPromise, DeferredPromiseTimeoutError } from 'foundation/src/DeferredPromise';

// Group 1: Async Logic (Standard)
describe('DeferredPromise', () => {
  describe('DeferredPromiseTimeoutError', () => {
    it('checks name, message, and inheritance', () => {
      const err = new DeferredPromiseTimeoutError('msg');
      expect(err.name).toBe('DeferredPromiseTimeoutError');
      expect(err.message).toBe('msg');
      expect(err).toBeInstanceOf(Error);
    });
  });

  describe('Resolution', () => {
    it('resolves with value', async () => {
      const def = new DeferredPromise<number>();
      def.resolve(42);
      await expectAsync(def.promise).toBeResolvedTo(42);
    });

    it('resolves with PromiseLike', async () => {
      const def = new DeferredPromise<string>();
      def.resolve(Promise.resolve('hi'));
      await expectAsync(def.promise).toBeResolvedTo('hi');
    });

    it('handles multiple then() calls', async () => {
      const def = new DeferredPromise<number>();
      const res: number[] = [];

      void def.then(v => res.push(v));
      void def.then(v => res.push(v * 2));

      def.resolve(10);
      await def.promise;
      expect(res).toEqual([10, 20]);
    });
  });

  describe('Rejection', () => {
    it('rejects with reason', async () => {
      const def = new DeferredPromise<number>();
      def.reject('err');
      await expectAsync(def.promise).toBeRejectedWith('err');
    });

    it('rejects without reason', async () => {
      const def = new DeferredPromise<void>();
      def.reject();
      await expectAsync(def.promise).toBeRejected();
    });

    it('triggers onrejected in then()', async () => {
      const def = new DeferredPromise<number>();
      const p = def.then(
        () => 'ok',
        (e: unknown) => `caught: ${String(e)}`,
      );
      def.reject('oops');
      await expectAsync(p).toBeResolvedTo('caught: oops');
    });
  });

  describe('Chaining', () => {
    it('chains transformations', async () => {
      const def = new DeferredPromise<number>();
      const p = def.then(v => v * 2).then(v => `Res: ${v}`);
      def.resolve(5);
      await expectAsync(p).toBeResolvedTo('Res: 10');
    });

    it('handles exceptions in then()', async () => {
      const def = new DeferredPromise<number>();
      const p = def.then(() => {
        throw new Error('fail');
      });
      def.resolve(1);
      await expectAsync(p).toBeRejectedWith(new Error('fail'));
    });
  });

  describe('Types & Properties', () => {
    it('works with objects', async () => {
      const def = new DeferredPromise<{ id: number }>();
      const obj = { id: 1 };
      def.resolve(obj);
      await expectAsync(def.promise).toBeResolvedTo(obj);
    });

    it('works with void', async () => {
      const def = new DeferredPromise<void>();
      def.resolve();
      await expectAsync(def.promise).toBeResolved();
    });

    it('keeps promise property stable', () => {
      const def = new DeferredPromise<number>();
      expect(def.promise).toBe(def.promise);
    });
  });
});

// Group 2: Timing & Idempotency (Mock Clock)
describe('DeferredPromise Timing', () => {
  beforeEach(() => jasmine.clock().install());
  afterEach(() => jasmine.clock().uninstall());

  it('timeouts after limit', async () => {
    const def = new DeferredPromise<number>(50);
    let err: unknown;

    void def.promise.catch((e: Error) => {
      err = e;
    });

    jasmine.clock().tick(51);
    await Promise.resolve();
    expect(err).toBeInstanceOf(DeferredPromiseTimeoutError);
  });

  it('does not timeout if resolved early', async () => {
    const def = new DeferredPromise<number>(100);
    let res: number | undefined;

    void def.promise.then(v => {
      res = v;
    });

    jasmine.clock().tick(50);
    def.resolve(42);
    await Promise.resolve();
    jasmine.clock().tick(100);

    expect(res).toBe(42);
  });

  it('does not timeout if rejected early', async () => {
    const def = new DeferredPromise<number>(100);
    let err: unknown;

    void def.promise.catch((e: unknown) => {
      err = e;
    });

    jasmine.clock().tick(50);
    def.reject('manual');
    await Promise.resolve();
    jasmine.clock().tick(100);

    expect(err).toBe('manual');
  });

  it('ignores reject after resolve', async () => {
    const def = new DeferredPromise<number>();
    let state = 'pending';

    void def.promise.then(
      () => {
        state = 'resolved';
      },
      () => {
        state = 'rejected';
      },
    );

    def.resolve(1);
    await Promise.resolve();
    def.reject('fail');
    jasmine.clock().tick(1);
    await Promise.resolve();

    expect(state).toBe('resolved');
  });
});
