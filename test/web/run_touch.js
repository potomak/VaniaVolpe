// Browser touch test for the WebAssembly build.
//
// The playthrough test (run_playtest.js) drives the game with the mouse at one
// desktop-shaped viewport. That leaves the thing most players actually do —
// tapping, on a phone — untested, and the native harness cannot cover it: its
// window is exactly the logical size at a device pixel ratio of 1, so every
// coordinate mapping, right or wrong, agrees there. This test taps real touch
// events at phone-shaped, high-DPR, letterboxed viewports, where a wrong
// mapping misses.
//
// Each tap is checked by what the game logs: the selection screen announces
// which adventure it opened, so a tap that lands is distinguishable from one
// that does not, and from one that landed on the wrong cartridge. The
// deliberate miss (an empty grid slot) is what stops a mapping that hits
// everything from passing.
//
// Usage: node run_touch.js
//   URL    web build to test  (default http://localhost:8099/)
//   SHOTS  screenshot output directory (default ./screenshots)
//
// Exit code: 0 = every viewport behaved, 2 = a tap missed, 1 = error.

const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer');

const URL = process.env.URL || 'http://localhost:8099/';
const SHOTS = process.env.SHOTS || 'screenshots';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Positions as fractions of the logical 800x600 stage, matching the native
// input test (test/test_input.c). Cartridges: src/hub.c, 220x200 in a 3-column
// grid, 35px gaps, top row at 110. The empty slot is the second grid row,
// which has no adventure in it. The corner button and the confirmation tick
// are the way back out of an adventure (src/game.c, src/confirm.c).
const VANIA = { x: 0.18125, y: 0.35 };
const GINA = { x: 0.5, y: 0.35 };
const EMPTY_SLOT = { x: 0.18125, y: 0.75 };
const HUB_BUTTON = { x: 0.945, y: 0.0733 };
const CONFIRM_YES = { x: 0.3575, y: 0.5 };

const OPENED_VANIA = "Hub: starting adventure 'Vania Volpe - Lo Scivolo'";
const OPENED_GINA = "Hub: starting adventure 'Gina la Gallina in Piscina'";

// Portrait and landscape phones letterbox on opposite axes and both run at a
// pixel ratio of 3; the desktop shape is the control, and the one the mouse
// playthrough already covers.
const VIEWPORTS = [
  {
    name: 'phone-portrait',
    viewport: { width: 390, height: 844, deviceScaleFactor: 3, isMobile: true, hasTouch: true },
  },
  {
    name: 'phone-landscape',
    viewport: { width: 844, height: 390, deviceScaleFactor: 3, isMobile: true, hasTouch: true },
  },
  {
    name: 'desktop',
    viewport: { width: 900, height: 720, deviceScaleFactor: 2, hasTouch: true },
  },
];

async function run(browser, spec) {
  const page = await browser.newPage();
  await page.setViewport(spec.viewport);
  const logs = [];
  page.on('console', (m) => logs.push(m.text()));
  page.on('pageerror', (e) => logs.push('PAGEERR ' + e.message));

  // Same locale pin as the playthrough: the headless browser reports en-US,
  // whose assets are placeholders.
  const testUrl = URL + (URL.includes('?') ? '&' : '?') + 'lang=it';
  await page.goto(testUrl, { waitUntil: 'load', timeout: 60000 });
  await sleep(12000); // let the wasm boot and the assets preload

  // The canvas is letterboxed inside the viewport, so stage fractions have to
  // go through its on-screen rect — that rect is the mapping under test.
  const box = await page.$eval('#canvas', (el) => {
    const r = el.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
  const tap = async (p, waitMs) => {
    await page.touchscreen.tap(box.x + box.w * p.x, box.y + box.h * p.y);
    await sleep(waitMs);
  };

  let failures = 0;
  const check = (ok, what) => {
    console.log((ok ? 'OK    ' : 'MISS  ') + spec.name + ': ' + what);
    if (!ok) failures++;
  };
  // Assertions read only what was logged since the previous one, so a line
  // left over from an earlier tap cannot satisfy a later check.
  let seen = 0;
  const since = () => {
    const text = logs.slice(seen).join('\n');
    seen = logs.length;
    return text;
  };
  const leave = async () => {
    await tap(HUB_BUTTON, 400);
    await tap(CONFIRM_YES, 1500);
    since();
  };

  // The miss goes first, while the game is known to be on the selection screen:
  // after an adventure has been opened it would also "pass" by never getting
  // back to the hub at all.
  since();
  await tap(EMPTY_SLOT, 1500);
  check(!since().includes('starting adventure'),
        'a tap on an empty grid slot opens nothing');

  await tap(GINA, 2000);
  check(since().includes(OPENED_GINA), 'a tap opens the cartridge under it');

  // The corner button and the tick are small and near the edges, where a
  // mapping that is merely stretched still lands somewhere plausible. Only a
  // cartridge opening afterwards proves both taps landed, since a cartridge can
  // only be opened from the selection screen.
  await leave();
  await tap(VANIA, 2000);
  check(since().includes(OPENED_VANIA),
        'the corner button and the tick both take a tap');

  await leave();
  await tap(GINA, 2000);
  check(since().includes(OPENED_GINA), 'a tap opens the other cartridge');

  await page.screenshot({ path: path.join(SHOTS, `touch-${spec.name}.png`) });
  await page.close();
  return failures;
}

(async () => {
  fs.mkdirSync(SHOTS, { recursive: true });
  const browser = await puppeteer.launch({
    headless: 'new',
    args: ['--no-sandbox', '--disable-setuid-sandbox', '--use-gl=angle',
           '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
           '--ignore-gpu-blocklist', '--enable-webgl'],
  });
  let failures = 0;
  for (const spec of VIEWPORTS) {
    failures += await run(browser, spec);
  }
  await browser.close();
  console.log(failures === 0 ? 'PASS: every tap landed'
                             : `FAIL: ${failures} tap(s) missed`);
  process.exit(failures === 0 ? 0 : 2);
})().catch((e) => { console.error('ERR', e.message); process.exit(1); });
