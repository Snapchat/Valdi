export function formatNumber(value: number, fractionalDigits: number): string {
  if (fractionalDigits === -1) {
    return value.toLocaleString();
  }

  return value.toLocaleString(undefined, {
    minimumFractionDigits: fractionalDigits,
    maximumFractionDigits: fractionalDigits,
  });
}

export function formatNumberWithCurrency(
  value: number,
  currencyCode: string,
  minimumFractionDigits?: number,
  maximumFractionDigits?: number,
  localeIdentifier?: string,
): string {
  const locale = localeIdentifier?.split('@')[0].replace(/_/g, '-');
  return new Intl.NumberFormat(locale, {
    style: 'currency',
    currency: currencyCode,
    minimumFractionDigits,
    maximumFractionDigits,
  }).format(value);
}
