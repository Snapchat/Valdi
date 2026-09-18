import 'jasmine';
import type { DaemonConnection } from '../utils/daemonClient';
import {
  DEBUGGER_INPUT_IDENTIFIER,
  DebuggerInputType,
  sendDebuggerInput,
  unwrapDebuggerInputResponse,
} from './inputClient';

function makeHandledResponse(type: DebuggerInputType, result: Record<string, unknown>): Record<string, unknown> {
  return {
    handled: true,
    data: {
      contractVersion: 1,
      handled: true,
      type,
      ...result,
    },
  };
}

describe('debugger input client', () => {
  it('sends the shared request identifier and unwraps the target result', async () => {
    const targetResult = {
      contractVersion: 1,
      handled: true,
      type: DebuggerInputType.Query,
      action: DebuggerInputType.Query,
      elements: [],
    };
    const customRequest = jasmine.createSpy('customRequest').and.resolveTo({ handled: true, data: targetResult });
    const conn = { customRequest } as unknown as DaemonConnection;
    const request = {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      selector: '#composer',
    };

    const result = await sendDebuggerInput(conn, 'client-1', request);

    expect(customRequest).toHaveBeenCalledOnceWith('client-1', DEBUGGER_INPUT_IDENTIFIER, request, 5000);
    expect(result).toEqual(targetResult);
  });

  it('returns a structured unsupported-target result', () => {
    expect(
      unwrapDebuggerInputResponse(
        { handled: false },
        {
          type: DebuggerInputType.Tap,
          elementId: 3,
        },
      ),
    ).toEqual({
      handled: false,
      type: DebuggerInputType.Tap,
      elementId: 3,
      message: 'The target app did not register the Valdi debugger input handler.',
    });
  });

  it('accepts context-free capabilities and rejects malformed target responses', async () => {
    const customRequest = jasmine.createSpy('customRequest').and.resolveTo({
      handled: true,
      data: {
        contractVersion: 1,
        handled: true,
        type: DebuggerInputType.Capabilities,
        action: DebuggerInputType.Capabilities,
        supportedTypes: Object.values(DebuggerInputType),
        selectorForms: ['elementId'],
      },
    });
    const conn = { customRequest } as unknown as DaemonConnection;

    await expectAsync(sendDebuggerInput(conn, 'client-1', { type: DebuggerInputType.Capabilities })).toBeResolvedTo(
      jasmine.objectContaining({ handled: true, type: DebuggerInputType.Capabilities }),
    );
    expect(customRequest).toHaveBeenCalledOnceWith(
      'client-1',
      DEBUGGER_INPUT_IDENTIFIER,
      { type: DebuggerInputType.Capabilities },
      5000,
    );

    for (const malformedResponse of [
      null,
      {},
      { handled: 'yes' },
      { handled: true },
      { handled: true, data: [] },
      { handled: true, data: { contractVersion: 1, type: 'query' } },
      { handled: true, data: { contractVersion: 0, handled: true, type: 'query' } },
      { handled: true, data: { contractVersion: 1, handled: true, type: 'tap' } },
      { handled: true, data: { contractVersion: 1, handled: true, type: 'query' } },
      { handled: true, data: { contractVersion: 1, handled: true, type: 'query', elements: 'bad' } },
      {
        handled: true,
        data: {
          contractVersion: 1,
          handled: true,
          type: 'query',
          elements: [{ elementId: 1, tag: 'view' }],
        },
      },
      {
        handled: true,
        data: {
          contractVersion: 1,
          handled: true,
          type: 'tap',
          elementId: 1.5,
          action: 'onTap',
          actionElementId: 1,
        },
      },
      {
        handled: true,
        data: {
          contractVersion: 1,
          handled: true,
          type: 'text',
          elementId: 1,
          action: 'onChange',
          actionElementId: 1,
          value: 'safe',
          selectionStart: Number.NaN,
          selectionEnd: 4,
        },
      },
    ]) {
      expect(() => unwrapDebuggerInputResponse(malformedResponse, { type: 'query' })).toThrowError(
        /Invalid debugger input response/,
      );
    }
  });

  it('rejects invalid Unicode in raw selector strings before daemon dispatch', async () => {
    const customRequest = jasmine.createSpy('customRequest');
    const conn = { customRequest } as unknown as DaemonConnection;

    await expectAsync(
      sendDebuggerInput(conn, 'client-1', {
        type: DebuggerInputType.Query,
        selector: String.fromCodePoint(0xd8_3d),
      }),
    ).toBeRejectedWithError('selector must contain valid Unicode.');
    expect(customRequest).not.toHaveBeenCalled();
  });

  it('validates action-specific handled response actions and fields', () => {
    const validCases: Array<{
      request: Record<string, unknown>;
      response: Record<string, unknown>;
    }> = [
      {
        request: { type: DebuggerInputType.Tap, elementId: 1 },
        response: makeHandledResponse(DebuggerInputType.Tap, {
          elementId: 1,
          action: 'onTap',
          actionElementId: 2,
        }),
      },
      {
        request: { type: DebuggerInputType.Focus, elementId: 1, focused: true },
        response: makeHandledResponse(DebuggerInputType.Focus, {
          elementId: 1,
          action: 'focused',
          actionElementId: 1,
        }),
      },
      {
        request: { type: DebuggerInputType.Text, elementId: 1, text: 'draft' },
        response: makeHandledResponse(DebuggerInputType.Text, {
          elementId: 1,
          action: 'onChange',
          actionElementId: 1,
          value: 'draft',
          selectionStart: 5,
          selectionEnd: 5,
        }),
      },
      {
        request: { type: DebuggerInputType.Key, elementId: 1, key: 'Enter' },
        response: makeHandledResponse(DebuggerInputType.Key, {
          elementId: 1,
          action: 'onReturn',
          actionElementId: 1,
          value: 'draft',
          selectionStart: 5,
          selectionEnd: 5,
        }),
      },
      {
        request: { type: DebuggerInputType.Key, elementId: 1, key: 'Escape' },
        response: makeHandledResponse(DebuggerInputType.Key, {
          elementId: 1,
          action: 'focused',
          actionElementId: 1,
          value: 'draft',
        }),
      },
      {
        request: { type: DebuggerInputType.Scroll, elementId: 1, deltaY: 3 },
        response: makeHandledResponse(DebuggerInputType.Scroll, {
          elementId: 1,
          action: 'contentOffset',
          actionElementId: 4,
          contentOffsetX: 0,
          contentOffsetY: 3,
        }),
      },
    ];
    for (const testCase of validCases) {
      expect(unwrapDebuggerInputResponse(testCase.response, testCase.request)['handled']).toBeTrue();
    }

    const invalidCases: Array<{
      request: Record<string, unknown>;
      response: Record<string, unknown>;
    }> = [
      {
        request: { type: DebuggerInputType.Tap, elementId: 1 },
        response: makeHandledResponse(DebuggerInputType.Tap, {
          elementId: 1,
          action: 'focused',
          actionElementId: 1,
        }),
      },
      {
        request: { type: DebuggerInputType.Focus, elementId: 1 },
        response: makeHandledResponse(DebuggerInputType.Focus, { elementId: 1, action: 'focused' }),
      },
      {
        request: { type: DebuggerInputType.Text, elementId: 1, text: 'draft' },
        response: makeHandledResponse(DebuggerInputType.Text, {
          elementId: 1,
          action: 'onChange',
          actionElementId: 1,
          value: 'draft',
          selectionStart: 5,
        }),
      },
      {
        request: { type: DebuggerInputType.Key, elementId: 1, key: 'Backspace' },
        response: makeHandledResponse(DebuggerInputType.Key, {
          elementId: 1,
          action: 'focused',
          actionElementId: 1,
          value: 'draf',
          selectionStart: 4,
          selectionEnd: 4,
        }),
      },
      {
        request: { type: DebuggerInputType.Key, elementId: 1, key: 'Escape' },
        response: makeHandledResponse(DebuggerInputType.Key, {
          elementId: 1,
          action: 'focused',
          actionElementId: 1,
        }),
      },
      {
        request: { type: DebuggerInputType.Scroll, elementId: 1, deltaY: 3 },
        response: makeHandledResponse(DebuggerInputType.Scroll, {
          elementId: 1,
          action: 'contentOffset',
          actionElementId: 4,
          contentOffsetX: 0,
        }),
      },
    ];
    for (const testCase of invalidCases) {
      expect(() => unwrapDebuggerInputResponse(testCase.response, testCase.request)).toThrowError(
        /Invalid debugger input response/,
      );
    }
  });
});
