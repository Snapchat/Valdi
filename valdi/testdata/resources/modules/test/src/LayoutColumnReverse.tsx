import { Component } from 'valdi_core/src/Component';

export class LayoutColumnReverse extends Component {
  onRender() {
    <view flexDirection="column-reverse">
      <view id="first" width={100} height={50}/>
      <view id="second" width={100} height={50}/>
      <view id="third" width={100} height={50}/>
    </view>
  }
}
