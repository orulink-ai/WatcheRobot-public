import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const windows = process.platform === 'win32';
const entry = fileURLToPath(new URL(windows ? './update.ps1' : './update.sh', import.meta.url));
const result = spawnSync(windows ? 'powershell.exe' : 'bash', windows
  ? ['-NoProfile', '-File', entry, ...process.argv.slice(2)]
  : [entry, ...process.argv.slice(2)], {stdio:'inherit'});
if(result.error) console.error(result.error.message);
process.exit(result.status ?? 1);
