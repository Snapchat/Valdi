/**
 * Takes in a number representing milliseconds and returns the length of time in "HH:MM:SS" format in the form of a string
 * @param ms The number of milliseconds
 * @returns A string showing the length of time represented by ms in "HH:MM:SS" format
 */
export function formatDurationMs(ms: number): string {
  const seconds = ms / 1000;
  return formatDurationSeconds(seconds);
}

/**
 * Takes in a number representing seconds and returns the length of time in "HH:MM:SS" format in the form of a string
 * @param seconds The number of seconds
 * @returns A string showing the length of time represented by seconds in "HH:MM:SS" format
 */
export function formatDurationSeconds(seconds: number): string {
  const totalSeconds = Math.max(0, Math.round(seconds));
  const h = Math.floor(totalSeconds / 3600);
  const m = Math.floor((totalSeconds % 3600) / 60);
  const s = totalSeconds % 60;
  return [h, m > 9 ? m : h ? `0${m}` : m || '0', s > 9 ? s : `0${s}`].filter(Boolean).join(':');
}
