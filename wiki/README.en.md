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

## Independent publication

Every main push tests and builds the exact commit, then publishes to the dedicated [wiki-site channel](https://github.com/orulink-ai/WatcheRobot-public/releases/tag/wiki-site). PRs only validate; select main for manual workflow dispatch. The channel is a prerelease explicitly excluded from Latest. It does not create firmware versions or change firmware assets.

Each archive has a unique name containing its source SHA, workflow run ID and attempt. Only after upload succeeds does the workflow update the release body's JSON publication pointer containing repository, source SHA, asset name and SHA256. Publishing is serialized, with main-head checks before and after upload to prevent old reruns from replacing a newer build. The channel requires mutable releases; do not manually replace its JSON body or delete the referenced asset.

AWS checks only `wiki-site` every five minutes, independently of firmware Latest. Missing or incomplete publications leave the current site intact. SHA256, archive traversal/type/size checks and embedded revision validation precede an atomic symlink switch. A failed HTTPS health check restores the previous symlink. Network errors fail that run and retry next cycle.

`/version.json` records the actual source commit, not the channel tag. The tag is a fixed channel entry point and is not moved to track source revisions. Firmware Release events no longer deploy documentation, and historical Wiki assets on firmware releases are not used by the updater. For local revision checks only, `WIKI_SOURCE_DIR` and `WIKI_SOURCE_REF` still select canonical source and revision without publishing.

## Server migration and operations

Install `update.py` at `/opt/watcherobot-wiki/update.py` and the supplied service/timer under `/etc/systemd/system/`. The service runs as `ubuntu`, with write access limited to `/www/wwwroot/wiki.orulink.ai/`; Nginx configuration and certificates remain administrator-owned. No GitHub credential or Node.js build runtime is stored on AWS. Check `systemctl status watcherobot-wiki.service` and `journalctl -u watcherobot-wiki.service` for failures.

When migrating from firmware Release updates, stop the timer and wait for any active service to finish before replacing the server's `update.py`. Run the Linux tests, start the service once, verify live `version.json`, both language homepages and deep routes, then enable the timer again. Merging this repository does not update the installed server script; without this step, the old policy remains active.

Both OS entry points have identical arguments and exit codes. The npm dispatcher selects PowerShell or Bash:

```text
npm run wiki:update -- --root /path/to/wiki --health-url https://wiki.orulink.ai/
```

Direct entry points are `./wiki/update.ps1` and `bash wiki/update.sh`; use `--help` for arguments. Linux systemd calls the Python core directly.

Release directories remain available for rollback. Stop the timer, atomically repoint `current` to the previous directory and verify HTTPS. Prefer a fix or revert on main that publishes a new package before restarting the timer. For emergency rollback, restore the `wiki-site` JSON pointer to a previously verified package. Do not change firmware Latest or modify active release files. Restarting the timer while it still points to the faulty package will redeploy it.
