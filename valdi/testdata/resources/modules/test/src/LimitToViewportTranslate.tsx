import { Component } from 'valdi_core/src/Component';

interface ViewModel {
  translationX: number | string;
  translationY: number | string;
}

export class TestComponent extends Component<ViewModel> {

  onRender() {
    <view width={100} height={100}>
      <view width={50} height={50} translationX={this.viewModel.translationX} translationY={this.viewModel.translationY}/>
    </view>
  }

}
