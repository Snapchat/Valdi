import { fs } from 'file_system/src/FileSystem';
import { Path } from 'valdi_cli/src/Path';

import type { CaseComparison, ComparisonSummary } from './compare';
import type { IntegrationTestRenderedNode } from './types';

interface HtmlReportCase extends CaseComparison {
  beforeImage?: string;
  afterImage?: string;
  diffImage?: string;
  beforeNodeXml?: string;
  afterNodeXml?: string;
}

interface HtmlReportData {
  before: string;
  after: string;
  beforePlatform: string;
  afterPlatform: string;
  pixelThreshold: number;
  caseCount: number;
  changedCaseCount: number;
  maxDiffPercent: number;
  generatedAt: string;
  elements: string[];
  cases: HtmlReportCase[];
}

function relativeImagePath(folder: string, path: string | undefined): string | undefined {
  if (path === undefined) {
    return undefined;
  }
  return `${folder}/${Path.fromString(path).basename()}`;
}

function scriptJson(value: unknown): string {
  return (JSON.stringify(value) ?? 'null').replace(/</g, '\\u003c');
}

function escapeHtml(value: unknown): string {
  return String(value ?? '').replace(/[&<>"']/g, character => {
    switch (character) {
      case '&':
        return '&amp;';
      case '<':
        return '&lt;';
      case '>':
        return '&gt;';
      case '"':
        return '&quot;';
      case "'":
        return '&#39;';
      default:
        return character;
    }
  });
}

function percent(value: number | undefined): string {
  return `${Number(value ?? 0).toFixed(4)}%`;
}

function escapeXmlAttribute(value: string): string {
  return value.replace(/[&"]/g, character => {
    switch (character) {
      case '&':
        return '&amp;';
      case '"':
        return '&quot;';
      default:
        return character;
    }
  });
}

function nodeToXml(node: IntegrationTestRenderedNode | undefined, depth = 0): string | undefined {
  if (node === undefined) {
    return undefined;
  }

  const indent = '  '.repeat(depth);
  const attributes = Object.keys(node.attributes)
    .sort()
    .map(name => ` ${name}="${escapeXmlAttribute(node.attributes[name] ?? '')}"`)
    .join('');

  if (!node.children.length) {
    return `${indent}<${node.tag}${attributes}/>`;
  }

  return [
    `${indent}<${node.tag}${attributes}>`,
    ...node.children.map(child => nodeToXml(child, depth + 1)!),
    `${indent}</${node.tag}>`,
  ].join('\n');
}

function highlightedXml(xml: string | undefined): string {
  if (xml === undefined || xml.length === 0) {
    return '<span class="xml-muted">No node output.</span>';
  }

  const highlighted = escapeHtml(xml).replace(
    /(&lt;\/?)([A-Za-z0-9_.:-]+)((?:\s+[A-Za-z0-9_.:-]+=&quot;[^&]*(?:&(?!quot;)[^&]*)*&quot;)*)?(\s*\/?&gt;)/g,
    (_match, open: string, tag: string, attributes: string | undefined, close: string) => {
      const highlightedAttributes = String(attributes ?? '').replace(
        /(\s+)([A-Za-z0-9_.:-]+)=(&quot;)(.*?)(&quot;)/g,
        (_attributeMatch, space: string, name: string, openingQuote: string, value: string, closingQuote: string) =>
          `${space}<span class="xml-attribute">${name}</span>=<span class="xml-value">${openingQuote}${value}${closingQuote}</span>`,
      );
      return `<span class="xml-punctuation">${open}</span><span class="xml-tag">${tag}</span>${highlightedAttributes}<span class="xml-punctuation">${close}</span>`;
    },
  );

  return highlighted;
}

function caseChanged(testCase: HtmlReportCase): boolean {
  return testCase.diffPercent > 0 || testCase.observationsChanged || testCase.status !== 'compared';
}

function statusKind(testCase: HtmlReportCase): string {
  if (testCase.status === 'missing' || testCase.status === 'missing-snapshot') {
    return 'bad';
  }
  return caseChanged(testCase) ? 'changed' : 'ok';
}

