import 'jasmine/src/jasmine';
import { DeferredPromise } from 'foundation/src/DeferredPromise';
import { componentTypeFind } from 'foundation/test/util/componentTypeFind';
import { IComponentTestDriver, withValdiRenderer } from 'valdi_test/test/JSXTestUtils';
import { AiService } from '../../ai_service/src/AiService';
import { Conversation, ConversationContext } from '../src/Conversation';
import { Message } from '../src/Message';

class TestAiService implements AiService {
  nextResponse: Promise<string> = Promise.resolve('response');

  addSystemPrompt(): void {}

  getResponse(): Promise<string> {
    return this.nextResponse;
  }
}

describe('Conversation', () => {
  const aiService = new TestAiService();
  const layoutParams = { width: 600, height: 1800 };

  describe('render', () => {
    it(
      'sanity check',
      withValdiRenderer(async driver => {
        const component = renderRoot(driver);

        await driver.performLayout(layoutParams);
        expect(component instanceof Conversation).toEqual(true);
      }),
    );

    it(
      'response is appended',
      withValdiRenderer(async driver => {
        const component = renderRoot(driver);
        component.aiService = aiService;

        await driver.performLayout(layoutParams);
        // verify simple layout
        expect(component instanceof Conversation).toEqual(true);

        // Verify no messages
        let messages = componentTypeFind(component, Message);
        expect(messages.length).toEqual(0);

        // Send a message but defer response
        const nextResponse = new DeferredPromise<string>();
        aiService.nextResponse = nextResponse.promise;
        component.onSubmit('knock knock.');

        // Verify one message appended
        await driver.performLayout(layoutParams);
        messages = componentTypeFind(component, Message);
        expect(messages.length).toEqual(1);
        const outboundViewModel = messages[0].viewModel;
        expect(outboundViewModel.outbound).toEqual(true);
        expect(outboundViewModel.text).toEqual('knock knock.');

        // Release response
        nextResponse.resolve("who's there?");
        await driver.performLayout(layoutParams);
        messages = componentTypeFind(component, Message);

        // Verify response appended
        expect(messages.length).toEqual(2);
        expect(messages[0].viewModel.outbound).toEqual(false);
        expect(messages[0].viewModel.text).toEqual("who's there?");

        // Also make sure our outbound message is the same
        expect(messages[1].viewModel).toEqual(outboundViewModel);
      }),
    );
  });
});

/**
 * Helper function to render the root component and ensure its a
 * Conversation
 *
 * @param driver driver to render
 * @param context Uses a default mock context
 */
function renderRoot(driver: IComponentTestDriver, context = mockContext()): Conversation {
  const nodes = driver.render(() => {
    <Conversation context={context} />;
  });
  if (nodes[0]?.component instanceof Conversation) {
    return nodes[0].component;
  }
  throw new Error('could not render Conversation');
}

/**
 * Helper function to return a mock context object for ContentDiscoverComponent
 * @returns {ConversationContext}
 */
function mockContext(): ConversationContext {
  return {};
}
