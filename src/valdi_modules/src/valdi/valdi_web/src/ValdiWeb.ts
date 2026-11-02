//import { Arena } from 'valdi_protobuf/src/Arena';
import { HTTPClient } from 'valdi_http/src/HTTPClient';
export default function bareMinimum(): void {
  //const arena = new Arena();
  console.log('Making a request');
  new HTTPClient('https://google.com').get('/').then(
    response => {
      console.log('success', response);
    },
    error => {
      console.error('request failed', error);
    },
  );
}

export function getKitten(): string {
  return 'http://placecats.com/400/700';
}

export function getOtherKitten(): string {
  return 'http://placecats.com/400/400';
}
