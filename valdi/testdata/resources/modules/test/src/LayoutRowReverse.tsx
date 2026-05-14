import { Component } from 'valdi_core/src/Component';

export class LayoutRowReverse extends Component {
  onRender() {
    <view flexDirection="row-reverse">
      <view id="first" width={60} height={40}/>
      <view id="second" width={60} height={40}/>
      <view id="third" width={60} height={40}/>
    </view>
  }
}
