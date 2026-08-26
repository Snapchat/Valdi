import 'jasmine';
import type { ServerResponse } from 'node:http';
import { ConsoleSseWriter, MAX_CONSOLE_SSE_BUFFERED_EVENTS } from './server';

class MockSseResponse {
  readonly chunks: string[] = [];
  readonly writeResults: boolean[] = [];
  private readonly drainListeners = new Set<() => void>();

  emitDrain(): void {
    const listeners = Array.from(this.drainListeners);
    this.drainListeners.clear();
    for (const listener of listeners) listener();
  }

  listenerCount(event: string): number {
    return event === 'drain' ? this.drainListeners.size : 0;
  }

  off(event: string, listener: () => void): this {
    if (event === 'drain') this.drainListeners.delete(listener);
    return this;
  }

  once(event: string, listener: () => void): this {
    if (event === 'drain') this.drainListeners.add(listener);
    return this;
  }

  write(chunk: string): boolean {
    this.chunks.push(chunk);
    return this.writeResults.shift() ?? true;
  }
}

describe('Chromium console SSE backpressure', () => {
  it('buffers complete SSE frames while blocked and flushes them in order on drain', () => {
    const response = new MockSseResponse();
    response.writeResults.push(false, true, true);
    const failures: Error[] = [];
    const writer = new ConsoleSseWriter(response as unknown as ServerResponse, error => failures.push(error));

    expect(writer.send('console', { message: 'first' })).toBeTrue();
    expect(writer.send('console', { message: 'second' })).toBeTrue();
    expect(writer.send('heartbeat', { sequence: 3 })).toBeTrue();
    expect(response.chunks).toHaveSize(1);

    response.emitDrain();

    expect(response.chunks).toHaveSize(3);
    expect(response.chunks[0]).toContain('"first"');
    expect(response.chunks[1]).toContain('"second"');
    expect(response.chunks[2]).toContain('event: heartbeat');
    expect(failures).toEqual([]);
  });

  it('fails closed instead of accumulating an unbounded backpressure queue', () => {
    const response = new MockSseResponse();
    response.writeResults.push(false);
    const failures: Error[] = [];
    const writer = new ConsoleSseWriter(response as unknown as ServerResponse, error => failures.push(error));

    expect(writer.send('console', { sequence: 0 })).toBeTrue();
    for (let index = 0; index < MAX_CONSOLE_SSE_BUFFERED_EVENTS; index += 1) {
      expect(writer.send('console', { sequence: index + 1 })).toBeTrue();
    }
    expect(writer.send('console', { sequence: MAX_CONSOLE_SSE_BUFFERED_EVENTS + 1 })).toBeFalse();

    expect(failures).toHaveSize(1);
    expect(failures[0]?.message).toContain('backpressure limit');
    expect(writer.send('console', { sequence: 999 })).toBeFalse();
    response.emitDrain();
    expect(response.chunks).toHaveSize(1);
  });

  it('fails closed on unserializable payloads and removes pending drain listeners on close', () => {
    const response = new MockSseResponse();
    response.writeResults.push(false);
    const failures: Error[] = [];
    const writer = new ConsoleSseWriter(response as unknown as ServerResponse, error => failures.push(error));
    const cyclic: Record<string, unknown> = {};
    cyclic['self'] = cyclic;

    expect(writer.send('console', { message: 'accepted' })).toBeTrue();
    expect(response.listenerCount('drain')).toBe(1);
    expect(writer.send('console', cyclic)).toBeFalse();

    expect(failures).toHaveSize(1);
    expect(response.listenerCount('drain')).toBe(0);
  });
});
