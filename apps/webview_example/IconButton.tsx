import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { Asset } from 'valdi_tsx/src/Asset';
import { ImageView, View } from 'valdi_tsx/src/NativeTemplateElements';

export interface IconButtonViewModel {
  icon: Asset;
  onTap: () => void;
}

export class IconButton extends Component<IconButtonViewModel> {
  onRender(): void {
    <view style={styles.button} onTap={this.viewModel.onTap}>
      <image style={styles.icon} src={this.viewModel.icon} />
    </view>;
  }
}

const styles = {
  button: new Style<View>({
    width: 40,
    height: 40,
    borderRadius: 8,
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 6,
    backgroundColor: '#eef1f5',
  }),

  icon: new Style<ImageView>({
    width: 22,
    height: 22,
    objectFit: 'contain',
    touchEnabled: false,
  }),
};
