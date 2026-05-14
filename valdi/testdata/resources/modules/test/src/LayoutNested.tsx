import { Component } from 'valdi_core/src/Component';

export class LayoutNested extends Component {
  onRender() {
    <view flexDirection="column">
      <view id="topRow" flexDirection="row" height={100}>
        <view id="left" flexGrow={1}/>
        <view id="right" flexGrow={1}/>
      </view>
      <view id="bottomCol" flexDirection="column" flexGrow={1}>
        <view id="topItem" height={100}/>
        <view id="bottomItem" flexGrow={1}/>
      </view>
    </view>
  }
}
