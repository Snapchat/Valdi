import { Component } from 'valdi_core/src/Component';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';

export class GettingStartedCodelab extends Component {
  onRender(): void {
    <layout padding={24} paddingTop={80}>
      <label value="Getting started codelab!" font={systemBoldFont(16)} />
      <label value="gitub.com/Snapchat/Valdi" font={systemFont(12)} />
    </layout>;
  }
}
