import { Component } from 'valdi_core/src/Component';

export class LayoutSidebar extends Component {
  onRender() {
    <view flexDirection="row">
      <view id="sidebar" width={100}/>
      <view id="main" flexGrow={1}/>
    </view>
  }
}
