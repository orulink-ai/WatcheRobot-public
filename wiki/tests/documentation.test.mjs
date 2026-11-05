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

test('resource indexes and support rules use a single sidebar entry in each language', () => {
  const hardware = ['/guide/reproduction', '/docs/pcb-fabrication', '/guide/assembly', '/hardware/'];
  const removed = ['/hardware/pcb/spares', '/firmware/', '/docs/versions', '/docs/compatibility', '/CONTRIBUTING', '/SECURITY', '/docs/governance', '/docs/release-process', '/skills/watche-release-flash/', '/skills/watche-sd-card-write/', '/troubleshooting/'];
  for (const lang of ['zh', 'en']) {
    const prefix = lang === 'zh' ? '' : '/en';
    const catalog = [...hardware, ...removed, '/products/watcherobot/community'].map(route => ({lang, route:prefix + route, title:route}));
    const groups = buildSidebar(catalog, lang)[0].items;
    assert.deepEqual(groups.find(g => g.text === (lang === 'zh' ? '硬件专区' : 'Hardware')).items.map(i => i.link), hardware.map(r => prefix + r));
    assert.deepEqual(groups.find(g => g.text === (lang === 'zh' ? '支持与贡献' : 'Support and contribution')).items.map(i => i.link), [prefix + '/products/watcherobot/community']);
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
    for (const media of ['01_parts_reveal_preview_full_cover.gif', '02_electronics_reveal_full_cover.gif', '03_screws_reveal_full_cover.gif']) {
      assert.match(content, new RegExp(`/media/docs/assembly/${media.replace('.', '\\.')}`));
    }
    assert.doesNotMatch(content, /(?:parts|electronics|fasteners)-overview\.mp4/);
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

test('both community pages offer Discord without removing the existing WeChat QR code', async () => {
  for (const prefix of ['', 'en/']) {
    const content = await read(`wiki/guides/${prefix}products/watcherobot/community.md`);
    const join = content.split('{#join}')[1].split('\n## ')[0];
    assert.match(join, /\[.*Discord.*\]\(https:\/\/discord\.gg\/VJWGxPDGm\)/);
    assert.match(join, /community-wechat\.jpg/);
  }
});

test('community channels use matching responsive cards in both languages', async () => {
  for (const prefix of ['', 'en/']) {
    const content = await read(`wiki/guides/${prefix}products/watcherobot/community.md`);
    const join = content.split('{#join}')[1].split('\n## ')[0];
    assert.match(join, /class="community-channels"/);
    assert.equal([...join.matchAll(/class="community-channel"/g)].length, 2);
  }
  const css = await read('wiki/.vitepress/theme/custom.css');
  assert.match(css, /\.community-channels\s*\{[^}]*grid-template-columns:\s*repeat\(2,\s*minmax\(0,\s*1fr\)\)/s);
  assert.match(css, /@media\s*\(max-width:\s*640px\)\s*\{\s*\.community-channels\s*\{\s*grid-template-columns:\s*1fr/s);
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

test('home introduces the product before setup and capabilities start with a plain-language list', async () => {
  for (const [home, capability, intro] of [
    ['README_zh.md', 'wiki/guides/products/watcherobot/capabilities.md', '## 产品介绍'],
    ['README.md', 'wiki/guides/en/products/watcherobot/capabilities.md', '## Product introduction'],
  ]) {
    const content = await read(home);
    assert.ok(content.includes(intro));
    assert.ok(content.indexOf(intro) < content.indexOf(home === 'README.md' ? '## Quick Start' : '## 快速开始'));
    assert.match(content.split(intro)[1].split('\n## ')[0], /capabilities\.md/);
    const guide = await read(capability);
    const overview = guide.split('{#overview}')[1]?.split('\n## ')[0];
    assert.ok(overview, capability);
    assert.ok([...overview.matchAll(/^- /gm)].length >= 10);
    for (const anchor of ['motion', 'display', 'audio', 'vision', 'inputs', 'composed']) {
      assert.ok(overview.includes(`](#${anchor})`), anchor);
    }
    assert.ok(guide.indexOf('{#overview}') < guide.indexOf('robot.motion.move_to()'));
    assert.match(guide, /robot\.inputs\.wait/);
    assert.match(guide, /robot\.capabilities/);
  }
});

test('home puts the product image first and capability overview leads straight to APIs', async () => {
  for (const [home, capability, intro] of [
    ['README_zh.md', 'wiki/guides/products/watcherobot/capabilities.md', '## 产品介绍'],
    ['README.md', 'wiki/guides/en/products/watcherobot/capabilities.md', '## Product introduction'],
  ]) {
    const content = await read(home);
    const afterTitle = content.split('# WatcheRobot\n')[1].trimStart();
    assert.ok(afterTitle.startsWith('!['), home);
    assert.ok(content.indexOf('watcher-robot-render.png') < content.indexOf(intro));
    assert.doesNotMatch(content, /wiki\.orulink\.ai|wiki\/README|## 项目预览|## Project Preview|## 项目文档|## Project Documentation/);
    const guide = await read(capability);
    const overview = guide.split('{#overview}')[1].split('\n## ')[0];
    assert.doesNotMatch(overview, /cc0a319|7699527|2026-10-15|Application 接入|Before developing|Checked on|先看机器人|Start with what/);
    assert.ok(overview.trim().endsWith('](#composed)'));
  }
});

test('Wiki brand uses the original ORULINK logo and links to the official website', async () => {
  const config = await read('wiki/.vitepress/config.mts');
  assert.match(config, /siteTitle:\s*false/);
  assert.match(config, /logo:\s*\{\s*src:\s*'\/media\/docs\/images\/brand\/orulink-logo\.png',\s*alt:\s*'ORULINK'/);
  assert.match(config, /logoLink:\s*\{\s*link:\s*websiteUrl,\s*target:\s*'_self'/);
  assert.match(config, /process\.env\.WIKI_WEBSITE_URL\s*\?\?\s*'https:\/\/orulink\.ai\/'/);
  const logo = await readFile(resolve(root, 'docs/images/brand/orulink-logo.png'));
  assert.deepEqual([...logo.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
  const css = await read('wiki/.vitepress/theme/custom.css');
  assert.doesNotMatch(css, /\.VPNavBarTitle \.title::before/);
  assert.match(css, /\.dark \.VPNavBarTitle \.logo\s*\{[^}]*invert\(1\)/s);
});

test('Wiki visual theme shares the website monochrome palette and local MiSans typography', async () => {
  const css = await read('wiki/.vitepress/theme/custom.css');
  for (const level of [1, 2, 3]) {
    assert.match(css, new RegExp(`\\.vp-doc h${level} \\{[^}]*font-weight: 400;`));
  }
  assert.match(css, /font-synthesis: none/);
  assert.match(css, /--vp-c-bg:\s*#050505/);
  assert.match(css, /--wr-radius-md:\s*3px/);
  assert.match(css, /--vp-font-family-base:\s*"MiSans"/);
  assert.doesNotMatch(css, /#a1d42a|#567c00|#86b51b|#0d1725|#b7e550/i);
  const config = await read('wiki/.vitepress/config.mts');
  assert.match(config, /appearance:\s*'dark'/);
  assert.match(config, /\/media\/docs\/fonts\/brand\.css/);
  const fonts = await read('docs/fonts/brand.css');
  assert.match(fonts, /MiSans-Regular\.min\.css/);
  await access(resolve(root, 'docs/fonts/misans/LICENSE.txt'));
});

test('Skill-led app onboarding provides the full download and bilingual entry points', async () => {
  for (const locale of ['', 'en/']) {
    const text = await read(`wiki/guides/${locale}guide/create-with-agent.md`);
    assert.match(text, /<SkillPrompt/);
    assert.match(text, /\/downloads\/watche-robot-app\.zip/);
    assert.match(text, /\/downloads\/watche-robot-app\/SKILL\.md/);
    assert.match(text, /Python 3\.10/);
  }
  const nav = await read('wiki/navigation.mjs');
  assert.match(nav, /'Python SDK', 'Python SDK', \['\/docs\/sdk', '\/guide\/create-with-agent'/);
  const component = await read('wiki/.vitepress/theme/SkillPrompt.vue');
  assert.match(component, /window\.location\.origin/);
  assert.match(component, /navigator\.clipboard\.writeText/);
  assert.match(component, /document\.execCommand\('copy'\)/);
  assert.doesNotMatch(component, /v-if="manual"|手动复制提示词/);
  assert.match(component, /v-model="requirement"/);
  assert.match(component, /immediate: true/);
  assert.match(component, /<details class="customize">/);
  assert.match(component, /:disabled="!prompt"/);
  assert.doesNotMatch(component, /\[填写我的需求\]/);
  await access(resolve(root, 'wiki/downloads/watche-robot-app.zip'));
  await access(resolve(root, 'wiki/downloads/watche-robot-app/scripts/workflow.py'));
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
        if (target.startsWith('/media/') || target.startsWith('/downloads/')) await access(resolve(output, 'public', '.' + decodeURI(target)));
        else assert.ok(routes.has(target), `${page.route} -> ${target}`);
      }
    }
  } finally {
    await rm(output, {recursive:true, force:true});
  }
});
