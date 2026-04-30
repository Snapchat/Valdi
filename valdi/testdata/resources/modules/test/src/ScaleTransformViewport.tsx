import { Component } from 'valdi_core/src/Component';

interface ViewModel {
  scaleY: number;
}

export class ScaleTransformViewport extends Component<ViewModel> {

  onRender() {
    <view width={100} height={100}>
      <view width={100} height={200} scaleY={this.viewModel.scaleY}>
        <view width={100} height={150}/>
        <view id="bottom-child" width={100} height={50}/>
      </view>
    </view>
  }

}
