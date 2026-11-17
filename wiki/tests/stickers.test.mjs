import assert from 'node:assert/strict';
import {readFile, access} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {resolve, dirname} from 'node:path';
import test from 'node:test';
import {buildSidebar} from '../navigation.mjs';
const root=fileURLToPath(new URL('../../',import.meta.url));

test('sticker tutorial has twelve illustrated steps and shared local photos in both languages', async()=>{
  const expected=['prepare','technique',...Array.from({length:12},(_,i)=>`step-${String(i+1).padStart(2,'0')}`),'finished'];
  const imageSets=[];
  for(const lang of ['zh','en']){
    const path=`wiki/guides/${lang==='en'?'en/':''}guide/stickers.md`;
    const text=await readFile(resolve(root,path),'utf8');
    assert.deepEqual([...text.matchAll(/^## .+\{#([^}]+)\}/gm)].map(m=>m[1]),expected);
    assert.match(text,/5[–-]10/);
    assert.match(text,/转移膜|transfer film/);
    assert.doesNotMatch(text,/feishu\.cn|file_token|revision_id|PR #|Git 历史/);
    const images=[...text.matchAll(/!\[[^\]]+\]\(([^)]+)\)/g)].map(m=>m[1]);
    assert.equal(images.length,19);
    for(const image of images){assert.ok(!image.startsWith('http'));await access(resolve(root,dirname(path),image));}
    imageSets.push(images.map(p=>p.split('/').pop()));
    const device=await readFile(resolve(root,`wiki/guides/${lang==='en'?'en/':''}products/watcherobot/device-guide.md`),'utf8');
    assert.match(device,/guide\/stickers\.md/);
  }
  assert.deepEqual(imageSets[0],imageSets[1]);
});

test('sticker tutorial follows device use in User guides, not hardware manufacturing',()=>{
  for(const lang of ['zh','en']){
    const prefix=lang==='en'?'/en':'';
    const catalog=['/products/watcherobot/device-guide','/guide/stickers','/hardware/'].map(route=>({lang,route:prefix+route,title:route}));
    const groups=buildSidebar(catalog,lang)[0].items;
    const guide=groups.find(g=>g.text===(lang==='zh'?'使用指南':'User guides'));
    assert.deepEqual(guide.items.map(i=>i.link),[prefix+'/products/watcherobot/device-guide',prefix+'/guide/stickers']);
    assert.ok(!groups.find(g=>g.text===(lang==='zh'?'硬件专区':'Hardware')).items.some(i=>i.link.includes('stickers')));
  }
});
