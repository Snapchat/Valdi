import { StatefulComponent } from 'valdi_core/src/Component';
import { Calculator, CalculatorState } from './Calculator';

interface CalculatorContext {
  connect(handler: ((key: string) => void) | undefined): void;
  reportState(display: string, expression: string): void;
  reportLayout?(id: string, parent: string, x: number, y: number, width: number, height: number): void;
}

const keys = [
  [['AC', 'clear'], ['±', 'negate'], ['%', 'percent'], ['÷', '/']],
  [['7', '7'], ['8', '8'], ['9', '9'], ['×', '*']],
  [['4', '4'], ['5', '5'], ['6', '6'], ['−', '-']],
  [['1', '1'], ['2', '2'], ['3', '3'], ['+', '+']],
  [['⌫', 'Backspace'], ['0', '0'], ['.', '.'], ['=', '=']],
];

/** Plain Valdi UI; the application context supplies optional desktop inputs. */
export class CalculatorApp extends StatefulComponent<object, CalculatorState, CalculatorContext> {
  private readonly calculator = new Calculator();
  state = this.calculator.snapshot();

  onCreate(): void {
    this.context.connect(this.press);
    this.context.reportState(this.state.display, this.state.expression);
  }

  onDestroy(): void {
    this.context.connect(undefined);
  }

  private readonly press = (key: string): void => {
    const state = this.calculator.press(key);
    this.setState(state);
    this.context.reportState(state.display, state.expression);
  };

  onRender(): void {
    <view width='100%' height='100%' padding={20} backgroundColor='#10151f'>
      <label height={24} marginBottom={12} value='CALCULATOR' font='Window 14' color='#a5b1c5' />
      <view height={100} marginBottom={16} justifyContent='flex-end'>
        <label height={24} value={this.state.expression} font='Window 16' color='#a5b1c5' textAlign='right' />
        <label height={68} value={this.state.display} font='Window 48' color='#f6f8fc' textAlign='right'
          adjustsFontSizeToFitWidth={true} accessibilityId='calculator-display' />
      </view>
      <layout flexGrow={1}
        onLayout={frame => this.context.reportLayout?.('keypad', '', frame.x, frame.y, frame.width, frame.height)}>
        {keys.forEach((row, index) => {
          const rowId = `row-${index}`;
          <layout flexDirection='row' flexGrow={1} marginBottom={index < 4 ? 8 : 0}
            onLayout={frame => this.context.reportLayout?.(rowId, 'keypad', frame.x, frame.y, frame.width, frame.height)}>
            {row.forEach(([label, key], column) => {
              const operator = ['+', '-', '*', '/', '='].includes(key);
              <view flexGrow={1} flexBasis={0} marginRight={column < 3 ? 8 : 0} borderRadius={12}
                backgroundColor={operator ? '#f6a94a' : index === 0 ? '#3c475c' : '#242f43'}
                justifyContent='center' alignItems='center' onTap={() => this.press(key)}
                accessibilityId={`calculator-key-${key}`} accessibilityLabel={label}
                onLayout={frame => this.context.reportLayout?.(key, rowId, frame.x, frame.y, frame.width, frame.height)}>
                <label value={label} font='Window 26' color={operator ? '#10151f' : '#f6f8fc'} />
              </view>;
            })}
          </layout>;
        })}
      </layout>
      <label height={20} marginTop={12} value='Enter: calculate  ·  Esc: clear  ·  Backspace: delete'
        font='Window 11' color='#a5b1c5' textAlign='center' />
    </view>;
  }
}
