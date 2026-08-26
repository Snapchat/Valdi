import net from 'node:net';

export function normalizedHostname(hostname: string): string {
  const unwrapped = hostname.startsWith('[') && hostname.endsWith(']') ? hostname.slice(1, -1) : hostname;
  return unwrapped.toLowerCase();
}

export function isLoopbackHost(hostname: string): boolean {
  const normalized = normalizedHostname(hostname);
  return (
    normalized === 'localhost' || normalized === '::1' || (net.isIP(normalized) === 4 && normalized.startsWith('127.'))
  );
}
