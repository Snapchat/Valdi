import { isDeviceBuild } from '../src/utils/buildInfo';

describe('isDeviceBuild', () => {
  it('builds for the simulator when --simulator is not passed', () => {
    expect(isDeviceBuild(undefined)).toBe(false);
  });

  it('builds for the simulator with --simulator', () => {
    expect(isDeviceBuild(true)).toBe(false);
  });

  it('builds for a device with --no-simulator', () => {
    expect(isDeviceBuild(false)).toBe(true);
  });
});
