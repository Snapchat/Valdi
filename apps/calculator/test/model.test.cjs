const assert = require('node:assert/strict');
const { test } = require('node:test');
const { Calculator } = require(process.argv[2]);

function calculate(keys) {
  const calculator = new Calculator();
  for (const key of keys) calculator.press(key);
  return calculator.snapshot();
}

test('addition and expression', () => {
  assert.deepEqual(calculate(['1', '2', '+', '3', '=']), { display: '15', expression: '12 + 3 =' });
});
test('clear resets a pending operation', () => assert.equal(calculate(['9', '/', 'clear', '2', '=', '3']).display, '23'));
test('backspace and fresh results', () => {
  assert.equal(calculate(['1', '2', 'Backspace', '+', '3', '=']).display, '4');
  assert.equal(calculate(['1', '+', '2', '=', '4']).display, '4');
});
test('decimal rounding and duplicate decimal', () => assert.equal(calculate(['0', '.', '.', '1', '+', '0', '.', '2', '=']).display, '0.3'));
test('division by zero recovers', () => {
  assert.equal(calculate(['8', '/', '0', '=']).display, 'Error');
  assert.equal(calculate(['8', '/', '0', '=', '2', '+', '3', '=']).display, '5');
});
test('operator replacement and chaining', () => {
  assert.equal(calculate(['8', '+', '-', '3', '=']).display, '5');
  assert.equal(calculate(['2', '+', '3', '*', '4', '=']).display, '20');
});
test('sign, percent, bounded digits and unknown input', () => {
  assert.equal(calculate(['5', 'negate', 'percent']).display, '-0.05');
  assert.equal(calculate(Array(100).fill('9')).display.length, 16);
  assert.equal(calculate(['1', 'unexpected']).display, '1');
});
