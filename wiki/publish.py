"""Publish a tested main build without changing the latest product Release."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile

from update import REPOSITORY, WIKI_TAG


def gh(*args):
    return subprocess.check_output(['gh', *args], text=True).strip()


def publication(archive, source_ref, build_id):
    if not re.fullmatch(r'[a-f0-9]{40}', source_ref):
        raise ValueError('Expected a full commit SHA')
    if not re.fullmatch(r'[1-9][0-9]*-[1-9][0-9]*', build_id):
        raise ValueError('Expected workflow run id and attempt')
    with tarfile.open(archive, 'r:gz') as bundle:
        manifest = json.load(bundle.extractfile('./version.json'))
    if manifest != {'repository': REPOSITORY, 'sourceRef': source_ref}:
        raise ValueError('Build identity mismatch')
    return {
        'repository': REPOSITORY,
        'sourceRef': source_ref,
        'archive': f'watcherobot-wiki-{source_ref}-{build_id}.tar.gz',
        'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
    }


def main():
    if os.environ.get('GITHUB_REF') != 'refs/heads/main' or os.environ.get('GH_REPO') != REPOSITORY:
        raise ValueError('Only the canonical main branch may publish')
    source_ref = os.environ['GITHUB_SHA']
    build_id = os.environ['GITHUB_RUN_ID'] + '-' + os.environ['GITHUB_RUN_ATTEMPT']
    archive = Path('watcherobot-wiki.tar.gz')
    manifest = publication(archive, source_ref, build_id)
    # A rerun of an older workflow must not roll the website back.
    if gh('api', f'repos/{REPOSITORY}/commits/main', '--jq', '.sha') != source_ref:
        print('Skipping superseded main build')
        return
    releases = json.loads(gh('api', '--paginate', '--slurp', f'repos/{REPOSITORY}/releases?per_page=100'))
    existing = next((r for page in releases for r in page if r['tag_name'] == WIKI_TAG), None)
    if existing is None:
        gh('release', 'create', WIKI_TAG, '--target', source_ref, '--prerelease', '--latest=false',
           '--title', 'Wiki website (continuous)', '--notes', 'Documentation publication pending.')
    elif existing.get('draft') or not existing.get('prerelease') or existing.get('immutable'):
        raise ValueError('Wiki channel must be a mutable, published prerelease')
    with tempfile.TemporaryDirectory(prefix='wiki-publish-') as directory:
        asset = Path(directory) / manifest['archive']
        shutil.copyfile(archive, asset)
        notes = Path(directory) / 'publication.json'
        notes.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        # Unique asset first; the release body is the atomic publication pointer.
        gh('release', 'upload', WIKI_TAG, str(asset))
        if gh('api', f'repos/{REPOSITORY}/commits/main', '--jq', '.sha') != source_ref:
            print('Main advanced during upload; keeping the previous publication')
            return
        gh('release', 'edit', WIKI_TAG, '--prerelease', '--latest=false', '--notes-file', str(notes))
    print(f'Published Wiki source {source_ref}; product Latest unchanged')


if __name__ == '__main__':
    main()
