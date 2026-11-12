import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';
import { generate } from '../content.mjs';

test('removed firmware sources keep bilingual compatibility URLs without importing C SDK docs', async () => {
  const root = await mkdtemp(join(tmpdir(), 'watche-sdk-migration-'));
  try {
    await mkdir(join(root, 'docs'), {recursive:true});
    await mkdir(join(root, 'hardware'), {recursive:true});
    for (const suffix of ['', '_zh']) {
      await writeFile(join(root, `docs/embedded-sdk${suffix}.md`), '# Current C SDK entry');
      await writeFile(join(root, `hardware/README${suffix}.md`), '# Resources\n## Firmware {#firmware}');
    }
    const output = join(root, 'out');
    const catalog = await generate(root, output, 'migration');
    assert.equal(catalog.length, 4);
    assert.ok(catalog.every(page => !page.source.startsWith('firmware/')));
    for (const prefix of ['', '/en']) {
      for (const [route, target] of [
        ['/firmware/', '/hardware/#firmware'],
        ['/firmware/sdk/', '/docs/embedded-sdk'],
        ['/firmware/sdk/docs/getting-started', '/docs/embedded-sdk'],
        ['/firmware/sdk/docs/hardware-check', '/docs/embedded-sdk'],
      ]) {
        const file = join(output, 'public', prefix + route + (route.endsWith('/') ? 'index.html' : '.html'));
        const html = await readFile(file, 'utf8');
        assert.ok(html.includes(`content="0;url=${prefix}${target}"`), html);
        assert.match(html, /noindex/);
      }
    }
    // Historical releases without the new hub keep their original snapshot pages.
    await rm(join(root, 'docs'), {recursive:true});
    await mkdir(join(root, 'firmware/sdk'), {recursive:true});
    await writeFile(join(root, 'firmware/sdk/README.md'), '# Historical SDK');
    const legacy = await generate(root, output, 'legacy');
    assert.ok(legacy.some(page => page.route === '/en/firmware/sdk/'));
  } finally {
    await rm(root, {recursive:true, force:true});
  }
});