function sideStatus(testCase: HtmlReportCase, side: 'before' | 'after'): string {
  if (testCase.status === 'missing' && testCase.missingFrom === side) {
    return 'missing';
  }
  if (testCase.status === 'missing-snapshot' && (side === 'before' ? testCase.beforeImage === undefined : testCase.afterImage === undefined)) {
    return 'missing snapshot';
  }
  return (side === 'before' ? testCase.beforeStatus : testCase.afterStatus) ?? 'unknown';
}

function badge(label: string, kind = ''): string {
  return `<span class="badge ${escapeHtml(kind)}">${escapeHtml(label)}</span>`;
}

function imagePanel(title: string, src: string | undefined): string {
  const body =
    src === undefined
      ? '<div class="missing-image">No snapshot</div>'
      : `<a href="${escapeHtml(src)}"><img src="${escapeHtml(src)}" alt="${escapeHtml(title)}" loading="lazy"></a>`;
  return `<div class="image-panel"><h4>${escapeHtml(title)}</h4>${body}</div>`;
}

function observationText(value: string | undefined): string {
  if (value === undefined || value.length === 0) {
    return '<span class="muted">No observations.</span>';
  }
  return `<pre>${escapeHtml(value)}</pre>`;
}

function xmlPanel(testCase: HtmlReportCase): string {
  if (testCase.beforeNodeXml === testCase.afterNodeXml) {
    return `<div class="xml-panel" hidden>
          <div class="xml-grid single">
            <section>
              <h4>XML</h4>
              <pre class="xml-code">${highlightedXml(testCase.beforeNodeXml ?? testCase.afterNodeXml)}</pre>
            </section>
          </div>
        </div>`;
  }

  return `<div class="xml-panel" hidden>
          <div class="xml-grid">
            <section>
              <h4>Before XML</h4>
              <pre class="xml-code">${highlightedXml(testCase.beforeNodeXml)}</pre>
            </section>
            <section>
              <h4>After XML</h4>
              <pre class="xml-code">${highlightedXml(testCase.afterNodeXml)}</pre>
            </section>
          </div>
        </div>`;
}

function renderHtmlCase(testCase: HtmlReportCase): string {
  const status = testCase.status === 'compared' ? (caseChanged(testCase) ? 'changed' : 'same') : testCase.status;
  const details = [
    badge(status, statusKind(testCase)),
    badge(testCase.element ?? 'unknown element'),
    badge(`diff ${percent(testCase.diffPercent)}`, testCase.diffPercent > 0 ? 'changed' : 'ok'),
    testCase.observationsChanged ? badge('observations changed', 'changed') : badge('observations same', 'ok'),
    testCase.dimensionMismatch ? badge('dimension mismatch', 'changed') : '',
  ].filter(value => value.length > 0);

  return `
      <article class="case">
        <header class="case-header">
          <div>
            <div class="case-id">${escapeHtml(testCase.id)}</div>
            <h3>${escapeHtml(testCase.name ?? testCase.id)}</h3>
          </div>
          <div class="case-diff">${percent(testCase.diffPercent)}</div>
        </header>
        <p class="description">${escapeHtml(testCase.description ?? '')}</p>
        <div class="badges">${details.join('')}</div>
        <button class="xml-toggle" type="button">View XML</button>
        ${xmlPanel(testCase)}
        <div class="images">
          ${imagePanel(`Before (${sideStatus(testCase, 'before')})`, testCase.beforeImage)}
          ${imagePanel(`After (${sideStatus(testCase, 'after')})`, testCase.afterImage)}
          ${imagePanel('Diff', testCase.diffImage)}
        </div>
        <div class="observations">
          <section>
            <h4>Before Observations</h4>
            ${observationText(testCase.beforeObservations)}
            ${testCase.beforeError ? `<pre class="error">${escapeHtml(testCase.beforeError)}</pre>` : ''}
          </section>
          <section>
            <h4>After Observations</h4>
            ${observationText(testCase.afterObservations)}
            ${testCase.afterError ? `<pre class="error">${escapeHtml(testCase.afterError)}</pre>` : ''}
          </section>
        </div>
      </article>`;
}

