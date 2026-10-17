const groups = [
  ['使用指南', 'User guides', ['/', '/guide/getting-started', '/products/watcherobot/device-guide', '/guide/client-and-device', '/docs/flashing', '/docs/sd-card-assets', '/docs/action-test', '/guide/firmware-version-management']],
  ['开发者文档', 'Development', ['/docs/sdk', '/sdk/', '/products/watcherobot/capabilities', '/products/watcherobot/voice', '/docs/embedded-sdk', '/firmware/sdk/', '/guide/hardware-in-the-loop']],
  ['硬件与资源', 'Hardware and resources', ['/guide/reproduction', '/docs/pcb-fabrication', '/hardware/', '/hardware/pcb/spares', '/firmware/', '/docs/versions', '/docs/compatibility']],
  ['支持与贡献', 'Support and contribution', ['/products/watcherobot/community', '/troubleshooting/', '/CONTRIBUTING', '/SECURITY', '/docs/governance', '/docs/release-process', '/skills/watche-release-flash/', '/skills/watche-sd-card-write/']],
];

// Older release tags may not contain the new guides. Do not publish dead navigation.
export function buildSidebar(catalog, lang) {
  const pages = new Map(catalog.filter(page => page.lang === lang).map(page => [page.route, page]));
  const items = groups.map(([zh, en, routes]) => ({
    text: lang === 'zh' ? zh : en,
    collapsed: false,
    items: routes.map(route => pages.get(lang === 'zh' ? route : '/en' + route))
      .filter(Boolean).map(page => ({text:page.title, link:page.route})),
  })).filter(group => group.items.length);
  return items.length ? [{text:'WatcheRobot', items}] : [];
}
