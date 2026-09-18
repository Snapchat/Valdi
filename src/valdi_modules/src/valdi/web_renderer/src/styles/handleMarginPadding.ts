export function handleMarginPadding(attribute: 'margin' | 'padding', value: string): Partial<CSSStyleDeclaration> {
  const split = String(value).split(' ');
  if (split.length === 1) {
    return {
      [`${attribute}Top`]: `${value}px`,
      [`${attribute}Right`]: `${value}px`,
      [`${attribute}Bottom`]: `${value}px`,
      [`${attribute}Left`]: `${value}px`,
    };
  }

  if (split.length === 2) {
    return {
      [`${attribute}Top`]: `${split[0]}px`,
      [`${attribute}Right`]: `${split[1]}px`,
      [`${attribute}Bottom`]: `${split[0]}px`,
      [`${attribute}Left`]: `${split[1]}px`,
    };
  }

  if (split.length === 3) {
    return {
      [`${attribute}Top`]: `${split[0]}px`,
      [`${attribute}Right`]: `${split[1]}px`,
      [`${attribute}Bottom`]: `${split[2]}px`,
      [`${attribute}Left`]: `${split[1]}px`,
    };
  }

  if (split.length === 4) {
    return {
      [`${attribute}Top`]: `${split[0]}px`,
      [`${attribute}Right`]: `${split[1]}px`,
      [`${attribute}Bottom`]: `${split[2]}px`,
      [`${attribute}Left`]: `${split[3]}px`,
    };
  }

  return {};
}
