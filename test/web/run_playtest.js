// Browser playthrough test for the WebAssembly build.
//
// Reads a shared script JSON (test/scripts/<name>.json) — the same file the
// native C test consumes via a generated header — drives the served web build
// with Puppeteer, saves a screenshot at each "screenshot" step, and asserts the
// expected dialogue shows up in the console in order.
//
// Usage: node run_playtest.js <script.json>
//   URL    web build to test          (default http://localhost:8099/)
//   SHOTS  screenshot output directory (default ./screenshots)
//   BRUSH_SAMPLES  horizontal samples per brush row (must match script.h)
//
// Exit code: 0 = all expected lines seen, 2 = a line is missing, 1 = error.

const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer');

const SCRIPT = process.argv[2];
const URL = process.env.URL || 'http://localhost:8099/';
const SHOTS = process.env.SHOTS || 'screenshots';
const BRUSH_SAMPLES = parseInt(process.env.BRUSH_SAMPLES || '16', 10);

if (!SCRIPT) {
  console.error('usage: node run_playtest.js <script.json>');
  process.exit(1);
}

const script = JSON.parse(fs.readFileSync(SCRIPT, 'utf8'));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let box; // canvas bounding rect in CSS px
async function refreshBox(page) {
  box = await page.$eval('#canvas', (el) => {
    const r = el.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
}
const px = (fx) => box.x + box.w * fx;
const py = (fy) => box.y + box.h * fy;

async function brush(page, s) {
  await page.mouse.move(px(s.x0), py(s.y0));
  await page.mouse.down();
  for (let row = 0; row < s.rows; row++) {
    const t = s.rows > 1 ? row / (s.rows - 1) : 0;
    const y = s.y0 + t * (s.y1 - s.y0);
    const from = row % 2 === 0 ? s.x0 : s.x1;
    const to = row % 2 === 0 ? s.x1 : s.x0;
    for (let k = 0; k <= BRUSH_SAMPLES; k++) {
      await page.mouse.move(px(from + ((to - from) * k) / BRUSH_SAMPLES), py(y));
    }
  }
  await page.mouse.up();
  await sleep(s.wait_ms || 0);
}

(async () => {
  fs.mkdirSync(SHOTS, { recursive: true });
  const logs = [];
  const browser = await puppeteer.launch({
    headless: 'new',
    args: ['--no-sandbox', '--disable-setuid-sandbox', '--use-gl=angle',
           '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
           '--ignore-gpu-blocklist', '--enable-webgl'],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 900, height: 720, deviceScaleFactor: 2 });
  page.on('console', (m) => { logs.push(m.text()); });
  page.on('pageerror', (e) => { logs.push('PAGEERR ' + e.message); });

  // Force the source locale (it_IT): the playthrough scripts assert the
  // Italian dialogue lines, which now resolve per-locale from their text
  // sidecars (SCENES.md milestone 4). Without this the headless browser's
  // navigator.language (en-US) would load the English build, whose dialogue
  // text isn't authored yet. (URL is a string constant above, so build the
  // query by hand rather than with the URL constructor.)
  const testUrl = URL + (URL.includes('?') ? '&' : '?') + 'lang=it';
  await page.goto(testUrl, { waitUntil: 'load', timeout: 60000 });
  await sleep(12000); // let the wasm boot and assets preload
  await refreshBox(page);
  const canvas = await page.$('#canvas');

  for (const s of script.steps) {
    switch (s.action) {
      case 'click':
        await page.mouse.click(px(s.x), py(s.y));
        await sleep(s.wait_ms || 0);
        break;
      case 'brush':
        await brush(page, s);
        break;
      case 'wait':
        await sleep(s.wait_ms || 0);
        break;
      case 'screenshot':
        await canvas.screenshot({ path: path.join(SHOTS, `${s.name}.png`) });
        console.log('shot ' + s.name);
        break;
      default:
        console.error('unknown action: ' + s.action);
    }
  }

  await browser.close();

  const all = logs.join('\n');
  let ok = true;
  let from = 0;
  for (const line of script.expect) {
    const at = all.indexOf(line, from);
    if (at < 0) {
      const anywhere = all.includes(line);
      console.log((anywhere ? 'MISS (out of order)  ' : 'MISS  ') + line);
      ok = false;
    } else {
      console.log('OK    ' + line);
      from = at + line.length;
    }
  }
  const errs = logs.filter((l) => l.startsWith('PAGEERR') || /Failed to load resource/.test(l));
  console.log('pageerrors/failed-loads:', errs.length);
  console.log(ok ? 'PASS: all lines present' : 'FAIL: a line is missing');
  process.exit(ok ? 0 : 2);
})().catch((e) => { console.error('ERR', e.message); process.exit(1); });
