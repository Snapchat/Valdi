import { Component } from 'valdi_core/src/Component';

interface ViewModel {
  showItem1: boolean;
  showItem2: boolean;
}

export class LayoutDynamic extends Component<ViewModel> {
  onRender() {
    <view flexDirection="column">
      <view id="header" height={50}/>
      {this.viewModel.showItem1 && <view id="item1" height={60}/>}
      {this.viewModel.showItem2 && <view id="item2" height={60}/>}
      <view id="footer" flexGrow={1}/>
    </view>
  }
}
