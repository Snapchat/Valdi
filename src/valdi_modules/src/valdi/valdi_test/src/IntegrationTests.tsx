import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { createReusableCallback } from 'valdi_core/src/utils/Callback';
import { Label, View } from 'valdi_tsx/src/NativeTemplateElements';

/**
 * @ExportModel
 */
export interface Entry {
  color: string;
  text: string;
}

/**
 * @ExportModel
 * @ViewModel
 */
export interface ViewModel {
  headerTitle: string;
  scrollable: boolean;
  entries: Entry[];
}

/**
 * @ExportProxy
 */
export interface Listener {
  onRender(): void;
}

/**
 * @ExportModel
 * @Context
 */
export interface Context {
  listener: Listener;
  onTap(index: number): void;
}

/**
 * @ExportModel
 * @Component
 */
export class IntegrationTests extends Component<ViewModel, Context> {
  onRender(): void {
    this.context.listener.onRender();

    <view style={styles.root}>
      <label style={styles.header} value={this.viewModel.headerTitle} />
      <view style={styles.card}>{this.renderCardBody()}</view>
    </view>;
  }

  private renderCardBody() {
    if (this.viewModel.scrollable) {
      <scroll>{this.renderEntries()}</scroll>;
    } else {
      this.renderEntries();
    }
  }

  private renderEntries() {
    for (const entry of this.viewModel.entries) {
      <label
        style={styles.cardLabel}
        color={entry.color}
        value={entry.text}
        onTap={createReusableCallback(() => {
          this.onTapCard(entry);
        })}
      />;
    }
  }

  private onTapCard(entry: Entry) {
    const index = this.viewModel.entries.indexOf(entry);
    if (index >= 0) {
      this.context.onTap(index);
    }
  }
}

const styles = {
  root: new Style<View>({
    backgroundColor: 'white',
    flexShrink: 1,
    alignItems: 'center',
  }),
  header: new Style<Label>({
    font: 'Montserrat-Bold 16',
    margin: 10,
  }),
  card: new Style<View>({
    backgroundColor: 'lightgray',
    borderRadius: 16,
    padding: 16,
    flexShrink: 1,
  }),
  cardLabel: new Style<Label>({
    touchEnabled: true,
    font: 'Montserrat-Medium 14',
    numberOfLines: 0,
  }),
};