function makeReport(summary: ComparisonSummary): HtmlReportData {
  const cases = summary.cases.map(testCase => ({
    ...testCase,
    beforeImage: relativeImagePath('before', testCase.beforeImagePath),
    afterImage: relativeImagePath('after', testCase.afterImagePath),
    diffImage: relativeImagePath('diffs', testCase.diffImagePath),
    beforeNodeXml: nodeToXml(testCase.beforeNodeOutput),
    afterNodeXml: nodeToXml(testCase.afterNodeOutput),
  }));
  const elements = [...new Set(cases.map(testCase => testCase.element ?? 'unknown'))].sort();
  return {
    before: summary.before,
    after: summary.after,
    beforePlatform: summary.beforePlatform,
    afterPlatform: summary.afterPlatform,
    pixelThreshold: summary.pixelThreshold,
    caseCount: summary.caseCount,
    changedCaseCount: summary.changedCaseCount,
    maxDiffPercent: summary.maxDiffPercent,
    generatedAt: new Date().toISOString(),
    elements,
    cases,
  };
}

function renderScript(report: HtmlReportData): string {
  return `
    const report = ${scriptJson(report)};
    const els = {
      search: document.getElementById('search'),
      elementFilter: document.getElementById('elementFilter'),
      statusFilter: document.getElementById('statusFilter'),
      renderMode: document.getElementById('renderMode'),
      sort: document.getElementById('sort'),
      resultCount: document.getElementById('resultCount'),
      cases: document.getElementById('cases'),
      caseCount: document.getElementById('caseCount'),
      changedCaseCount: document.getElementById('changedCaseCount'),
      maxDiff: document.getElementById('maxDiff'),
      platforms: document.getElementById('platforms'),
      paths: document.getElementById('paths')
    };

    function escapeHtml(value) {
      return String(value ?? '').replace(/[&<>"']/g, character => {
        switch (character) {
          case '&':
            return '&amp;';
          case '<':
            return '&lt;';
          case '>':
            return '&gt;';
          case '"':
            return '&quot;';
          case "'":
            return '&#39;';
          default:
            return character;
        }
      });
    }

    function percent(value) {
      return Number(value ?? 0).toFixed(4) + '%';
    }

    function changed(item) {
      return item.diffPercent > 0 || item.observationsChanged || item.status !== 'compared';
    }

    function textBlob(item) {
      return [
        item.id,
        item.name,
        item.element,
        item.description,
        item.beforeObservations,
        item.afterObservations,
        item.beforeError,
        item.afterError,
        item.beforeNodeXml,
        item.afterNodeXml
      ].join(' ').toLowerCase();
    }

    function statusMatches(item, status) {
      if (status === 'all') {
        return true;
      }
      if (status === 'changed') {
        return changed(item);
      }
      if (status === 'image-changed') {
        return item.diffPercent > 0;
      }
      if (status === 'observations-changed') {
        return item.observationsChanged;
      }
      if (status === 'missing') {
        return item.status === 'missing';
      }
      if (status === 'missing-snapshot') {
        return item.status === 'missing-snapshot';
      }
      if (status === 'dimension-mismatch') {
        return item.dimensionMismatch === true;
      }
      return true;
    }

    function sorted(items) {
      const out = [...items];
      if (els.sort.value === 'diff-desc') {
        out.sort((a, b) => b.diffPercent - a.diffPercent || a.id.localeCompare(b.id));
      } else if (els.sort.value === 'diff-asc') {
        out.sort((a, b) => a.diffPercent - b.diffPercent || a.id.localeCompare(b.id));
      } else if (els.sort.value === 'element') {
        out.sort((a, b) => String(a.element ?? '').localeCompare(String(b.element ?? '')) || a.id.localeCompare(b.id));
      } else {
        out.sort((a, b) => a.id.localeCompare(b.id));
      }
      return out;
    }

    function statusKind(item) {
      if (item.status === 'missing' || item.status === 'missing-snapshot') {
        return 'bad';
      }
      return changed(item) ? 'changed' : 'ok';
    }

    function badge(label, kind = '') {
      return '<span class="badge ' + escapeHtml(kind) + '">' + escapeHtml(label) + '</span>';
    }

    function imagePanel(title, src) {
      const body = src === undefined
        ? '<div class="missing-image">No snapshot</div>'
        : '<a href="' + escapeHtml(src) + '"><img src="' + escapeHtml(src) + '" alt="' + escapeHtml(title) + '" loading="lazy"></a>';
      return '<div class="image-panel"><h4>' + escapeHtml(title) + '</h4>' + body + '</div>';
    }

    function sideStatus(item, side) {
      if (item.status === 'missing' && item.missingFrom === side) {
        return 'missing';
      }
      if (item.status === 'missing-snapshot' && (side === 'before' ? item.beforeImage === undefined : item.afterImage === undefined)) {
        return 'missing snapshot';
      }
      return (side === 'before' ? item.beforeStatus : item.afterStatus) ?? 'unknown';
    }

    function observationText(value) {
      if (value === undefined || value.length === 0) {
        return '<span class="muted">No observations.</span>';
      }
      return '<pre>' + escapeHtml(value) + '</pre>';
    }

    function highlightedXml(xml) {
      if (xml === undefined || xml.length === 0) {
        return '<span class="xml-muted">No node output.</span>';
      }

      const escaped = escapeHtml(xml);
      return escaped.replace(
        /(&lt;\\/?)([A-Za-z0-9_.:-]+)((?:\\s+[A-Za-z0-9_.:-]+=&quot;[^&]*(?:&(?!quot;)[^&]*)*&quot;)*)?(\\s*\\/?&gt;)/g,
        function(_match, open, tag, attributes, close) {
          const highlightedAttributes = String(attributes ?? '').replace(
            /(\\s+)([A-Za-z0-9_.:-]+)=(&quot;)(.*?)(&quot;)/g,
            function(_attributeMatch, space, name, openingQuote, value, closingQuote) {
              return space + '<span class="xml-attribute">' + name + '</span>=<span class="xml-value">' +
                openingQuote + value + closingQuote + '</span>';
            }
          );
          return '<span class="xml-punctuation">' + open + '</span><span class="xml-tag">' + tag +
            '</span>' + highlightedAttributes + '<span class="xml-punctuation">' + close + '</span>';
        }
      );
    }

    function xmlPanel(item) {
      if (item.beforeNodeXml === item.afterNodeXml) {
        return '<div class="xml-panel" hidden><div class="xml-grid single">' +
          '<section><h4>XML</h4><pre class="xml-code">' + highlightedXml(item.beforeNodeXml ?? item.afterNodeXml) + '</pre></section>' +
          '</div></div>';
      }

      return '<div class="xml-panel" hidden><div class="xml-grid">' +
        '<section><h4>Before XML</h4><pre class="xml-code">' + highlightedXml(item.beforeNodeXml) + '</pre></section>' +
        '<section><h4>After XML</h4><pre class="xml-code">' + highlightedXml(item.afterNodeXml) + '</pre></section>' +
        '</div></div>';
    }

    function caseBadges(item) {
      const status = item.status === 'compared' ? (changed(item) ? 'changed' : 'same') : item.status;
      return [
        badge(status, statusKind(item)),
        badge(item.element ?? 'unknown element'),
        badge('diff ' + percent(item.diffPercent), item.diffPercent > 0 ? 'changed' : 'ok'),
        item.observationsChanged ? badge('observations changed', 'changed') : badge('observations same', 'ok'),
        item.dimensionMismatch ? badge('dimension mismatch', 'changed') : ''
      ].filter(value => value.length > 0).join('');
    }

    function renderCompactCase(item) {
      return '<article class="case compact">' +
        '<header class="case-header"><div><div class="case-id">' + escapeHtml(item.id) + '</div><h3>' +
        escapeHtml(item.name ?? item.id) + '</h3></div><div class="case-diff">' + percent(item.diffPercent) + '</div></header>' +
        '<div class="badges">' + caseBadges(item) + '</div></article>';
    }

    function renderExpandedCase(item) {
      return '<article class="case expanded">' +
        '<header class="case-header"><div><div class="case-id">' + escapeHtml(item.id) + '</div><h3>' +
        escapeHtml(item.name ?? item.id) + '</h3></div><div class="case-diff">' + percent(item.diffPercent) + '</div></header>' +
        '<p class="description">' + escapeHtml(item.description ?? '') + '</p>' +
        '<div class="badges">' + caseBadges(item) + '</div>' +
        '<button class="xml-toggle" type="button">View XML</button>' +
        xmlPanel(item) +
        '<div class="images">' +
        imagePanel('Before (' + sideStatus(item, 'before') + ')', item.beforeImage) +
        imagePanel('After (' + sideStatus(item, 'after') + ')', item.afterImage) +
        imagePanel('Diff', item.diffImage) +
        '</div>' +
        '<div class="observations"><section><h4>Before Observations</h4>' + observationText(item.beforeObservations) +
        (item.beforeError ? '<pre class="error">' + escapeHtml(item.beforeError) + '</pre>' : '') +
        '</section><section><h4>After Observations</h4>' + observationText(item.afterObservations) +
        (item.afterError ? '<pre class="error">' + escapeHtml(item.afterError) + '</pre>' : '') +
        '</section></div></article>';
    }

    function renderCase(item) {
      return els.renderMode.value === 'compact' ? renderCompactCase(item) : renderExpandedCase(item);
    }

    function render() {
      const query = els.search.value.trim().toLowerCase();
      const element = els.elementFilter.value;
      const status = els.statusFilter.value;
      const items = sorted(report.cases.filter(item => {
        return (query.length === 0 || textBlob(item).includes(query)) &&
          (element === 'all' || (item.element ?? 'unknown') === element) &&
          statusMatches(item, status);
      }));
      els.resultCount.textContent = items.length + ' of ' + report.caseCount + ' cases shown';
      els.cases.innerHTML = items.map(renderCase).join('');
    }

    function init() {
      els.caseCount.textContent = report.caseCount;
      els.changedCaseCount.textContent = report.changedCaseCount;
      els.maxDiff.textContent = percent(report.maxDiffPercent);
      els.platforms.textContent = report.beforePlatform + ' -> ' + report.afterPlatform;
      els.paths.textContent = 'Before: ' + report.before + ' | After: ' + report.after + ' | Generated: ' + report.generatedAt;
      els.elementFilter.innerHTML = '<option value="all">All elements</option>' +
        report.elements.map(element => '<option value="' + escapeHtml(element) + '">' + escapeHtml(element) + '</option>').join('');
      for (const input of [els.search, els.elementFilter, els.statusFilter, els.renderMode, els.sort]) {
        input.addEventListener('input', render);
        input.addEventListener('change', render);
      }
      els.cases.addEventListener('click', event => {
        const button = event.target instanceof Element ? event.target.closest('.xml-toggle') : undefined;
        if (!button) {
          return;
        }
        const caseElement = button.closest('.case');
        const panel = caseElement ? caseElement.querySelector('.xml-panel') : undefined;
        if (!panel) {
          return;
        }
        const shouldOpen = panel.hasAttribute('hidden');
        if (shouldOpen) {
          panel.removeAttribute('hidden');
          button.textContent = 'Hide XML';
        } else {
          panel.setAttribute('hidden', '');
          button.textContent = 'View XML';
        }
      });
      render();
    }

    init();
  `;
}

