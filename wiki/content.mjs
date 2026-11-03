import { cp, mkdir, readFile, readdir, rm, writeFile } from 'node:fs/promises';
import { dirname, join, posix, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const repository = 'https://github.com/orulink-ai/WatcheRobot-public';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const exists = async path => { try { await readFile(path); return true; } catch (error) { if(error.code === 'ENOENT') return false; throw error; } };

async function markdownFiles(directory, prefix = '') {
  let entries;
  try { entries = await readdir(directory, {withFileTypes:true}); }
  catch (error) { if(error.code === 'ENOENT') return []; throw error; }
  const files = await Promise.all(entries.map(async entry => {
    const path = posix.join(prefix, entry.name);
    if (entry.isDirectory()) return markdownFiles(join(directory, entry.name), path);
    return entry.name.endsWith('.md') ? [path] : [];
  }));
  return files.flat().sort();
}

function routeFor(source) {
  if(source.startsWith('wiki/guides/')) {
    const relative = source.slice('wiki/guides/'.length).replace(/\.md$/, '');
    return '/' + relative.replace(/(^|\/)index$/, '$1');
  }
  const chinese = /_zh\.md$/.test(source);
  const page = source.replace(/_zh\.md$|\.md$/g, '').replace(/README$|SKILL$/, 'index');
  return (chinese ? '/' : '/en/') + page.replace(/(^|\/)index$/, '$1');
}

export function rewriteLinks(content, source, routes, ref) {
  const rewrite = target => {
    if (/^(?:[a-z][a-z\d+.-]*:|\/|#)/i.test(target)) return target;
    const match = target.match(/^([^?#]+)(.*)$/);
    if(!match) return target;
    const path = posix.normalize(posix.join(posix.dirname(source), decodeURI(match[1])));
    const suffix = match[2];
    if(routes.has(path)) {
      const mapped = routes.get(path);
      if (!mapped.includes('#')) return mapped + suffix;
      // Old article sections no longer exist: land on the hub's relevant section.
      const [route, anchor] = mapped.split('#');
      const query = suffix.startsWith('?') ? suffix.split('#')[0] : '';
      return route + query + '#' + anchor;
    }
    if(path.startsWith('docs/images/') || path.startsWith('docs/manuals/')) return '/media/' + path + suffix;
    if(path.startsWith('python-sdk/')) return 'https://github.com/orulink-ai/WatcheRobot_python_sdk/blob/main/' + path.slice(11) + suffix;
    return `${repository}/blob/${encodeURIComponent(ref)}/${path.split('/').map(encodeURIComponent).join('/')}${suffix}`;
  };
  // Fenced and inline code are examples, not navigation targets.
  return content.split(/(```[\s\S]*?```|~~~[\s\S]*?~~~)/g).map((part, i) => i % 2 ? part : part
    .replace(/`[^`\n]*`|(!?\[[^\]]*\]\()([^\s)]+)([^)]*\))|^(\s*\[[^\]]+\]:\s*)(\S+)|\b(href|src)=(['"])([^'"]+)\7/gm,
      (match, start, target, end, reference, referenceTarget, attr, quote, htmlTarget) => {
        if(start) return start + rewrite(target) + end;
        if(reference) return reference + rewrite(referenceTarget);
        if(attr) return `${attr}=${quote}${rewrite(htmlTarget)}${quote}`;
        return match;
      })
  ).join('');
}

// Repository policy/tool documents stay in GitHub; Wiki readers use the two hubs.
// Preserve old public URLs as non-indexed redirects, not duplicate Wiki articles.
const consolidatedPages = [
  ['hardware/pcb/spares', 'hardware/README', 'hardware'],
  ['firmware/README', 'hardware/README', 'firmware'],
  ['docs/versions', 'hardware/README', 'versions'],
  ['docs/compatibility', 'hardware/README', 'versions'],
  ['CONTRIBUTING', 'community', 'contribute'],
  ['SECURITY', 'community', 'security'],
  ['docs/governance', 'community', 'contribute'],
  ['docs/release-process', 'community', 'contribute'],
  ['skills/watche-release-flash/SKILL', 'community', 'tools'],
  ['skills/watche-sd-card-write/SKILL', 'community', 'tools'],
];

export async function generate(sourceRoot, destination, ref = 'main') {
  const candidates = ['README.md', 'README_zh.md', 'CONTRIBUTING.md', 'CONTRIBUTING_zh.md', 'SECURITY.md', 'SECURITY_zh.md',
    'firmware/README.md', 'firmware/README_zh.md', 'firmware/sdk/README.md', 'firmware/sdk/README_zh.md',
    'firmware/sdk/docs/getting-started.md', 'firmware/sdk/docs/getting-started_zh.md',
    'firmware/sdk/docs/hardware-check.md', 'firmware/sdk/docs/hardware-check_zh.md',
    'hardware/README.md', 'hardware/README_zh.md', 'hardware/pcb/spares.md', 'hardware/pcb/spares_zh.md',
    ...(await markdownFiles(join(sourceRoot, 'docs'))).map(path => 'docs/' + path),
    ...(await markdownFiles(join(sourceRoot, 'skills'))).map(path => 'skills/' + path),
    ...(await markdownFiles(join(sourceRoot, 'wiki/guides'))).map(path => 'wiki/guides/' + path)];
  const sources = [];
  for(const source of candidates) if(await exists(join(sourceRoot, source))) sources.push(source);
  const routes = new Map(sources.map(source => [source, routeFor(source)]));
  const redirects = new Map();
  for (const suffix of ['_zh', '']) {
    for (const [page, hub, anchor] of consolidatedPages) {
      const source = page + suffix + '.md';
      const targetSource = hub === 'community'
        ? `wiki/guides/${suffix ? '' : 'en/'}products/watcherobot/community.md`
        : hub + suffix + '.md';
      if (routes.has(source) && routes.has(targetSource)
          && (await readFile(join(sourceRoot, targetSource), 'utf8')).includes(`{#${anchor}}`)) {
        const target = routes.get(targetSource) + '#' + anchor;
        redirects.set(source, {route:routeFor(source), target});
        routes.set(source, target);
      }
    }
  }
  await rm(destination, {recursive:true, force:true});
  await mkdir(join(destination, 'public'), {recursive:true});
  const catalog = [];
  for(const source of sources) {
    if (redirects.has(source)) {
      const {route, target} = redirects.get(source);
      const output = join(destination, 'public', route + (route.endsWith('/') ? 'index.html' : '.html'));
      await mkdir(dirname(output), {recursive:true});
      await writeFile(output, `<!doctype html><html lang="${route.startsWith('/en/') ? 'en' : 'zh-CN'}"><head><meta charset="utf-8"><meta name="robots" content="noindex"><meta http-equiv="refresh" content="0;url=${target}"><link rel="canonical" href="${target}"><title>${route.startsWith('/en/') ? 'Page moved' : '页面已合并'}</title></head><body><a href="${target}">${route.startsWith('/en/') ? 'Continue to the consolidated guide' : '前往合并后的指引'}</a></body></html>`);
      continue;
    }
    const content = await readFile(join(sourceRoot, source), 'utf8');
    const route = routes.get(source);
    const output = join(destination, route + (route.endsWith('/') ? 'index.md' : '.md'));
    await mkdir(dirname(output), {recursive:true});
    await writeFile(output, rewriteLinks(content, source, routes, ref));
    catalog.push({source, route, title:content.match(/^#\s+(.+)$/m)?.[1] ?? source, lang:route.startsWith('/en/') ? 'en' : 'zh'});
  }
  for(const asset of ['docs/images', 'docs/manuals', 'docs/assembly']) {
    try { await cp(join(sourceRoot, asset), join(destination, 'public/media', asset), {recursive:true}); }
    catch(error) { if(error.code !== 'ENOENT') throw error; }
  }
  await writeFile(join(destination, 'catalog.json'), JSON.stringify(catalog, null, 2));
  await writeFile(join(destination, 'public/version.json'), JSON.stringify({repository:'orulink-ai/WatcheRobot-public', sourceRef:ref}, null, 2));
  return catalog;
}

if(process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const sourceRoot = resolve(process.env.WIKI_SOURCE_DIR ?? root);
  const ref = process.env.WIKI_SOURCE_REF ?? 'main';
  // Output is always confined to the known generated directory.
  const catalog = await generate(sourceRoot, join(root, 'wiki/.generated'), ref);
  console.log(`Prepared ${catalog.length} Wiki pages from ${ref}`);
}
