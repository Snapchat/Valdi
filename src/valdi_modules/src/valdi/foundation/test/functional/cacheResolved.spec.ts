import { cacheResolved } from 'foundation/src/functional/cacheResolved';
import 'jasmine/src/jasmine';

describe('cacheResovled', () => {
  it('returns values', async () => {
    const squareString = cacheResolved<[number], string>(x => x.toString())(async x => (x * x).toString());

    expect(await squareString(5)).toBe('25');
    expect(await squareString(7)).toBe('49');
  });

  it('caches results', async () => {
    const squareString = jasmine.createSpy('squareString', async x => (x * x).toString());
    const cached = cacheResolved<[number], string>(x => x.toString())(squareString);
    squareString.and.callThrough();

    expect(await cached(5)).toBe('25');
    expect(await cached(7)).toBe('49');
    expect(await cached(5)).toBe('25');
    expect(await cached(7)).toBe('49');
    expect(squareString).toHaveBeenCalledTimes(2);
  });

  it('does not cache rejects', async () => {
    const throws = jasmine.createSpy();
    const cached = cacheResolved<[number], string>(x => x.toString())(throws);
    throws.and.rejectWith('some error');

    try {
      await cached(1);
      fail('unexpected success');
    } catch {
      // no-op
    }
    try {
      await cached(1);
      fail('unexpected success');
    } catch {
      // no-op
    }
    expect(throws).toHaveBeenCalledTimes(2);
  });

  it('shares unresolved promise', async () => {
    const resolvers: Record<number, (result: string) => void> = {};
    const longOperation = jasmine.createSpy(
      'longOperation',
      (arg: number) =>
        new Promise<string>(resolve => {
          resolvers[arg] = resolve;
        }),
    );
    const cached = cacheResolved<[number], string>(x => x.toString())(longOperation);
    longOperation.and.callThrough();

    // eslint-disable-next-line @snap/valdi/assign-timer-id
    setTimeout(() => Object.entries(resolvers).forEach(([value, resolver]) => resolver(`done ${value}`)), 10);

    expect(await Promise.all([cached(1), cached(2), cached(1)])).toEqual(['done 1', 'done 2', 'done 1']);
    expect(longOperation).toHaveBeenCalledTimes(2);
  });
});
