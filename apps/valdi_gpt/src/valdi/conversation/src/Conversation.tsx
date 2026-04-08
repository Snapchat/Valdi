import { StatefulComponent } from 'valdi_core/src/Component';
import { OpenAi } from 'ai_service/src/OpenAi';
import { Message, MesssageViewModel } from './Message';
import { InputBar } from './InputBar';
import { AiService, GptRole } from '../../ai_service/src/AiService';

export interface ConversationViewModel { }

export interface ConversationContext { }

interface ConversationState {
  results: MesssageViewModel[];
  awaitingResponse: boolean;
}

export class Conversation extends StatefulComponent<ConversationViewModel, ConversationState, ConversationContext> {
  aiService: AiService = new OpenAi();

  onCreate(): void {
    this.aiService.addSystemPrompt('You are a text message companion. Reply concisely.');
    this.setState({
      results: [],
      awaitingResponse: false,
    });
  }

  onRender(): void {
    <layout width="100%" height="100%" flexDirection="column-reverse" padding={16}>
      <InputBar onSubmit={this.onSubmit} />
      <scroll>
        <view flexDirection="column-reverse" paddingTop={20}>
          {[...this.state!.results].reverse().map(message => (
            <Message text={message.text} outbound={message.outbound} />
          ))}
        </view>
      </scroll>
    </layout>;
  }

  onSubmit: (text: string) => void = async (text: string) => {
    const messages = this.state!.results.concat([{ text: text, outbound: true }]);
    this.setState({
      results: messages,
      awaitingResponse: true,
    });

    const gptMessages = messages.map(message => ({
      role: message.outbound ? GptRole.USER : GptRole.ASSISTANT,
      content: message.text,
    }));

    let results: MesssageViewModel[];
    try {
      const response = await this.aiService.getResponse(gptMessages);
      results = messages.concat([{ text: response, outbound: false }]);
    } catch (e) {
      results = messages.concat([{ text: `Error, ${e}`, outbound: false }]);
    }

    this.setState({
      results: results,
      awaitingResponse: false,
    });
  };
}
