import 'jasmine';
import { CliError } from '../src/core/errors';
import { requireFlagsWhenNonInteractive } from '../src/utils/nonInteractive';

describe('requireFlagsWhenNonInteractive', () => {
  const flags = [
    { flag: '--yes', provided: true },
    { flag: '--template <type>', provided: false },
    { flag: '<module-name>', provided: false },
  ];

  it('does nothing in an interactive terminal', () => {
    expect(() => requireFlagsWhenNonInteractive('new_module', flags, true)).not.toThrow();
  });

  it('does nothing without a TTY when every flag is provided', () => {
    const provided = flags.map(f => ({ ...f, provided: true }));
    expect(() => requireFlagsWhenNonInteractive('new_module', provided, false)).not.toThrow();
  });

  it('lists only the missing flags without a TTY', () => {
    let error: unknown;
    try {
      requireFlagsWhenNonInteractive('new_module', flags, false);
    } catch (e) {
      error = e;
    }

    expect(error).toBeInstanceOf(CliError);
    const message = (error as CliError).message;
    expect(message).toContain("'new_module'");
    expect(message).toContain('--template <type> <module-name>');
    expect(message).not.toContain('--yes');
  });
});
