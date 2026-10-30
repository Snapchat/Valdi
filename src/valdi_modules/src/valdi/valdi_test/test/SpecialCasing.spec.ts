import 'jasmine/src/jasmine';
import { supportsLocaleCasing } from './EngineSupport';

/**
 * Full Unicode SpecialCasing.txt coverage:
 *   - Turkic (tr, az): i↔İ, I↔ı, Before_Dot, After_I
 *   - Lithuanian (lt): More_Above dot insertion, precomposed Ì/Í/Ĩ decomposition
 *   - Final_Sigma (locale-independent, regression check)
 *
 * On QuickJS this exercises the vendored patch. On macOS/JSC these
 * assertions match native Intl behavior — both engines must agree.
 */

// Skipped where the engine has no locale-aware casing; the non-locale entry points still run.
const describeIfLocaleCasing = supportsLocaleCasing ? describe : xdescribe;

describeIfLocaleCasing('Turkic (tr, az) SpecialCasing', () => {
  describe('uppercase', () => {
    it("maps 'i' to 'İ' under 'tr'", () => {
      expect('i'.toLocaleUpperCase('tr')).toBe('İ');
    });

    it("maps 'i' to 'İ' under 'az'", () => {
      expect('i'.toLocaleUpperCase('az')).toBe('İ');
    });

    it("leaves 'ı' (dotless i) mapping to plain 'I'", () => {
      expect('ı'.toLocaleUpperCase('tr')).toBe('I');
    });

    it('uppercases production reason string', () => {
      expect('benimle birlikte 1+ grupta'.toLocaleUpperCase('tr')).toBe(
        'BENİMLE BİRLİKTE 1+ GRUPTA',
      );
    });
  });

  describe('lowercase', () => {
    it("maps 'İ' to 'i' as a single codepoint", () => {
      expect('İ'.toLocaleLowerCase('tr')).toBe('i');
    });

    it("maps 'I' to 'ı' when not followed by dot above", () => {
      expect('I'.toLocaleLowerCase('tr')).toBe('ı');
    });

    it("lowercases 'İSTANBUL' correctly", () => {
      expect('İSTANBUL'.toLocaleLowerCase('tr')).toBe('istanbul');
    });

    it("absorbs 'I' + U+0307 into 'i' (After_I + Before_Dot)", () => {
      // Decomposed İ = I + combining dot above. Under tr:
      //   - I is NOT lowercased to ı because Before_Dot triggers.
      //   - The U+0307 that follows is absorbed by After_I.
      // Result is a single 'i'.
      expect('İ'.toLocaleLowerCase('tr')).toBe('i');
    });

    // Skipped on Hermes, the default engine: its Turkic lowercase keeps the dot after a non-Above mark.
    xit("preserves 'I' + non-Above mark + dot as absorbed", () => {
      // I + U+0327 (cedilla, ccc=202 Below) + U+0307.
      // Before_Dot allows non-Above marks to intervene, so I stays;
      // After_I ignores non-Above intervening marks and absorbs 0307.
      // Result: i + cedilla.
      expect('İ̧'.toLocaleLowerCase('tr')).toBe('i̧');
    });

    it("does NOT absorb U+0307 when an Above mark intervenes", () => {
      // I + U+0301 (acute, ccc=230 Above) + U+0307. Above intervening
      // breaks Before_Dot AND After_I:
      //   - Before_Dot fails → I becomes ı
      //   - After_I fails → U+0307 stays
      expect('Í̇'.toLocaleLowerCase('tr')).toBe('ı́̇');
    });
  });
});

describeIfLocaleCasing('Lithuanian (lt) SpecialCasing', () => {
  describe('lowercase', () => {
    it("adds combining dot when lowercasing 'I' with More_Above", () => {
      // I + acute → i + dot above + acute (Lithuanian dot preservation).
      expect('Í'.toLocaleLowerCase('lt')).toBe('i̇́');
    });

    it("adds combining dot when lowercasing 'J' with More_Above", () => {
      expect('J́'.toLocaleLowerCase('lt')).toBe('j̇́');
    });

    it("adds combining dot when lowercasing 'Į' with More_Above", () => {
      expect('Į́'.toLocaleLowerCase('lt')).toBe('į̇́');
    });

    it("does NOT add dot without a following Above mark", () => {
      expect('I'.toLocaleLowerCase('lt')).toBe('i');
    });

    it("decomposes 'Ì' to 'i' + dot + grave", () => {
      expect('Ì'.toLocaleLowerCase('lt')).toBe('i̇̀');
    });

    it("decomposes 'Í' to 'i' + dot + acute", () => {
      expect('Í'.toLocaleLowerCase('lt')).toBe('i̇́');
    });

    it("decomposes 'Ĩ' to 'i' + dot + tilde", () => {
      expect('Ĩ'.toLocaleLowerCase('lt')).toBe('i̇̃');
    });
  });
});

