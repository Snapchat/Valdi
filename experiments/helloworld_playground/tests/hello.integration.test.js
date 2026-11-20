const {
  BASE_URL,
  TIMEOUT,
  BEFOREALL_TIMEOUT,
  launchBrowser,
  setupPageLogging,
  createErrorCollector,
  waitForValdiBootstrap,
  getTestStatus,
} = require('valdi-web-devtools/test_utils/puppeteer_helpers');

describe('helloworld_playground — Valdi minimal wiring', () => {
  let browser;
  let page;

  beforeAll(async () => {
    browser = await launchBrowser();
  }, BEFOREALL_TIMEOUT);

  afterAll(async () => {
    if (browser) await browser.close();
  });

  beforeEach(async () => {
    page = await browser.newPage();
    setupPageLogging(page);
  });

  afterEach(async () => {
    if (page) await page.close();
  });

  test('boots without JS errors and marks bootstrap loaded', async () => {
    const errorCollector = createErrorCollector(page);

    await page.goto(BASE_URL, { waitUntil: 'networkidle2', timeout: TIMEOUT });
    await waitForValdiBootstrap(page);

    const testStatus = await getTestStatus(page);
    expect(testStatus.valdiStatus).toBe('success');
    expect(testStatus.bootstrapStatus.loaded).toBe(true);
    expect(testStatus.bootstrapStatus.error).toBeNull();

    const realErrors = errorCollector.getRealErrors();
    if (realErrors.length > 0) console.error('Errors found:', realErrors);
    expect(realErrors).toHaveLength(0);
  }, TIMEOUT);

  test('attaches a shadow root to #app for Valdi rendering', async () => {
    await page.goto(BASE_URL, { waitUntil: 'networkidle2', timeout: TIMEOUT });
    await waitForValdiBootstrap(page);

    const hasShadowRoot = await page.evaluate(() => {
      const app = document.getElementById('app');
      return app != null && app.shadowRoot != null;
    });
    expect(hasShadowRoot).toBe(true);
  }, TIMEOUT);

  test('renders the HelloWorld app title', async () => {
    await page.goto(BASE_URL, { waitUntil: 'networkidle2', timeout: TIMEOUT });
    await waitForValdiBootstrap(page);

    // NativeModule stub returns "Valdi Hello World (Web)"; the app renders
    // `Welcome to ${APP_NAME}!`. Read through the shadow root.
    const titleText = await page.evaluate(() => {
      const app = document.getElementById('app');
      if (!app || !app.shadowRoot) return null;
      return app.shadowRoot.textContent || '';
    });

    expect(titleText).not.toBeNull();
    expect(titleText).toContain('Valdi Web');
  }, TIMEOUT);

  test('decodes the byte-exact .bin bitmap (getModuleFileEntryAsBytes + decodeBitmap on web)', async () => {
    await page.goto(BASE_URL, { waitUntil: 'networkidle2', timeout: TIMEOUT });
    await waitForValdiBootstrap(page);

    const text = await page.evaluate(() => {
      const app = document.getElementById('app');
      return app && app.shadowRoot ? app.shadowRoot.textContent || '' : '';
    });

    // checker_2x2.png.bin is a lossless 2x2 black/white checker. A stubbed decode gives one transparent pixel.
    const pixels = [...text.matchAll(/\((\d), (\d)\)\s+->\s+(rgba\([^)]*\))/g)].map(m => `(${m[1]}, ${m[2]}) ${m[3]}`);
    expect(pixels).toEqual([
      '(0, 0) rgba(0, 0, 0, 255)',
      '(1, 0) rgba(255, 255, 255, 255)',
      '(0, 1) rgba(255, 255, 255, 255)',
      '(1, 1) rgba(0, 0, 0, 255)',
    ]);
  }, TIMEOUT);
});
