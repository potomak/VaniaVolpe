// Round-trip check for the animation packer (src/emscripten/anim_packer.html).
//
// The acceptance test #82 asks for: cut a committed sheet back into frames
// using its own .anim, feed those frames through the packer, and the output
// must be the input. Both halves are checked — the .anim byte-for-byte, and
// the sheet pixel-for-pixel — because the two failure modes are different: a
// wrong .anim stutters in game, a wrong sheet is silently resampled.
//
// Usage: node test/web/run_anim_packer.js [<sheet.png> <sheet.anim>]
// Defaults to the fox's walking animation.

const fs = require('fs');
const os = require('os');
const path = require('path');
const puppeteer = require('puppeteer');

const URL = process.env.URL || 'http://localhost:8099/anim_packer.html';
const ROOT = path.resolve(__dirname, '../..');
const DEFAULT_SHEET =
  'src/adventures/vania_fox_the_slide/assets/common/fox/walking.png';

const sheetPath = path.resolve(ROOT, process.argv[2] || DEFAULT_SHEET);
const animPath = path.resolve(
  ROOT, process.argv[3] || sheetPath.replace(/\.png$/, '.anim'));

let failures = 0;
function check(ok, what) {
  console.log((ok ? 'OK    ' : 'MISS  ') + what);
  if (!ok) failures++;
}

(async () => {
  const expectedAnim = fs.readFileSync(animPath, 'utf8');
  const rects = expectedAnim.trim().split('\n').map((line) => {
    const [x, y, w, h] = line.split(',').map(Number);
    return { x, y, w, h };
  });
  const sheetB64 = fs.readFileSync(sheetPath).toString('base64');

  const browser = await puppeteer.launch({
    headless: 'new',
    args: ['--no-sandbox', '--disable-setuid-sandbox'],
  });
  const page = await browser.newPage();
  page.on('pageerror', (e) => check(false, 'page error: ' + e.message));
  await page.goto(URL, { waitUntil: 'load', timeout: 30000 });

  // Cut the committed sheet back into individual frame PNGs, in the browser,
  // then hand them to the packer exactly as a drop would.
  const result = await page.evaluate(async (sheetB64, rects) => {
    const sheet = new Image();
    await new Promise((resolve, reject) => {
      sheet.onload = resolve;
      sheet.onerror = reject;
      sheet.src = 'data:image/png;base64,' + sheetB64;
    });

    const files = [];
    for (let i = 0; i < rects.length; i++) {
      const r = rects[i];
      const c = document.createElement('canvas');
      c.width = r.w;
      c.height = r.h;
      c.getContext('2d').drawImage(sheet, r.x, r.y, r.w, r.h, 0, 0, r.w, r.h);
      const blob = await new Promise((res) => c.toBlob(res, 'image/png'));
      // Deliberately out of order and awkwardly named, so the natural sort is
      // what puts them back: 10 must not land between 1 and 2.
      files.push(new File([blob], `frame_${i + 1}.png`, { type: 'image/png' }));
    }
    files.reverse();

    await window.animPacker.addFiles(files);
    return {
      count: window.animPacker.frameCount(),
      anim: window.animPacker.animText(),
      sheet: window.animPacker.sheetDataURL(),
    };
  }, sheetB64, rects);

  check(result.count === rects.length,
        `every frame loaded (${result.count}/${rects.length})`);
  check(result.anim === expectedAnim,
        'the exported .anim matches the committed one byte-for-byte');

  const packed = Buffer.from(result.sheet.split(',')[1], 'base64');

  // Compare pixels rather than bytes: PNG encoders differ, the image must not.
  const same = await page.evaluate(async (aB64, bB64) => {
    const decode = async (b64) => {
      const img = new Image();
      await new Promise((res, rej) => {
        img.onload = res;
        img.onerror = rej;
        img.src = 'data:image/png;base64,' + b64;
      });
      const c = document.createElement('canvas');
      c.width = img.naturalWidth;
      c.height = img.naturalHeight;
      c.getContext('2d', { willReadFrequently: true }).drawImage(img, 0, 0);
      return {
        w: c.width,
        h: c.height,
        data: c.getContext('2d', { willReadFrequently: true })
              .getImageData(0, 0, c.width, c.height).data,
      };
    };
    const a = await decode(aB64);
    const b = await decode(bB64);
    if (a.w !== b.w || a.h !== b.h) return { ok: false, why: `${a.w}x${a.h} vs ${b.w}x${b.h}` };
    for (let i = 0; i < a.data.length; i++) {
      if (a.data[i] !== b.data[i]) {
        return { ok: false, why: `first difference at byte ${i}` };
      }
    }
    return { ok: true, why: `${a.w}x${a.h}, identical` };
  }, sheetB64, packed.toString('base64'));

  check(same.ok, `the exported sheet is pixel-identical (${same.why})`);
  if (!same.ok) {
    const out = path.join(os.tmpdir(), 'anim_packer_packed.png');
    fs.writeFileSync(out, packed);
    console.log('      wrote the mismatching sheet to ' + out);
  }

  // Ordering is filename-natural, and the hazard is numeric: a plain string
  // sort puts frame_10 between frame_1 and frame_2, which silently reorders an
  // animation. Four frames cannot reach that, so check it with eleven.
  const order = await page.evaluate(async () => {
    const files = [];
    for (let i = 1; i <= 11; i++) {
      const c = document.createElement('canvas');
      c.width = 2;
      c.height = 2;
      const ctx = c.getContext('2d');
      // Encode the index in the pixel, so the order can be read back out.
      ctx.fillStyle = `rgba(${i * 20}, 0, 0, 0.5)`;
      ctx.fillRect(0, 0, 2, 2);
      const blob = await new Promise((res) => c.toBlob(res, 'image/png'));
      files.push(new File([blob], `frame_${i}.png`, { type: 'image/png' }));
    }
    // Shuffled into the order a lexicographic sort would produce.
    files.sort((a, b) => a.name.localeCompare(b.name));
    window.animPacker.reset();
    await window.animPacker.addFiles(files);
    return window.animPacker.frameNames();
  });
  const expectedOrder =
    Array.from({ length: 11 }, (_, i) => `frame_${i + 1}.png`);
  check(JSON.stringify(order) === JSON.stringify(expectedOrder),
        `frame_10 sorts after frame_9, not after frame_1 (${order.join(' ')})`);

  await browser.close();
  console.log(failures === 0 ? 'PASS: round-trip is lossless'
                             : `FAIL: ${failures} check(s)`);
  process.exit(failures === 0 ? 0 : 2);
})().catch((e) => { console.error('ERR', e.message); process.exit(1); });
