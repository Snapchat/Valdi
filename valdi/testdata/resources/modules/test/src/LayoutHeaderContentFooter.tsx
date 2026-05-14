import { Component } from 'valdi_core/src/Component';

export class LayoutHeaderContentFooter extends Component {
  onRender() {
    <view flexDirection="column">
      <view id="header" height={60}/>
      <view id="content" flexGrow={1}/>
      <view id="footer" height={40}/>
    </view>
  }
}
