import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';
import { generate, rewriteLinks } from '../content.mjs';

test('Markdown and HTML links map bilingual pages and local assets, leaving code alone', () => {
  const routes = new Map([
    ['docs/flashing_zh.md', '/docs/flashing'],
    ['docs/flashing.md', '/en/docs/flashing'],
    ['README_zh.md', '/'],
  ]);
  const input = '[English](flashing.md) [首页](../README_zh.md#开始)\n<img src="images/wiring.png">\n```md\n[x](flashing.md)\n```';
  const output = rewriteLinks(input, 'docs/flashing_zh.md', routes, 'v1');
  assert.match(output, /\[English\]\(\/en\/docs\/flashing\)/);
  assert.match(output, /\[首页\]\(\/#开始\)/);
  assert.match(output, /src="\/media\/docs\/images\/wiring.png"/);
  assert.match(output, /```md\n\[x\]\(flashing.md\)\n```/);
});

test('unrendered source downloads point to the selected GitHub revision', () => {
  const output = rewriteLinks('[source](../tools/flash.sh)', 'docs/flashing.md', new Map(), 'v1');
  assert.match(output, /github.com\/orulink-ai\/WatcheRobot-public\/blob\/v1\/tools\/flash.sh/);
});

test('reference-style links use their canonical page route', () => {
  const output = rewriteLinks('[flash]: ../skills/watche-release-flash/SKILL_zh.md', 'firmware/README_zh.md', new Map([['skills/watche-release-flash/SKILL_zh.md', '/skills/watche-release-flash/']]), 'v1');
  assert.equal(output, '[flash]: /skills/watche-release-flash/');
});

test('code labels still link correctly while inline code examples remain literal', () => {
  const output = rewriteLinks('[`flash.md`](flashing.md) and `[x](flashing.md)`', 'docs/flashing_zh.md', new Map([['docs/flashing.md', '/en/docs/flashing']]), 'v1');
  assert.equal(output, '[`flash.md`](/en/docs/flashing) and `[x](flashing.md)`');
});

test('consolidated links keep queries and use at most one section fragment', () => {
  const routes = new Map([['docs/versions_zh.md', '/hardware/#versions']]);
  const output = rewriteLinks('[query](versions_zh.md?from=home) [section](versions_zh.md#details)', 'docs/flashing_zh.md', routes, 'review');
  assert.equal(output, '[query](/hardware/?from=home#versions) [section](/hardware/#versions)');
});

test('consolidated indexes become bilingual redirects rather than duplicate searchable pages', async () => {
  const root = await mkdtemp(join(tmpdir(), 'watche-consolidated-'));
  const output = join(root, 'out');
  try {
    const pairs = [
      ['hardware/pcb/spares', '/hardware/pcb/spares', '/hardware/#hardware'],
      ['firmware/README', '/firmware/', '/hardware/#firmware'],
      ['docs/versions', '/docs/versions', '/hardware/#versions'],
      ['docs/compatibility', '/docs/compatibility', '/hardware/#versions'],
      ['CONTRIBUTING', '/CONTRIBUTING', '/products/watcherobot/community#contribute'],
      ['SECURITY', '/SECURITY', '/products/watcherobot/community#security'],
      ['docs/governance', '/docs/governance', '/products/watcherobot/community#contribute'],
      ['docs/release-process', '/docs/release-process', '/products/watcherobot/community#contribute'],
      ['skills/watche-release-flash/SKILL', '/skills/watche-release-flash/', '/products/watcherobot/community#tools'],
      ['skills/watche-sd-card-write/SKILL', '/skills/watche-sd-card-write/', '/products/watcherobot/community#tools'],
    ];
    for (const [suffix, prefix, hub] of [['_zh', '', 'wiki/guides/products/watcherobot/community.md'], ['', '/en', 'wiki/guides/en/products/watcherobot/community.md']]) {
      const sources = [[`hardware/README${suffix}.md`, '# Resource hub\n## Hardware {#hardware}\n## Firmware {#firmware}\n## Versions {#versions}'], [hub, '# Support hub\n## Contribute {#contribute}\n## Security {#security}\n## Tools {#tools}'], [`README${suffix}.md`, '[BOM](hardware/pcb/spares' + suffix + '.md)']];
      for (const [source] of pairs) sources.push([source + suffix + '.md', '# Obsolete index']);
      for (const [path, content] of sources) {
        await mkdir(join(root, path, '..'), {recursive:true});
        await writeFile(join(root, path), content);
      }
    }
    const catalog = await generate(root, output, 'review-ref');
    for (const prefix of ['', '/en']) {
      for (const [, route, target] of pairs) {
        assert.ok(!catalog.some(page => page.route === prefix + route), prefix + route);
        const redirect = await readFile(join(output, 'public', prefix + route + (route.endsWith('/') ? 'index.html' : '.html')), 'utf8');
        assert.ok(redirect.includes(prefix + target), redirect);
        assert.match(redirect, /http-equiv="refresh"/);
        assert.match(redirect, /noindex/);
      }
      const home = await readFile(join(output, prefix, 'index.md'), 'utf8');
      assert.ok(home.includes(prefix + '/hardware/#hardware'), home);
    }
    // Older releases without hubs still retain their original documentation.
    for(const [source, content] of [['hardware/README_zh.md', '# Legacy resource index']]) {
      await writeFile(join(root, source), content);
    }
    const oldHub = await generate(root, output, 'old-hub-release');
    assert.ok(oldHub.some(page => page.route === '/hardware/pcb/spares'), 'old hubs without consolidated sections must retain their original pages');
    await rm(join(root, 'hardware/README_zh.md'));
    const legacy = await generate(root, output, 'older-release');
    assert.ok(legacy.some(page => page.route === '/hardware/pcb/spares'));
  } finally { await rm(root, {recursive:true, force:true}); }
});

test('generation reads canonical source, copies assets and records source revision', async () => {
  const root = await mkdtemp(join(tmpdir(), 'watche-wiki-'));
  try {
    await mkdir(join(root, 'docs/images'), {recursive:true});
    await writeFile(join(root, 'README_zh.md'), '# 项目\n[烧录](docs/flashing_zh.md)');
    await writeFile(join(root, 'README.md'), '# Project');
    await writeFile(join(root, 'docs/flashing_zh.md'), '# 烧录');
    await writeFile(join(root, 'docs/flashing.md'), '# Flashing');
    await writeFile(join(root, 'docs/images/test.png'), 'fixture');
    await generate(root, join(root, 'out'), 'v1');
    assert.match(await readFile(join(root, 'out/index.md'), 'utf8'), /\[烧录\]\(\/docs\/flashing\)/);
    assert.equal(await readFile(join(root, 'out/public/media/docs/images/test.png'), 'utf8'), 'fixture');
    assert.equal(JSON.parse(await readFile(join(root, 'out/public/version.json'))).sourceRef, 'v1');
    await writeFile(join(root, 'README_zh.md'), '# 新内容');
    await generate(root, join(root, 'out'), 'v2');
    assert.match(await readFile(join(root, 'out/index.md'), 'utf8'), /新内容/);
  } finally { await rm(root, {recursive:true, force:true}); }
});
