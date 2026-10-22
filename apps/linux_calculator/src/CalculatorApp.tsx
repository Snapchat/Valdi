import { Device } from 'valdi_core/src/Device';
import { StatefulComponent } from 'valdi_core/src/Component';
import { Calculator, CalculatorState } from './Calculator';

interface CalculatorContext {
  connect(handler: ((key: string) => void) | undefined): void;
  reportState(display: string, expression: string): void;
  reportLayout?(id: string, parent: string, x: number, y: number, width: number, height: number): void;
}

const light = { background: '#f4f6fa', text: '#18202e', secondary: '#526079', key: '#e1e6ef', utility: '#cdd5e3', accent: '#f6a94a' };
const dark = { background: '#10151f', text: '#f6f8fc', secondary: '#a5b1c5', key: '#242f43', utility: '#3c475c', accent: '#f6a94a' };

const keys = [
  [['AC', 'clear'], ['±', 'negate'], ['%', 'percent'], ['÷', '/']],
  [['7', '7'], ['8', '8'], ['9', '9'], ['×', '*']],
  [['4', '4'], ['5', '5'], ['6', '6'], ['−', '-']],
  [['1', '1'], ['2', '2'], ['3', '3'], ['+', '+']],
  [['⌫', 'Backspace'], ['0', '0'], ['.', '.'], ['=', '=']],
];

/** Plain Valdi UI; the application context supplies optional desktop inputs. */
export class CalculatorApp extends StatefulComponent<object, CalculatorState & {darkMode: boolean}, CalculatorContext> {
  private readonly calculator = new Calculator();
  private appearance?: {cancel(): void};
  state = {...this.calculator.snapshot(), darkMode: Device.isDarkMode()};

  onCreate(): void {
    this.appearance = Device.observeDarkMode(darkMode => this.setState({darkMode}));
    this.context.connect(this.press);
    this.context.reportState(this.state.display, this.state.expression);
  }

  onDestroy(): void {
    this.appearance?.cancel();
    this.appearance = undefined;
    this.context.connect(undefined);
  }

  private readonly press = (key: string): void => {
    const state = this.calculator.press(key);
    this.setState(state);
    this.context.reportState(state.display, state.expression);
  };

  onRender(): void {
    const colors = this.state.darkMode ? dark : light;
    <view width='100%' height='100%' padding={20} backgroundColor={colors.background}>
      <label height={24} marginBottom={12} value='CALCULATOR' font='Window 14' color={colors.secondary} />
      <view height={100} marginBottom={16} justifyContent='flex-end'>
        <label height={24} value={this.state.expression} font='Window 16' color={colors.secondary} textAlign='right' />
        <label height={68} value={this.state.display} font='Window 48' color={colors.text} textAlign='right'
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
                backgroundColor={operator ? colors.accent : index === 0 ? colors.utility : colors.key}
                justifyContent='center' alignItems='center' onTap={() => this.press(key)}
                accessibilityId={`calculator-key-${key}`} accessibilityLabel={label}
                onLayout={frame => this.context.reportLayout?.(key, rowId, frame.x, frame.y, frame.width, frame.height)}>
                <label value={label} font='Window 26' color={operator ? '#18202e' : colors.text} />
              </view>;
            })}
          </layout>;
        })}
      </layout>
      <label height={20} marginTop={12} value='Enter: calculate  ·  Esc: clear  ·  Backspace: delete'
        font='Window 11' color={colors.secondary} textAlign='center' />
    </view>;
  }
}
