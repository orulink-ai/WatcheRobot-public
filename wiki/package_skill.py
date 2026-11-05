"""Build a deterministic downloadable Skill ZIP from canonical source files."""
import sys
import zipfile
from pathlib import Path


def package(source: Path, destination: Path):
    source = source.resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source.rglob('*')):
            if path.is_symlink():
                raise ValueError('Skill source must not contain symbolic links')
            if not path.is_file():
                continue
            info = zipfile.ZipInfo((Path(source.name) / path.relative_to(source)).as_posix(), (2020, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())


if __name__ == '__main__':
    package(Path(sys.argv[1]), Path(sys.argv[2]))