export function writeHtmlReport(summary: ComparisonSummary, outputDir: Path): void {
  const report = makeReport(summary);
  const staticCasesHtml = report.cases.map(renderHtmlCase).join('\n');
  const elementOptions = [
    '<option value="all">All elements</option>',
    ...report.elements.map(element => `<option value="${escapeHtml(element)}">${escapeHtml(element)}</option>`),
  ].join('');
  const html = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Valdi Integration Diff</title>
  <style>
    :root {
      color-scheme: light;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #f8fafc;
      color: #111827;
    }

    body {
      margin: 0;
      background: #f8fafc;
    }

    main {
      max-width: 1440px;
      margin: 0 auto;
      padding: 24px;
    }

    header.page-header {
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 24px;
      margin-bottom: 18px;
    }

    h1 {
      margin: 0 0 8px;
      font-size: 28px;
      line-height: 1.15;
    }

    .paths {
      color: #64748b;
      font-size: 13px;
      line-height: 1.5;
      overflow-wrap: anywhere;
    }

    .metrics {
      display: grid;
      grid-template-columns: repeat(4, minmax(120px, 1fr));
      gap: 10px;
      margin-bottom: 18px;
    }

    .metric {
      background: #ffffff;
      border: 1px solid #e5e7eb;
      border-radius: 8px;
      padding: 12px;
    }

    .metric span {
      display: block;
      color: #64748b;
      font-size: 12px;
      margin-bottom: 6px;
    }

    .metric strong {
      display: block;
      font-size: 20px;
    }

    .controls {
      display: grid;
      grid-template-columns: minmax(220px, 1fr) repeat(4, minmax(150px, 210px));
      gap: 10px;
      align-items: center;
      margin-bottom: 10px;
    }

    input,
    select {
      width: 100%;
      box-sizing: border-box;
      border: 1px solid #cbd5e1;
      border-radius: 8px;
      padding: 10px 12px;
      background: #ffffff;
      color: #111827;
      font: inherit;
    }

    .result-count {
      color: #64748b;
      font-size: 13px;
      margin-bottom: 18px;
    }

    .case {
      background: #ffffff;
      border: 1px solid #e5e7eb;
      border-radius: 8px;
      padding: 18px;
      margin-bottom: 16px;
    }

    .case.compact {
      padding: 12px 14px;
      margin-bottom: 8px;
    }

    .case.compact .case-header {
      margin-bottom: 10px;
    }

    .case.compact h3 {
      font-size: 16px;
    }

    .case.compact .case-diff {
      font-size: 15px;
    }

    .case.compact .badges {
      margin-bottom: 0;
    }

    .case-header {
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 18px;
      margin-bottom: 8px;
    }

    .case-id {
      color: #64748b;
      font-size: 12px;
      margin-bottom: 4px;
    }

    h3 {
      margin: 0;
      font-size: 20px;
    }

    .case-diff {
      color: #b91c1c;
      font-size: 18px;
      font-weight: 700;
      white-space: nowrap;
    }

    .description {
      margin: 0 0 12px;
      color: #334155;
      line-height: 1.45;
    }

    .badges {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin-bottom: 14px;
    }

    .xml-toggle {
      appearance: none;
      border: 1px solid #cbd5e1;
      border-radius: 8px;
      background: #ffffff;
      color: #0f172a;
      font: 13px/1.2 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      font-weight: 600;
      padding: 8px 10px;
      margin: 0 0 14px;
      cursor: pointer;
    }

    .xml-toggle:hover {
      background: #f8fafc;
    }

    .xml-panel {
      margin: 0 0 14px;
    }

    .xml-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }

    .xml-grid.single {
      grid-template-columns: 1fr;
    }

    .xml-grid section {
      min-width: 0;
      border: 1px solid #2d2d2d;
      border-radius: 8px;
      overflow: hidden;
      background: #1e1e1e;
    }

    .xml-grid h4 {
      margin: 0;
      padding: 9px 12px;
      background: #252526;
      border-bottom: 1px solid #333333;
      color: #cccccc;
      font-size: 13px;
    }

    .xml-code {
      margin: 0;
      padding: 12px;
      min-height: 44px;
      max-height: 360px;
      overflow: auto;
      white-space: pre;
      overflow-wrap: normal;
      background: #1e1e1e;
      color: #d4d4d4;
      font: 13px/1.5 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    }

    .xml-punctuation {
      color: #808080;
    }

    .xml-tag {
      color: #569cd6;
    }

    .xml-attribute {
      color: #9cdcfe;
    }

    .xml-value {
      color: #ce9178;
    }

    .xml-muted {
      color: #808080;
    }

    .badge {
      display: inline-flex;
      align-items: center;
      border-radius: 999px;
      background: #f1f5f9;
      color: #334155;
      padding: 4px 8px;
      font-size: 12px;
      font-weight: 600;
    }

    .badge.ok {
      background: #dcfce7;
      color: #166534;
    }

    .badge.changed {
      background: #fef3c7;
      color: #92400e;
    }

    .badge.bad {
      background: #fee2e2;
      color: #991b1b;
    }

    .images {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
      margin-bottom: 14px;
    }

    .image-panel {
      border: 1px solid #e5e7eb;
      border-radius: 8px;
      overflow: hidden;
      background: #f8fafc;
    }

    .image-panel h4 {
      margin: 0;
      padding: 10px 12px;
      border-bottom: 1px solid #e5e7eb;
      background: #ffffff;
      font-size: 13px;
    }

    .image-panel a {
      display: block;
    }

    .image-panel img {
      display: block;
      width: 100%;
      height: auto;
      background: #ffffff;
    }

    .missing-image {
      min-height: 160px;
      display: grid;
      place-items: center;
      color: #64748b;
      font-size: 13px;
    }

    .observations {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }

    .observations section {
      min-width: 0;
      border: 1px solid #e5e7eb;
      border-radius: 8px;
      padding: 12px;
      background: #f8fafc;
    }

    .observations h4 {
      margin: 0 0 8px;
      font-size: 13px;
    }

    pre {
      margin: 0;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      font: 12px/1.45 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    }

    .error {
      color: #991b1b;
      margin-top: 8px;
    }

    .muted {
      color: #64748b;
      font-size: 13px;
    }

    @media (max-width: 960px) {
      main {
        padding: 16px;
      }

      header.page-header,
      .case-header {
        display: block;
      }

      .metrics,
      .controls,
      .images,
      .xml-grid,
      .observations {
        grid-template-columns: 1fr;
      }

      .case-diff {
        margin-top: 8px;
      }
    }
  </style>
