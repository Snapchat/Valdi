import { Component } from 'valdi_core/src/Component';

export class LayoutCardGrid extends Component {
  onRender() {
    <view flexDirection="row" flexWrap="wrap">
      <view id="card1" width={90} height={80}/>
      <view id="card2" width={90} height={80}/>
      <view id="card3" width={90} height={80}/>
      <view id="card4" width={90} height={80}/>
    </view>
  }
}
