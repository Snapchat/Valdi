import { Component } from 'valdi_core/src/Component';
import { ValdiSSRRouter } from 'valdi_ssr/src/ValdiSSRRouter';

interface GreetingViewModel {
  readonly updateCount: number;
}

class Greeting extends Component<GreetingViewModel> {
  onRender(): void {
    <view flexDirection='column' padding={24}>
      <label value='Hello from Valdi SSR' font='system-bold 24' />
      <label value={`Server update ${this.viewModel.updateCount}`} marginTop={12} />
    </view>;
  }
}

const router = new ValdiSSRRouter('Valdi SSR example');

router.add('/', Greeting, () => ({
  componentContext: {},
  viewModel: { updateCount: 0 },
}));

router.add('/stream', Greeting, () => ({
  componentContext: {},
  viewModel: { updateCount: 0 },
  startViewModelStream: render => {
    let updateCount = 0;
    const timer = setInterval(() => {
      updateCount++;
      void render({ updateCount });
      if (updateCount === 5) {
        clearInterval(timer);
      }
    }, 1_000);
    return () => clearInterval(timer);
  },
}));

void router.listen(8080).then(address => {
  console.log(`Valdi SSR example listening at ${address.url}/`);
  console.log(`Streaming example: ${address.url}/stream`);
});