</head>
<body>
  <main>
    <header class="page-header">
      <div>
        <h1>Valdi Integration Diff</h1>
        <div class="paths" id="paths">${escapeHtml(`Before: ${report.before} | After: ${report.after} | Generated: ${report.generatedAt}`)}</div>
      </div>
    </header>

    <section class="metrics" aria-label="Summary">
      <div class="metric"><span>Cases</span><strong id="caseCount">${escapeHtml(report.caseCount)}</strong></div>
      <div class="metric"><span>Changed</span><strong id="changedCaseCount">${escapeHtml(report.changedCaseCount)}</strong></div>
      <div class="metric"><span>Max Diff</span><strong id="maxDiff">${escapeHtml(percent(report.maxDiffPercent))}</strong></div>
      <div class="metric"><span>Platforms</span><strong id="platforms">${escapeHtml(`${report.beforePlatform} -> ${report.afterPlatform}`)}</strong></div>
    </section>

    <section class="controls" aria-label="Filters">
      <input id="search" type="search" placeholder="Search cases, descriptions, observations">
      <select id="elementFilter">${elementOptions}</select>
      <select id="statusFilter">
        <option value="all">All statuses</option>
        <option value="changed">Changed</option>
        <option value="image-changed">Image changed</option>
        <option value="observations-changed">Observations changed</option>
        <option value="missing">Missing case</option>
        <option value="missing-snapshot">Missing snapshot</option>
        <option value="dimension-mismatch">Dimension mismatch</option>
      </select>
      <select id="renderMode">
        <option value="expanded">Expanded view</option>
        <option value="compact">Compact view</option>
      </select>
      <select id="sort">
        <option value="diff-desc">Largest diff first</option>
        <option value="diff-asc">Smallest diff first</option>
        <option value="id">Case id</option>
        <option value="element">Element</option>
      </select>
    </section>
    <div class="result-count" id="resultCount">${escapeHtml(`${report.cases.length} of ${report.caseCount} cases shown`)}</div>

    <section id="cases">${staticCasesHtml}
    </section>
  </main>
  <script>
${renderScript(report)}
  </script>
</body>
</html>
`;
  fs.writeFileSync(outputDir.appending('index.html').toString(), html);
}
