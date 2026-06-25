#!/usr/bin/env node

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

const root = path.resolve(__dirname, '..');
const testFile = path.join(root, 'tools', 'test.sh');
const outDir = path.join(root, 'comparison_all');
const assetDir = path.join(outDir, 'assets');
const cli = process.env.CLI || path.join(root, 'build', 'mathjax-cli');
const font = process.env.FONT || path.join(root, 'fonts', 'STIXTwoMath-Regular.ttf');
const concurrency = Math.max(1, Math.min(Number(process.env.JOBS || os.cpus().length || 4), 12));

function htmlEscape(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function parseCases() {
  const lines = fs.readFileSync(testFile, 'utf8').split(/\r?\n/);
  const cases = [];
  let section = 'Uncategorized';

  for (const line of lines) {
    const sectionMatch = line.match(/^echo "===\s*(.*?)\s*==="/);
    if (sectionMatch) {
      section = sectionMatch[1];
      continue;
    }

    const testMatch = line.match(/^run_test\s+"([^"]+)"\s*'([^']*)'\s+([0-9]+)/);
    if (!testMatch) continue;
    const index = cases.length + 1;
    const name = testMatch[1];
    const formula = testMatch[2];
    const minSize = Number(testMatch[3]);
    cases.push({
      index,
      id: String(index).padStart(4, '0'),
      section,
      name,
      formula,
      minSize,
      placeholder: false,
      lenient: minSize === 1,
      image: `assets/${String(index).padStart(4, '0')}.png`,
    });
  }
  return cases;
}

