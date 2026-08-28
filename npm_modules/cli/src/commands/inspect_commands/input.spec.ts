import 'jasmine';
import { DebuggerInputType, unwrapDebuggerInputResponse } from '../../debugger/inputClient';
import { buildInputRequest } from './input';

describe('inspect input', () => {
  it('builds a query using a stable accessibility selector', () => {
    expect(
      buildInputRequest({
        action: DebuggerInputType.Query,
        selector: '#composer',
        focused: true,
      }),
    ).toEqual({
      type: 'query',
      selector: '#composer',
    });
  });

  it('builds context-free capabilities without a selector', () => {
    expect(
      buildInputRequest({
        action: DebuggerInputType.Capabilities,
        focused: true,
      }),
    ).toEqual({ type: DebuggerInputType.Capabilities });
  });

  it('parses structured selectors and input details', () => {
    expect(
      buildInputRequest({
        action: DebuggerInputType.Text,
        selector: '{"accessibilityId":"composer","tag":"textfield"}',
        focused: true,
        text: 'hello',
        selectionStart: 5,
        selectionEnd: 5,
      }),
    ).toEqual({
      type: 'text',
      selector: {
        accessibilityId: 'composer',
        tag: 'textfield',
      },
      text: 'hello',
      selectionStart: 5,
      selectionEnd: 5,
    });
  });

  it('builds focus, key, tap, and scroll operations', () => {
    expect(
      buildInputRequest({
        action: DebuggerInputType.Focus,
        accessibilityId: 'composer',
        focused: false,
      }),
    ).toEqual({
      type: 'focus',
      accessibilityId: 'composer',
      focused: false,
    });
    expect(
      buildInputRequest({
        action: DebuggerInputType.Key,
        elementId: 12,
        focused: true,
        key: 'Enter',
      }),
    ).toEqual({
      type: 'key',
      elementId: 12,
      key: 'Enter',
    });
    expect(
      buildInputRequest({
        action: DebuggerInputType.Key,
        elementId: 12,
        focused: true,
        key: '👍🏽',
      }),
    ).toEqual({
      type: 'key',
      elementId: 12,
      key: '👍🏽',
    });
    expect(
      buildInputRequest({
        action: DebuggerInputType.Tap,
        selector: '#send',
        focused: true,
        x: 10,
        y: 20,
      }),
    ).toEqual({
      type: 'tap',
      selector: '#send',
      x: 10,
      y: 20,
    });
    expect(
      buildInputRequest({
        action: DebuggerInputType.Scroll,
        accessibilityId: 'messages',
        focused: true,
        deltaX: 2,
        deltaY: 100,
      }),
    ).toEqual({
      type: 'scroll',
      accessibilityId: 'messages',
      deltaX: 2,
      deltaY: 100,
    });
  });

  it('validates selectors and action-specific values', () => {
    expect(() =>
      buildInputRequest({
        action: DebuggerInputType.Tap,
        elementId: 1,
        accessibilityId: 'send',
        focused: true,
      }),
    ).toThrowError('Use only one of --element-id, --accessibility-id, or --selector.');
    expect(() =>
      buildInputRequest({
        action: DebuggerInputType.Text,
        accessibilityId: 'composer',
        focused: true,
      }),
    ).toThrowError('The text action requires --text.');
    expect(() =>
      buildInputRequest({
        action: DebuggerInputType.Key,
        accessibilityId: 'composer',
        focused: true,
      }),
    ).toThrowError('The key action requires --key.');
    expect(() =>
      buildInputRequest({
        action: DebuggerInputType.Tap,
        focused: true,
        x: Number.NaN,
      }),
    ).toThrowError('An elementId, accessibilityId, or selector is required.');
    expect(() =>
      buildInputRequest({
        action: DebuggerInputType.Query,
        selector: '{"elementId":1.5}',
        focused: true,
      }),
    ).toThrowError('elementId must be a finite integer.');
    expect(() =>
      buildInputRequest({
        action: DebuggerInputType.Text,
        elementId: 1,
        focused: true,
        text: String.fromCodePoint(0xd8_3d),
      }),
    ).toThrowError('text must contain valid Unicode.');
    expect(() =>
      buildInputRequest({
        action: DebuggerInputType.Key,
        elementId: 1,
        focused: true,
        key: 'abc',
      }),
    ).toThrowError('key must be Enter, Return, Escape, Backspace, Delete, or one printable grapheme.');
  });

  it('unwraps target responses into one stable result object', () => {
    expect(
      unwrapDebuggerInputResponse(
        {
          handled: true,
          data: {
            contractVersion: 1,
            handled: true,
            type: 'capabilities',
            action: 'capabilities',
            supportedTypes: Object.values(DebuggerInputType),
            selectorForms: ['elementId'],
          },
        },
        { type: 'capabilities' },
      ),
    ).toEqual({
      contractVersion: 1,
      handled: true,
      type: 'capabilities',
      action: 'capabilities',
      supportedTypes: Object.values(DebuggerInputType),
      selectorForms: ['elementId'],
    });

    expect(unwrapDebuggerInputResponse({ handled: false }, { type: 'tap', elementId: 3 })).toEqual({
      handled: false,
      type: 'tap',
      elementId: 3,
      message: 'The target app did not register the Valdi debugger input handler.',
    });
  });
});
