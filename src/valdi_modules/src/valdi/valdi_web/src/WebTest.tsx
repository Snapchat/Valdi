import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { Label, View, ImageView } from 'valdi_tsx/src/NativeTemplateElements';
import { getOtherKitten } from './ValdiWeb';

export interface NewModuleViewModel {
  title: string;
}

export class WebTest extends Component<NewModuleViewModel> {
  onCreate(): void {
    // Called when the component is created in the render-tree
    // Used to prepare some resources, precomputed values or subscribe to observables
  }
  onDestroy(): void {
    // Called the component is not longer in the render-tree
    // Used to dispose of resources and observables
  }
  onViewModelUpdate(_previous?: NewModuleViewModel): void {
    // Called every time the view model changes
    // Used to trigger some computations, typically used for asynchronous operations,
    // that would then impact the internal component state
  }

  onRender(): void {
    // Called every time the component needs to be rendered
    // Used to define child components and child render-tree
    // Can access "this.viewModel" that contains the current component parameters values
    <view style={styles.root}>
      <label style={styles.label} value="totes" />
      <image src={getOtherKitten()} style={styles.img}></image>
    </view>;
  }
}

const styles = {
  root: new Style<View>({
    backgroundColor: 'blue',
    width: '100%',
    height: '100%',
  }),
  label: new Style<Label>({
    font: systemFont(20),
    color: 'white',
  }),
  img: new Style<ImageView>({
    width: '200px',
    height: '200px',
  }),
};
