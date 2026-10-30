import assert from 'node:assert/strict';
import { readFile, access, mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { generate } from '../content.mjs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import { buildSidebar } from '../navigation.mjs';

const root = fileURLToPath(new URL('../../', import.meta.url));
const read = path => readFile(resolve(root, path), 'utf8');

test('product navigation groups user tasks and only links to available release content', () => {
  const catalog = [
    {lang:'zh', route:'/', title:'WatcheRobot'},
    {lang:'zh', route:'/products/watcherobot/device-guide', title:'设备使用指南'},
    {lang:'zh', route:'/docs/sdk', title:'SDK 安装与运行'},
    {lang:'en', route:'/en/', title:'WatcheRobot'},
  ];
  const sidebar = buildSidebar(catalog, 'zh');
  assert.equal(sidebar[0].text, 'WatcheRobot');
  const links = JSON.stringify(sidebar);
  assert.match(links, /设备使用指南/);
  assert.match(links, /Python SDK/);
  assert.doesNotMatch(links, /\/en\/|community/);
  assert.deepEqual(buildSidebar([], 'zh'), []);
});

test('C SDK preview is separate from Python and HIL maintenance stays hidden', () => {
  const visible = ['/docs/embedded-sdk', '/firmware/sdk/', '/firmware/sdk/docs/getting-started', '/firmware/sdk/docs/hardware-check'];
  for (const lang of ['zh', 'en']) {
    const prefix = lang === 'zh' ? '' : '/en';
    const catalog = [...visible, '/guide/hardware-in-the-loop', '/docs/sdk', '/hardware/'].map((route, index) => ({
      lang,
      route: prefix + route,
      title: `Hidden preview ${index}`,
    }));
    const links = JSON.stringify(buildSidebar(catalog, lang));
    for (const route of visible) assert.ok(links.includes(prefix + route));
    assert.match(links, /C SDK/);
    assert.match(links, /Python SDK/);
    assert.match(links, /Hardware|硬件专区/);
    assert.doesNotMatch(links, /hardware-in-the-loop/);
  }
});

test('assembly tutorial is bilingual, linked from hardware entry points, and uses published media', async () => {
  const pages = [
    ['wiki/guides/guide/assembly.md', '/guide/assembly', '整机拼装教程'],
    ['wiki/guides/en/guide/assembly.md', '/en/guide/assembly', 'Whole-robot assembly tutorial'],
  ];
  for (const [path, route, title] of pages) {
    const content = await read(path);
    assert.match(content, new RegExp(`^# ${title}`, 'm'));
    for (const media of ['parts-overview.mp4', 'electronics-overview.mp4', 'fasteners-overview.mp4']) {
      assert.match(content, new RegExp(`/media/docs/assembly/${media}`));
    }
    assert.match(content, /https:\/\/wiki\.orulink\.ai\/media-assets\/assembly\/watcherobot-assembly-tutorial-720p-7cd0f58e\.mp4/);
    assert.doesNotMatch(content, /github\.com\/orulink-ai\/WatcheRobot-public\/releases\/download\/wiki-media-assembly/);
    const sections = [...content.matchAll(/^## .+\{#([^}]+)\}/gm)].map(match => match[1]);
    assert.deepEqual(sections, ['parts', 'video']);
    assert.doesNotMatch(content, /\{#steps\}|\{#check\}/);
    const lang = route.startsWith('/en/') ? 'en' : 'zh';
    const catalog = [{lang, route, title}];
    assert.match(JSON.stringify(buildSidebar(catalog, lang)), /assembly/);
  }
  for (const path of ['wiki/guides/guide/reproduction.md', 'wiki/guides/en/guide/reproduction.md', 'hardware/pcb/spares_zh.md', 'hardware/pcb/spares.md']) {
    const content = await read(path);
    assert.match(content, /assembly\.md/);
    assert.doesNotMatch(content, /文字步骤|text steps/);
  }
});

test('new guides have bilingual counterparts and locally owned image assets', async () => {
  for (const name of ['device-guide', 'capabilities', 'community', 'voice']) {
    const zhPath = `wiki/guides/products/watcherobot/${name}.md`;
    const enPath = `wiki/guides/en/products/watcherobot/${name}.md`;
    const zh = await read(zhPath);
    const en = await read(enPath);
    assert.ok(zh.startsWith('[English]'), zhPath);
    assert.ok(en.startsWith('English |'), enPath);
    const headings = text => [...text.matchAll(/^##+ .+\{#([^}]+)\}/gm)].map(m => m[1]);
    assert.deepEqual(headings(zh), headings(en), name);
    const images = text => [...text.matchAll(/!\[[^\]]*\]\(([^)]+)\)/g)].map(m => m[1]);
    assert.deepEqual(images(zh).map(p => p.split('/').pop()), images(en).map(p => p.split('/').pop()), name);
    for (const [path, content] of [[zhPath, zh], [enPath, en]]) {
      for (const image of images(content)) {
        assert.ok(!image.startsWith('http'), image);
        await access(resolve(root, dirname(path), image));
      }
    }
  }
});

test('entry points use maintained guides, not the old manual or empty inventory', async () => {
  for (const path of ['README_zh.md', 'README.md']) {
    const content = await read(path);
    assert.doesNotMatch(content, /## (目录结构|Repository structure|Repository Structure|Directory structure)/);
    assert.doesNotMatch(content, /device-network-setup\.pdf|WatcheRobot-user-manual-20260724\.pdf/);
    assert.match(content, /device-guide\.md/);
  }
  for (const path of ['hardware/pcb/spares_zh.md', 'hardware/pcb/spares.md']) {
    const content = await read(path);
    assert.doesNotMatch(content, /待填写|TBD|To be filled/i);
    assert.match(content, /BOM_Assembly/);
  }
});

test('quick start is for assembled robots and DIY instructions have a separate guide', async () => {
  for (const [file, title, prefix] of [['README_zh.md', '快速开始', ''], ['README.md', 'Quick Start', 'en/']]) {
    const content = await read(file);
    const quick = content.split(`## ${title}\n`)[1].split('\n## ')[0];
    assert.doesNotMatch(quick, /git clone|ST-LINK|conda create/);
    assert.match(quick, /Desktop Link/);
    assert.ok(content.includes(`wiki/guides/${prefix}guide/reproduction.md`));
    const guide = await read(`wiki/guides/${prefix}guide/reproduction.md`);
    assert.doesNotMatch(guide, /已装配的 Watcher 头部和机器人身体|Assembled Watcher head and robot body/);
    const stages = [...guide.matchAll(/^## .+\{#([^}]+)\}/gm)].map(match => match[1]);
    assert.deepEqual(stages, ['parts', 'printing', 'pcb', 'assembly', 'firmware', 'verify']);
    assert.ok(guide.indexOf('makerworld.com.cn') < guide.indexOf('{#firmware}'));
    assert.ok(guide.indexOf('oshwhub.com') < guide.indexOf('{#firmware}'));
    assert.match(guide, /BOM_Assembly/);
    assert.match(guide, /PNhqwdCVEiqizvkelq9c2q7Qnth/);
    assert.match(guide, /pcb-fabrication(?:_zh)?\.md/);
    assert.match(guide, /flashing(?:_zh)?\.md#full/);
    assert.match(guide, /getting-started\.md/);
  }
});

test('SDK instructions activate the environment first and use managed Applications', async () => {
  for (const path of ['docs/sdk_zh.md', 'docs/sdk.md']) {
    const content = await read(path);
    assert.ok(content.indexOf('conda activate') < content.indexOf('pip install'));
    assert.match(content, /Desktop Link/);
    assert.match(content, /watcherobot app run/);
    assert.doesNotMatch(content, /python my_robot\.py/);
  }
});

test('generated pages keep internal page and asset links resolvable in both languages', async () => {
  const output = await mkdtemp(resolve(tmpdir(), 'watche-doc-links-'));
  try {
    const catalog = await generate(root, output, 'main');
    const routes = new Set(catalog.map(page => page.route));
    for (const page of catalog) {
      const file = resolve(output, '.' + page.route + (page.route.endsWith('/') ? 'index.md' : '.md'));
      const content = (await readFile(file, 'utf8')).replace(/```[\s\S]*?```/g, '');
      const targets = [...content.matchAll(/\]\((\/[^\s)]+)\)|(?:src|href)="(\/[^"\s]+)"/g)]
        .map(match => (match[1] ?? match[2]).split(/[?#]/)[0]);
      for (const target of targets) {
        if (target.startsWith('/media/')) await access(resolve(output, 'public', '.' + decodeURI(target)));
        else assert.ok(routes.has(target), `${page.route} -> ${target}`);
      }
    }
  } finally {
    await rm(output, {recursive:true, force:true});
  }
});
