const puppeteer = require('puppeteer');

const BASE_URL = process.env.BASE_URL || 'http://localhost:3030';
const TIMEOUT = 30000;
const LAUNCH_TIMEOUT = 30000;
const BEFOREALL_TIMEOUT = 45000;

const LAUNCH_OPTIONS = {
  headless: true,
  args: [
    '--no-sandbox',
    '--disable-setuid-sandbox',
    '--disable-dev-shm-usage',
    '--disable-accelerated-2d-canvas',
    '--disable-gpu',
    '--single-process',
    '--no-zygote',
  ],
  timeout: LAUNCH_TIMEOUT,
};

async function launchBrowser() {
  console.log('Launching Puppeteer browser...');
  
  const launchOptions = { ...LAUNCH_OPTIONS };
  
  if (process.env.PUPPETEER_EXECUTABLE_PATH) {
    console.log(`Using Chrome Headless Shell at: ${process.env.PUPPETEER_EXECUTABLE_PATH}`);
    launchOptions.executablePath = process.env.PUPPETEER_EXECUTABLE_PATH;
  } else {
    console.log('Using Puppeteer bundled Chrome');
  }
  
  try {
    const browser = await puppeteer.launch(launchOptions);
    console.log('Browser launched successfully');
    return browser;
  } catch (error) {
    console.error('Failed to launch browser:', error);
    throw error;
  }
}

function setupPageLogging(page) {
  page.on('console', msg => {
    const type = msg.type();
    if (type === 'error' || type === 'warning') {
      console.log(`[Browser ${type}]:`, msg.text());
    }
  });

  page.on('pageerror', error => {
    console.error('[Browser Error]:', error.message);
  });
}

function createErrorCollector(page) {
  const errors = [];
  
  page.on('pageerror', error => {
    errors.push(error.message);
  });

  page.on('console', msg => {
    if (msg.type() === 'error') {
      errors.push(msg.text());
    }
  });
  
  return {
    getErrors: () => errors,
    getRealErrors: () => errors.filter(e => 
      !e.includes('favicon.ico') && 
      !e.includes('Failed to load resource: the server responded with a status of 404')
    ),
  };
}

async function waitForValdiBootstrap(page, timeout = 10000) {
  await page.waitForFunction(
    () => window.__valdiTestBootstrapStatus && window.__valdiTestBootstrapStatus.loaded,
    { timeout }
  );
}

async function getTestStatus(page) {
  return page.evaluate(() => ({
    valdiStatus: window.valdiTestStatus,
    bootstrapStatus: window.__valdiTestBootstrapStatus,
    appContent: document.getElementById('app') ? document.getElementById('app').textContent : ''
  }));
}

module.exports = {
  BASE_URL,
  TIMEOUT,
  LAUNCH_TIMEOUT,
  BEFOREALL_TIMEOUT,
  launchBrowser,
  setupPageLogging,
  createErrorCollector,
  waitForValdiBootstrap,
  getTestStatus,
};
