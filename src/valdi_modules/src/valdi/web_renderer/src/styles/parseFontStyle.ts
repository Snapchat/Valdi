/**
 * Valdi font strings come in two shapes. Native-facing callers name a concrete
 * font face and, optionally, a dynamic-type scale:
 * `"<PostScriptName> <size> [<textStyle>] [<maxSize>]"`, for example
 * `"AvenirNext-DemiBold 16"` or `"AvenirNext-Medium 16 Body"`. Web-facing
 * callers write CSS: `"<family...> <size> [<weight>]"`, for example
 * `"sans-serif 32 bold"` or `"Times New Roman 16"`.
 *
 * Native resolves a PostScript name straight to a face, so in the first shape
 * the weight rides in the name. CSS matches on family, so the name has to be
 * turned back into a family plus a numeric weight, with fallbacks for hosts
 * that don't have the face installed.
 */

/** Weight suffixes used by PostScript face names, in CSS numeric weights. */
const WEIGHT_BY_SUFFIX: Record<string, number> = {
  ultralight: 200,
  thin: 100,
  light: 300,
  regular: 400,
  book: 400,
  medium: 500,
  demibold: 600,
  semibold: 600,
  bold: 700,
  heavy: 800,
  black: 900,
};

/** Weights a font string may carry as a literal CSS value. */
const LITERAL_WEIGHTS = new Set([
  'normal',
  'bold',
  'lighter',
  'bolder',
  '100',
  '200',
  '300',
  '400',
  '500',
  '600',
  '700',
  '800',
  '900',
]);

/** Last resort when the named face isn't installed. */
const GENERIC_FALLBACK = 'sans-serif';

export type ParsedFontStyle = {
  fontFamily: string;
  fontSize: string;
  fontWeight: string;
};

export function isWeightToken(token: string): boolean {
  return LITERAL_WEIGHTS.has(token.toLowerCase());
}

function isSizeToken(token: string): boolean {
  return !Number.isNaN(Number(token));
}

function quoteFamily(family: string): string {
  return /[^A-Za-z0-9-]/.test(family) ? `"${family}"` : family;
}

/**
 * Splits `AvenirNext-DemiBold` into its base name and weight. Returns an
 * undefined weight when the trailing segment isn't a recognized weight, so a
 * hyphenated family that doesn't encode one is left alone.
 */
function splitFace(postScriptName: string): { base: string; weight?: number } {
  const separator = postScriptName.lastIndexOf('-');
  if (separator <= 0) {
    return { base: postScriptName };
  }

  const weight = WEIGHT_BY_SUFFIX[postScriptName.slice(separator + 1).toLowerCase()];
  if (weight === undefined) {
    return { base: postScriptName };
  }

  return { base: postScriptName.slice(0, separator), weight };
}

/** `AvenirNext` -> `Avenir Next`, the name the face is installed under on most hosts. */
function displayName(base: string): string {
  return base.replace(/([a-z0-9])([A-Z])/g, '$1 $2');
}

export function parseFontStyle(value: string): ParsedFontStyle {
  const tokens = value.split(/\s+/).filter((token) => token.length > 0);

  // The size anchors the string: the family runs up to it and may be multi-word
  // (`Times New Roman 16`), and what follows is a trailer. Only the first
  // trailer can be a weight — in the native shape that slot holds a
  // dynamic-type style name, and the max size after it has no CSS meaning.
  // Scanning from token 1 leaves a single-token font string as a family.
  const sizeIndex = tokens.findIndex((token, index) => index > 0 && isSizeToken(token));

  let familyEnd = tokens.length;
  let fontSize: string | undefined;
  let literalWeight: string | undefined;

  if (sizeIndex !== -1) {
    familyEnd = sizeIndex;
    fontSize = tokens[sizeIndex];
    const trailer = tokens[sizeIndex + 1];
    literalWeight = trailer !== undefined && isWeightToken(trailer) ? trailer : undefined;
  } else if (familyEnd > 1 && isWeightToken(tokens[familyEnd - 1])) {
    // No size to anchor on (`sans-serif bold`), so a trailing weight would
    // otherwise be read as the last word of the family.
    literalWeight = tokens[--familyEnd];
  }

  const familyTokens = tokens.slice(0, familyEnd);
  const families: string[] = [];
  let suffixWeight: number | undefined;

  if (familyTokens.length === 1) {
    // One token is a PostScript name, so the weight has to be recovered from it
    // and the installed family name derived alongside it.
    const postScriptName = familyTokens[0];
    const { base, weight } = splitFace(postScriptName);
    suffixWeight = weight;

    families.push(postScriptName);
    const display = displayName(base);
    if (display !== postScriptName) {
      families.push(display);
    }
  } else if (familyTokens.length > 1) {
    families.push(familyTokens.join(' '));
  }

  if (families.length > 0 && !families.includes(GENERIC_FALLBACK)) {
    families.push(GENERIC_FALLBACK);
  }

  // Styles are Object.assign-ed onto the element, so these stay present-but-empty
  // rather than omitted: an absent key would leave the value the previous font set.
  return {
    fontFamily: families.map(quoteFamily).join(', '),
    // The size is optional (`WebValdiLabel` defaults to a bare `Montserrat-SemiBold`),
    // and `${undefined}px` is not a size the browser can use.
    fontSize: fontSize === undefined ? '' : `${fontSize}px`,
    // An explicit weight wins over one encoded in the face name.
    fontWeight: literalWeight?.toLowerCase() ?? (suffixWeight === undefined ? '' : `${suffixWeight}`),
  };
}