function renderCase(testCase) {
  return new Promise((resolve) => {
    const imagePath = path.join(assetDir, `${testCase.id}.png`);
    const child = spawn(cli, ['-f', font, '-o', imagePath, testCase.formula], {
      cwd: root,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => { stdout += chunk; });
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.on('close', (code) => {
      const sizeMatch = stdout.match(/Wrote\s+([0-9]+)x([0-9]+)\s+PNG/);
      resolve({
        ...testCase,
        ok: code === 0 && fs.existsSync(imagePath),
        width: sizeMatch ? Number(sizeMatch[1]) : null,
        height: sizeMatch ? Number(sizeMatch[2]) : null,
        stdout: stdout.trim(),
        stderr: stderr.trim(),
      });
    });
  });
}

async function renderAll(cases) {
  const results = new Array(cases.length);
  let next = 0;
  let done = 0;

  async function worker() {
    while (next < cases.length) {
      const current = next++;
      results[current] = await renderCase(cases[current]);
      done++;
      if (done % 50 === 0 || done === cases.length) {
        process.stdout.write(`Rendered ${done}/${cases.length}\n`);
      }
    }
  }

  await Promise.all(Array.from({ length: concurrency }, worker));
  return results;
}

function writeHtml(results) {
  const sections = [...new Set(results.map((item) => item.section))];
  const failures = results.filter((item) => !item.ok);
  const placeholders = results.filter((item) => item.placeholder).length;
  const lenient = results.filter((item) => item.lenient).length;
  const generatedAt = new Date().toISOString();

  const cards = results.map((item) => `
      <article class="case${item.placeholder ? ' placeholder' : ''}${item.ok ? '' : ' failed'}" data-section="${htmlEscape(item.section)}" data-placeholder="${item.placeholder ? '1' : '0'}">
        <header>
          <span class="idx">#${item.id}</span>
          <h2>${htmlEscape(item.name)}</h2>
          <span class="tag">${htmlEscape(item.section)}</span>
          ${item.placeholder ? '<span class="tag warn">placeholder</span>' : ''}
          ${item.lenient ? '<span class="tag">lenient</span>' : ''}
          ${item.ok ? '' : '<span class="tag bad">render failed</span>'}
        </header>
        <p class="source"><code>${htmlEscape(item.formula || '(empty)')}</code></p>
        <div class="compare">
          <section>
            <h3>mathjax-c ${item.width && item.height ? `<small>${item.width}x${item.height}</small>` : ''}</h3>
            <div class="render c-render">${item.ok ? `<img loading="lazy" src="${item.image}" alt="${htmlEscape(item.name)} by mathjax-c">` : `<pre>${htmlEscape(item.stderr || item.stdout || 'failed')}</pre>`}</div>
          </section>
          <section>
            <h3>MathJax reference</h3>
            <div class="render mjx-render">$$${htmlEscape(item.formula)}$$</div>
          </section>
        </div>
      </article>`).join('\n');

  const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>mathjax-c 全量测试公式对照</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f4f6f9;
      --panel: #ffffff;
      --ink: #111827;
      --muted: #5f6b7a;
      --line: #d8dee8;
      --soft: #eef2f7;
      --accent: #185abc;
      --warn: #8a5a00;
      --bad: #b42318;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--ink);
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    .top {
      position: sticky;
      top: 0;
      z-index: 10;
      border-bottom: 1px solid var(--line);
      background: rgba(244, 246, 249, 0.96);
      backdrop-filter: blur(10px);
    }
    .top-inner, main {
      width: min(1440px, calc(100vw - 28px));
      margin: 0 auto;
    }
    .top-inner { padding: 18px 0 14px; }
    h1 { margin: 0 0 10px; font-size: 24px; line-height: 1.2; }
    .summary {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      color: var(--muted);
      font-size: 13px;
    }
    .pill {
      border: 1px solid var(--line);
      border-radius: 999px;
      padding: 6px 10px;
      background: var(--panel);
    }
    .controls {
      display: grid;
      grid-template-columns: minmax(220px, 1fr) minmax(220px, 320px) auto;
      gap: 10px;
      margin-top: 14px;
      align-items: center;
    }
    input, select, button {
      height: 36px;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #fff;
      color: var(--ink);
      font: inherit;
      padding: 0 10px;
    }
    button { cursor: pointer; color: var(--accent); }
    main { padding: 18px 0 44px; }
    .case {
      margin-bottom: 14px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      overflow: hidden;
    }
    .case.placeholder { border-color: #e3d3a2; }
    .case.failed { border-color: #f0aaa3; }
    .case header {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
      padding: 12px 14px;
      border-bottom: 1px solid var(--line);
      background: #fbfcfe;
    }
    .idx, .tag {
      flex: none;
      border-radius: 999px;
      padding: 4px 8px;
      background: var(--soft);
      color: var(--muted);
      font-size: 12px;
    }
    .tag.warn { color: var(--warn); background: #fff6d8; }
    .tag.bad { color: var(--bad); background: #fee4e2; }
    h2 { margin: 0; font-size: 15px; line-height: 1.35; }
    .source {
      margin: 0;
      padding: 10px 14px 0;
      color: var(--muted);
      font-size: 13px;
      word-break: break-word;
    }
    code { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; }
    .compare {
      display: grid;
      grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
      gap: 12px;
      padding: 12px 14px 14px;
    }
    h3 {
      margin: 0 0 8px;
      color: var(--accent);
      font-size: 12px;
      letter-spacing: .06em;
      text-transform: uppercase;
    }
    h3 small {
      color: var(--muted);
      font-weight: 500;
      letter-spacing: 0;
      text-transform: none;
    }
    .render {
      min-height: 92px;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: auto;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #fff;
      padding: 16px;
    }
    .render img { max-width: 100%; height: auto; }
    .mjx-render { font-size: 24px; color: #111; }
    .hidden { display: none; }
    @media (max-width: 900px) {
      .controls { grid-template-columns: 1fr; }
      .compare { grid-template-columns: 1fr; }
    }
  </style>
  <script>
    window.MathJax = {
      tex: {
        inlineMath: [['$', '$'], ['\\\\(', '\\\\)']],
        displayMath: [['$$', '$$'], ['\\\\[', '\\\\]']],
        packages: {'[+]': ['ams', 'color', 'cancel', 'bbox', 'textmacros']}
      },
      loader: {load: ['[tex]/ams', '[tex]/color', '[tex]/cancel', '[tex]/bbox', '[tex]/textmacros']},
      svg: {fontCache: 'global'}
    };
  </script>
  <script defer src="https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-svg.js"></script>
</head>
<body>
  <div class="top">
    <div class="top-inner">
      <h1>mathjax-c 全量测试公式对照</h1>
      <div class="summary">
        <span class="pill">生成时间：${generatedAt}</span>
        <span class="pill">公式：${results.length}</span>
        <span class="pill">placeholder：${placeholders}</span>
        <span class="pill">lenient：${lenient}</span>
        <span class="pill">渲染失败：${failures.length}</span>
      </div>
      <div class="controls">
        <input id="q" type="search" placeholder="搜索 name / LaTeX">
        <select id="section">
          <option value="">全部分类</option>
          ${sections.map((section) => `<option value="${htmlEscape(section)}">${htmlEscape(section)}</option>`).join('\n')}
        </select>
        <button id="togglePlaceholders" type="button">隐藏 placeholder</button>
      </div>
    </div>
  </div>
  <main id="cases">
${cards}
  </main>
  <script>
    const q = document.getElementById('q');
    const section = document.getElementById('section');
    const toggle = document.getElementById('togglePlaceholders');
    const cases = [...document.querySelectorAll('.case')];
    let showPlaceholders = true;

    function applyFilter() {
      const needle = q.value.trim().toLowerCase();
      const selected = section.value;
      for (const el of cases) {
        const text = el.textContent.toLowerCase();
        const sectionOk = !selected || el.dataset.section === selected;
        const textOk = !needle || text.includes(needle);
        const placeholderOk = showPlaceholders || el.dataset.placeholder !== '1';
        el.classList.toggle('hidden', !(sectionOk && textOk && placeholderOk));
      }
    }
    q.addEventListener('input', applyFilter);
    section.addEventListener('change', applyFilter);
    toggle.addEventListener('click', () => {
      showPlaceholders = !showPlaceholders;
      toggle.textContent = showPlaceholders ? '隐藏 placeholder' : '显示 placeholder';
      applyFilter();
    });
  </script>
</body>
</html>`;

  fs.writeFileSync(path.join(outDir, 'index.html'), html);
}

async function main() {
  if (!fs.existsSync(cli)) {
    throw new Error(`CLI not found: ${cli}`);
  }
  fs.rmSync(outDir, { recursive: true, force: true });
  fs.mkdirSync(assetDir, { recursive: true });

  const cases = parseCases();
  console.log(`Rendering ${cases.length} cases with ${concurrency} workers`);
  const results = await renderAll(cases);
  fs.writeFileSync(path.join(outDir, 'cases.json'), JSON.stringify(results, null, 2));
  writeHtml(results);

  const failed = results.filter((item) => !item.ok);
  console.log(`Done: ${results.length - failed.length}/${results.length} rendered`);
  console.log(`Output: ${path.join(outDir, 'index.html')}`);
  if (failed.length) process.exitCode = 1;
}

main().catch((error) => {
  console.error(error.stack || error.message);
  process.exit(1);
});
