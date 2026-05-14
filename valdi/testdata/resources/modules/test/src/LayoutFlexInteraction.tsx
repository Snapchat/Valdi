import { Component } from 'valdi_core/src/Component';

export class LayoutFlexInteraction extends Component {
  onRender() {
    <view flexDirection="row">
      <view id="fixed" width={80} flexShrink={0}/>
      <view id="flexible" flexGrow={1} flexShrink={1} flexBasis={200}/>
      <view id="capped" flexGrow={1} maxWidth={120}/>
    </view>
  }
}
