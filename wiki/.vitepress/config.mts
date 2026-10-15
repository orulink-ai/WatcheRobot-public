import { defineConfig } from 'vitepress';
import { readFileSync } from 'node:fs';
import { buildSidebar } from '../navigation.mjs';

const catalog = JSON.parse(readFileSync(new URL('../.generated/catalog.json', import.meta.url), 'utf8'));
export default defineConfig({
  srcDir:'.generated',
  title:'WatcheRobot Docs',
  description:'WatcheRobot 官方使用与开发文档',
  base:'/',
  cleanUrls:true,
  sitemap:{hostname:'https://wiki.orulink.ai'},
  head:[['meta', {name:'theme-color', content:'#101c2c'}]],
  locales:{
    root:{label:'简体中文', lang:'zh-CN', title:'WatcheRobot 文档', themeConfig:{
      nav:[{text:'快速开始', link:catalog.some(page => page.route === '/guide/getting-started') ? '/guide/getting-started' : '/'}, {text:'烧录与使用',link:'/docs/flashing'}, {text:'Python SDK',link:'/docs/sdk'}, {text:'硬件资料',link:'/hardware/'}],
      sidebar:buildSidebar(catalog, 'zh'), outline:{level:[2,3],label:'本页内容'},
      footer:{message:'WatcheRobot 官方文档',copyright:'Copyright © 2026 Orulink AI'},
    }},
    en:{label:'English', lang:'en-US', link:'/en/', themeConfig:{
      nav:[{text:'Get started',link:catalog.some(page => page.route === '/en/guide/getting-started') ? '/en/guide/getting-started' : '/en/'}, {text:'Flashing',link:'/en/docs/flashing'}, {text:'Python SDK',link:'/en/docs/sdk'}, {text:'Hardware',link:'/en/hardware/'}],
      sidebar:buildSidebar(catalog, 'en'),
      footer:{message:'WatcheRobot official documentation',copyright:'Copyright © 2026 Orulink AI'},
    }},
  },
  themeConfig:{
    siteTitle:'WatcheRobot',
    search:{provider:'local'},
    socialLinks:[{icon:'github',link:'https://github.com/orulink-ai/WatcheRobot-public'}],
    outline:{level:[2,3]},
    // The revision is also exposed as machine-readable /version.json.
    lastUpdated:false,
  },
});
