import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

const root = fileURLToPath(new URL('../../', import.meta.url));
const read = path => readFile(new URL(path, `file://${root}/`), 'utf8');

test('documentation images expose an accessible zoom lightbox', async () => {
  const theme = await read('wiki/.vitepress/theme/index.ts');
  const lightbox = await read('wiki/.vitepress/theme/image-lightbox.ts');
  const css = await read('wiki/.vitepress/theme/custom.css');

  assert.match(theme, /setupImageLightbox/);
  assert.match(theme, /enhanceApp/);
  assert.match(lightbox, /\.vp-doc img/);
  assert.match(lightbox, /role.*dialog/);
  assert.match(lightbox, /aria-modal/);
  assert.match(lightbox, /Escape/);
  assert.match(lightbox, /wheel/);
  assert.match(lightbox, /pointerdown/);
  assert.match(lightbox, /pointermove/);
  assert.match(lightbox, /pointerup/);
  assert.match(lightbox, /router\.onAfterRouteChange/);
  assert.match(css, /\.wr-image-lightbox/);
  assert.match(css, /cursor:\s*zoom-in/);
});
