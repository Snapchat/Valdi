import { EmbeddedPlatformViewRasterMethod, IManagedContextOptions } from 'drawing/src/ManagedContextFactory';

import 'jasmine/src/jasmine';

/**
 * Compile-time API compatibility tests for the managed_context_example.
 *
 * These tests guard against regressions where IManagedContextOptions fields are renamed
 * without updating the example. If a field is renamed (e.g. embeddedPlatformViewRasterMethod
 * → useNewExternalSurfaceRasterMethod), the TypeScript assignment below will fail to compile,
 * catching the breakage before it ships.
 */
describe('managed_context_example API compatibility', () => {
  it('IManagedContextOptions accepts embeddedPlatformViewRasterMethod and deltaRasterization', () => {
    // This is primarily a compile-time check. If either field is renamed in IManagedContextOptions,
    // this assignment will produce a TS2353 error ("Object literal may only specify known properties").
    const options: IManagedContextOptions = {
      embeddedPlatformViewRasterMethod: EmbeddedPlatformViewRasterMethod.FAST,
      deltaRasterization: true,
    };

    expect(options.embeddedPlatformViewRasterMethod).toBe(EmbeddedPlatformViewRasterMethod.FAST);
    expect(options.deltaRasterization).toBeTrue();
  });

  it('EmbeddedPlatformViewRasterMethod.ACCURATE is distinct from FAST', () => {
    // Ensures the enum values used in the example (FAST / ACCURATE) remain meaningful.
    expect(EmbeddedPlatformViewRasterMethod.FAST).not.toBe(EmbeddedPlatformViewRasterMethod.ACCURATE);
  });
});
