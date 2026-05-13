import { Label, View } from 'valdi_tsx/src/NativeTemplateElements';
import { Style } from '../Style';

export const fonts = {
  body: 'RobotoMono-Regular 12',
  medium: 'RobotoMono-Bold 12',
  title: 'RobotoMono-Bold 16',
};

export const colors = {
  errorText: '#ff0000',
  infoText: 'white',
  tagBracket: '#808080',
  tagComponent: '#4EC9B0',
  tagElement: '#569cd6',
  comment: '#6A9955',

  expression: '#C8C8C8',
  attributeName: '#9CDCFE',
  attributeValue: '#CE9178',

  special: '#C586C0',
};

export const styles = {
  console: new Style<View>({
    backgroundColor: '#1E1E1E',
    padding: 8,
    borderRadius: 8,
  }),
  mainBackground: new Style<View>({
    backgroundColor: '#252526',
  }),
  consoleText: new Style<Label>({
    color: '#D4D4D4',
    font: fonts.body,
  }),

  infoText: new Style<Label>({
    color: 'black',
    font: fonts.medium,
  }),

  bodyText: new Style<Label>({
    color: 'white',
    font: fonts.body,
  }),

  bodyStrongText: new Style<Label>({
    color: 'white',
    font: fonts.medium,
  }),
};
