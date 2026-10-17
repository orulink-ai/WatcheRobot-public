import assert from 'node:assert/strict';
import { access, readFile, readdir } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import { buildSidebar } from '../navigation.mjs';

const root = fileURLToPath(new URL('../../', import.meta.url));
const read = path => readFile(resolve(root, path), 'utf8');
const files = ['docs/pcb-fabrication_zh.md', 'docs/pcb-fabrication.md'];
const anchors = ['open-design', 'pcb-options', 'smt', 'receive'];
const approvedImages = ['select-board.jpg', 'pcb-options.jpg', 'smt-options.jpg'];

test('PCB guide has matching bilingual stages and only reviewed local images', async () => {
  for (const file of files) {
    const content = await read(file);
    assert.deepEqual([...content.matchAll(/^## .+\{#([^}]+)\}/gm)].map(m => m[1]), anchors);
    const images = [...content.matchAll(/!\[[^\]]+\]\(([^)]+)\)/g)].map(m => m[1]);
    assert.deepEqual(images.map(path => path.split('/').pop()), approvedImages);
    for (const image of images) await access(resolve(root, dirname(file), image));
    assert.match(content, /YSAlwY4EZiuR0jkYcu2ctHzKn3R/);
    assert.match(content, /reproduction\.md#assembly/);
    assert.match(content, /导出 → PCB\/FPC 一键打样/);
    assert.match(content, /FR-4/);
    assert.match(content, /1\.2 mm/);
    assert.ok(content.indexOf('确认并提交') < content.indexOf('{#smt}'));
  }
  assert.deepEqual((await readdir(resolve(root, 'docs/images/pcb-fabrication'))).sort(), [...approvedImages].sort());
  assert.match(await read(files[0]), /^\[English\]\(pcb-fabrication\.md\)/);
  assert.match(await read(files[1]), /^English \| \[简体中文\]\(pcb-fabrication_zh\.md\)/);
});

test('PCB guide is reachable from reproduction, hardware, and both sidebars', async () => {
  for (const [file, target] of [
    ['wiki/guides/guide/reproduction.md', '../../../docs/pcb-fabrication_zh.md'],
    ['wiki/guides/en/guide/reproduction.md', '../../../../docs/pcb-fabrication.md'],
    ['hardware/README_zh.md', '../docs/pcb-fabrication_zh.md'],
    ['hardware/README.md', '../docs/pcb-fabrication.md'],
  ]) assert.ok((await read(file)).includes(target), file);
  for (const [lang, route] of [['zh', '/docs/pcb-fabrication'], ['en', '/en/docs/pcb-fabrication']]) {
    const sidebar = buildSidebar([{lang, route, title: 'PCB'}], lang);
    assert.ok(JSON.stringify(sidebar).includes(route));
  }
});
