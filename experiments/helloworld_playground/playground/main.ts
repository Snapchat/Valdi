import './setup';
import * as helloPage from './pages/hello';

function init() {
  const container = document.getElementById('app');
  if (!container) {
    (window as any).valdiTestStatus = 'error';
    (window as any).__valdiTestBootstrapStatus = { loaded: false, error: 'Missing #app container' };
    console.error('Missing #app container');
    return;
  }
  container.innerHTML = '';
  try {
    helloPage.render(container);
    (window as any).valdiTestStatus = 'success';
    (window as any).__valdiTestBootstrapStatus = { loaded: true, error: null };
  } catch (error: any) {
    (window as any).valdiTestStatus = 'error';
    (window as any).__valdiTestBootstrapStatus = { loaded: false, error: error?.message ?? String(error) };
    throw error;
  }
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
