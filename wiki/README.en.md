# Wiki maintenance and automatic updates

The public Wiki is <https://wiki.orulink.ai/>. [中文说明](README.md).

Edit the original root README files, `docs/`, hardware/firmware entry points and skills in this repository. `wiki/guides/` contains the supplementary client and Application guides migrated from WatcheRobot-Docs. The build generates bilingual routes, local images/PDFs and search in ignored output directories. Do not edit generated Markdown.

Use Node.js 24 and Python 3.12+. On all platforms:

```text
npm ci
npm run check
python -m unittest discover -s wiki/tests -p "test_*.py"
npm run docs:dev
```

Windows skips four atomic symlink tests; deployment on Windows requires symlink privileges. Run the full suite on Linux before deploying. Rebuild after editing canonical Markdown.

The Wiki workflow validates PRs and main pushes without publishing. A published stable Release builds the Markdown at its tag and attaches `watcherobot-wiki.tar.gz` and its `.sha256` checksum to that existing Release. Manual workflow dispatch accepts an existing stable tag to backfill historical releases. No product tag is created or rewritten. Current checkout supplies the build tooling, while `WIKI_SOURCE_DIR` and `WIKI_SOURCE_REF` select canonical source and revision. Historical tags without supplementary guides omit them.

The AWS systemd timer checks GitHub's Latest stable Release every five minutes. Drafts, prereleases and missing/incomplete assets leave the current Wiki intact. Releases not selected as Latest do not activate. SHA256, archive traversal/type/size checks and embedded revision validation precede an atomic symlink switch. A failed HTTPS health check restores the previous symlink. Network errors fail that run and retry next cycle. Replacing a Release asset updates its digest; prefer new Releases for normal changes. Immutable releases need all assets prepared before locking.

Install `update.py` at `/opt/watcherobot-wiki/update.py` and the supplied service/timer under `/etc/systemd/system/`. The service runs as `ubuntu`, with write access limited to `/www/wwwroot/wiki.orulink.ai/`; Nginx configuration and certificates remain administrator-owned. No GitHub credential or Node.js build runtime is stored on AWS. Check `systemctl status watcherobot-wiki.service` and `journalctl -u watcherobot-wiki.service` for failures.

Both OS entry points have identical arguments and exit codes. The npm dispatcher selects PowerShell or Bash:

```text
npm run wiki:update -- --root /path/to/wiki --health-url https://wiki.orulink.ai/
```

Direct entry points are `./wiki/update.ps1` and `bash wiki/update.sh`; use `--help` for arguments. Linux systemd calls the Python core directly.

Release directories remain available for rollback. Stop the timer, atomically repoint `current` to the previous directory and verify HTTPS. Change GitHub's Latest selection before restarting the timer to avoid automatically reactivating the unwanted release. Do not modify active release files.
