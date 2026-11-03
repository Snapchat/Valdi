import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageStatefulComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { load } from 'valdi_protobuf/src/ProtobufBuilder';
import { Label } from 'valdi_tsx/src/NativeTemplateElements';
import { now } from './cpp';

export function runProtoImport(src: string): number {
  const start = now();
  load(src);
  const end = now();
  return end - start;
}

export interface ViewModel {}
export interface ComponentContext {}

interface State {
  importLatency: number;
  importLatencyNoIndex: number;
}

@NavigationPage(module)
export class ProtoImportTest extends NavigationPageStatefulComponent<ViewModel, ComponentContext> {
  state: State = {
    importLatency: 0,
    importLatencyNoIndex: 0,
  };

  onCreate(): void {
    const importLatency = runProtoImport('benchmark/src/proto.protodecl');
    const importLatencyNoIndex = runProtoImport('benchmark/src/proto_noidx.protodecl');
    this.setState({ importLatency: importLatency, importLatencyNoIndex: importLatencyNoIndex });
  }

  onRender(): void {
    <view backgroundColor="white" flexDirection="column">
      <label marginTop={20} style={styles.text} value="1000 messages and 200 enums imported in"></label>
      <label style={styles.text} value={`${this.state.importLatency.toFixed(2)} ms`} />
      <label style={styles.text} value={`${this.state.importLatencyNoIndex.toFixed(2)} ms`} />
    </view>;
  }
}

const styles = {
  text: new Style<Label>({
    font: systemFont(12),
  }),
};