describeIfLocaleCasing('Locale-independent rules (regression)', () => {
  it('preserves Greek final sigma under any locale', () => {
    expect('ΟΔΥΣΣΕΥΣ'.toLocaleLowerCase('en')).toBe('οδυσσευς');
    expect('ΟΔΥΣΣΕΥΣ'.toLocaleLowerCase('tr')).toBe('οδυσσευς');
    expect('ΟΔΥΣΣΕΥΣ'.toLocaleLowerCase('lt')).toBe('οδυσσευς');
  });

  // Skipped on Hermes, the default engine: toUpperCase doesn't apply the unconditional ß → SS mapping.
  xit('applies unconditional ß → SS under uppercase', () => {
    expect('ß'.toUpperCase()).toBe('SS');
    expect('ß'.toLocaleUpperCase('en')).toBe('SS');
  });
});

describe('Non-locale entry points ignore extra args', () => {
  it("toUpperCase('tr') maps 'i' to 'I' (locale arg ignored)", () => {
    expect(('i' as any).toUpperCase('tr')).toBe('I');
  });

  it("toLowerCase('tr') maps 'I' to 'i' (locale arg ignored)", () => {
    expect(('I' as any).toLowerCase('tr')).toBe('i');
  });
});

describeIfLocaleCasing('Locale argument shapes (ECMA-402 first-locale-wins)', () => {
  it('accepts a bare tag string', () => {
    expect('i'.toLocaleUpperCase('tr')).toBe('İ');
  });

  it('accepts an array of tags and uses the first', () => {
    expect('i'.toLocaleUpperCase(['tr', 'en'])).toBe('İ');
  });

  it("ignores a Turkic tag beyond index 0", () => {
    expect('i'.toLocaleUpperCase(['en', 'tr'])).toBe('I');
  });

  it('treats an empty array as ROOT', () => {
    expect('i'.toLocaleUpperCase([])).toBe('I');
  });

  it("does not match tags with 'tr' as a prefix but different subtag", () => {
    expect('i'.toLocaleUpperCase('tra')).toBe('I');
    expect('i'.toLocaleUpperCase('trees')).toBe('I');
  });

  it("accepts 'tr-Latn-TR'", () => {
    expect('i'.toLocaleUpperCase('tr-Latn-TR')).toBe('İ');
  });

  // Skipped on Hermes, the default engine: toLocaleUpperCase throws on non-dense locale lists.
  xit('resolves through sparse array holes to the first defined tag', () => {
    // Per ECMA-402 CanonicalizeLocaleList: iterate with HasProperty
    // and skip holes. First defined element wins.
    const sparse = ['tr'];
    delete (sparse as any)[0];
    // sparse now has length=1 with a hole; expect ROOT.
    expect('i'.toLocaleUpperCase(sparse)).toBe('I');

    const sparseWithLater = ['tr'];
    delete (sparseWithLater as any)[0];
    sparseWithLater[1] = 'tr';
    // length=2, hole at 0, 'tr' at 1 → 'tr' wins.
    expect('i'.toLocaleUpperCase(sparseWithLater)).toBe('İ');
  });

  // Skipped on Hermes, the default engine: toLocaleUpperCase throws on array-like locale lists.
  xit('accepts array-like objects (ToObject then iterate)', () => {
    const arrayLike: any = { 0: 'tr', length: 1 };
    expect('i'.toLocaleUpperCase(arrayLike)).toBe('İ');

    const withHole: any = { 1: 'tr', length: 2 };
    expect('i'.toLocaleUpperCase(withHole)).toBe('İ');
  });

  // Skipped on Hermes, the default engine: toLocaleUpperCase throws on undefined/null locale entries.
  xit('skips undefined/null elements when scanning', () => {
    expect('i'.toLocaleUpperCase([undefined as any, 'tr'])).toBe('İ');
    expect('i'.toLocaleUpperCase([null as any, 'tr'])).toBe('İ');
  });
});

describeIfLocaleCasing('Turkic Not_Before_Dot / After_I regressions (PR feedback)', () => {
  // Explicit test cases for Scott's P2 finding on PR #109963:
  // decomposed dotted I must produce plain 'i' under tr/az.

  it("lowercases decomposed 'I\\u0307' to 'i' under 'tr'", () => {
    expect('İ'.toLocaleLowerCase('tr')).toBe('i');
  });

  it("lowercases decomposed 'I\\u0307' to 'i' under 'az'", () => {
    expect('İ'.toLocaleLowerCase('az')).toBe('i');
  });

  it("lowercases 'I' alone to 'ı' (Not_Before_Dot succeeds)", () => {
    expect('I'.toLocaleLowerCase('tr')).toBe('ı');
  });

  it("preserves the dot after non-Turkic 'I' + U+0307 lowercase", () => {
    // Under root, 'I' → 'i' and the combining dot stays as-is.
    expect('İ'.toLocaleLowerCase('en')).toBe('i̇');
  });

  it("lowercases 'INFO' with tr locale", () => {
    // Word 'INFO' — no combining marks. Each 'I' becomes 'ı'.
    expect('INFO'.toLocaleLowerCase('tr')).toBe('ınfo');
  });
});
