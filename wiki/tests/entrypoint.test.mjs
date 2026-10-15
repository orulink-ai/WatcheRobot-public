import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

const dispatcher = fileURLToPath(new URL('../dispatch.mjs', import.meta.url));
test('OS dispatcher forwards updater arguments and preserves usage exit codes', () => {
  const help = spawnSync(process.execPath, [dispatcher, '--help'], {encoding:'utf8'});
  assert.equal(help.status, 0, help.stderr);
  assert.match(help.stdout, /--root ROOT/);
  const invalid = spawnSync(process.execPath, [dispatcher, '--not-a-valid-option'], {encoding:'utf8'});
  assert.equal(invalid.status, 2, invalid.stderr);
});
