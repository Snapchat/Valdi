import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { Component } from 'valdi_core/src/Component';

interface ViewModel {
  onLayout: (frame: ElementFrame) => void;
}

export class FrameObserverUpdates extends Component<ViewModel> {
  onRender() {
    <view width='100%' height='100%' onLayout={this.viewModel.onLayout}/>;
  }
}
