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
