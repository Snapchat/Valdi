const probe = globalThis as { integrationLazyProbeLoads?: number };
probe.integrationLazyProbeLoads = (probe.integrationLazyProbeLoads ?? 0) + 1;

export const PROBE_VALUE = 'lazy-probe';
