import { HTTPClient } from 'valdi_http/src/HTTPClient';
import { AiService, GptMessage, GptRole } from './AiService';

const OPENAI_MODEL = 'gpt-4o-mini';
const OPENAI_API_KEY = 'YOUR_API_KEY_HERE';

interface ChatCompletionResponse {
  choices?: { message: { content: string } }[];
}

/**
 * Provides barebones access to OpenAI, using direct HTTP requests.
 * A much better implementation would use the OpenAI SDK:
 *    https://github.com/openai/openai-node
 * but Valdi currently isn't compatible with Node modules.
 *
 * API reference: https://platform.openai.com/docs/api-reference/making-requests
 */
export class OpenAi implements AiService {
  private systemMessage: GptMessage[] = [];
  private client = new HTTPClient('https://api.openai.com/v1');

  public addSystemPrompt(message: string): void {
    this.systemMessage.push({
      role: GptRole.SYSTEM,
      content: message,
    });
  }

  public async getResponse(messages: GptMessage[]): Promise<string> {
    const requestBody = JSON.stringify({
      model: OPENAI_MODEL,
      messages: this.systemMessage.concat(messages),
    });

    console.log('Request:', requestBody);

    const encodedRequestBody = new TextEncoder().encode(requestBody);

    try {
      const response = await this.client.post('/chat/completions', encodedRequestBody, {
        'Content-Type': 'application/json',
        // HTTP header names are not camelCase.
        // eslint-disable-next-line @typescript-eslint/naming-convention
        Authorization: `Bearer ${OPENAI_API_KEY}`,
      });
      const body = response.body;
      if (body) {
        const text = new TextDecoder().decode(body);
        const results = JSON.parse(text) as ChatCompletionResponse;
        if (results.choices) {
          return results.choices[0].message.content;
        } else {
          return `Error: ${text}\n\nRequest:\n\n ${requestBody}`;
        }
      } else {
        return '????';
      }
    } catch (err: unknown) {
      return (err as Error).toString();
    }
  }
}
