"""两个平台入口共用的应用准备/检查核心；不启动 Daemon 或设备。"""
from __future__ import annotations
import argparse
import ast
import importlib
import importlib.metadata
import inspect
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SDK = 'watcherobot'
TEMPLATES = {'photo': ['camera.capture'],
             'input': ['inputs.wait', 'lights.set_color', 'lights.off'],
             'button': ['camera.capture'], 'voice': ['cli.voice']}


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def paths(project):
    scripts = project / '.venv' / ('Scripts' if os.name == 'nt' else 'bin')
    return scripts / ('python.exe' if os.name == 'nt' else 'python'), scripts / (
        'watcherobot.exe' if os.name == 'nt' else 'watcherobot')


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def cli_help(cli, *command):
    """只运行帮助命令，不触发示例或 Daemon；统一两平台编码和等待上限。"""
    if not cli.exists():
        return ''
    try:
        output = subprocess.run([str(cli), *command, '--help'], capture_output=True,
            text=True, encoding='utf-8', errors='replace', timeout=15,
            env={**os.environ, 'PYTHONUTF8': '1'})
    except (OSError, subprocess.TimeoutExpired):
        return ''
    return output.stdout if output.returncode == 0 else ''


def probe():
    """检查实际安装对象和手册的调用签名；缺失能力留在报告中，不假装可用。"""
    import watcherobot
    contract = json.loads((ROOT / 'scripts/api-contract.json').read_text(encoding='utf-8'))
    result = {'python': sys.executable, 'sdk_version': importlib.metadata.version('watcherobot'),
              'sdk_path': watcherobot.__file__, 'methods': {}}
    for name, item in contract.items():
        try:
            cls = getattr(importlib.import_module(item['module']), item['class'])
            method = getattr(cls, item['method'])
            signature = inspect.signature(method)
            for call in item['calls']:
                signature.bind(None, *([None] * call['args']),
                               **{k: None for k in call['keywords']})
            result['methods'][name] = {'available': True, 'signature': str(signature)}
        except (ImportError, AttributeError, TypeError, ValueError) as error:
            result['methods'][name] = {'available': False, 'reason': str(error)}
    cli = Path(sys.executable).parent / ('watcherobot.exe' if os.name == 'nt' else 'watcherobot')
    # Conda/virtualenv 对脚本的布局可能不同，sysconfig 返回实际入口目录。
    if not cli.exists():
        import sysconfig
        cli = Path(sysconfig.get_path('scripts')) / cli.name
    init = cli_help(cli, 'app', 'init')
    config = cli_help(cli, 'app', 'configure')
    voice = '--template' in init and 'voice' in init and bool(config)
    result['methods']['cli.voice'] = {'available': voice,
        'reason': '需要 app init --template voice 和 app configure 同时存在'}
    from watcherobot.robot import WatcheRobot
    evidence = inspect.getattr_static(WatcheRobot, 'resource_evidence', None)
    result['methods']['robot.resource_evidence'] = {
        'available': isinstance(evidence, property), 'kind': 'property',
        'reason': '只检查公共属性存在；实时性、设备身份与连接代次在读取后验证'}
    return result


def prepare(project, template, sdk, python=None):
    import re
    if sdk != DEFAULT_SDK and not re.fullmatch(r'watcherobot==[0-9A-Za-z.+-]+', sdk):
        source = Path(sdk)
        if '://' in sdk or not source.exists():
            raise ValueError('--sdk 仅接受 watcherobot、watcherobot==用户指定版本或已存在的本地 wheel/源码路径')
        sdk = str(source.resolve())
    project = project.resolve()
    # 原子创建；已有目录一律不覆盖，包括上次失败留下的环境。
    project.mkdir(parents=True, exist_ok=False)
    write_json(project / 'skill-setup.json', {'state': 'preparing', 'template': template, 'sdk': sdk})
    try:
        run([python or sys.executable, '-c',
             'import sys; assert (3, 10) <= sys.version_info[:2] <= (3, 12), "项目需要 Python 3.10-3.12"'])
        run([python or sys.executable, '-m', 'venv', project / '.venv'])
        py, cli = paths(project)
        # 不固定 SDK 版本；正式索引选择当前 Python 可安装的最新稳定包。
        run([py, '-m', 'pip', '--isolated', 'install', '--index-url',
             'https://pypi.org/simple', '--upgrade', sdk])
        run([py, '-m', 'pip', 'check'])
        output = run([py, __file__, 'probe'], capture_output=True, text=True, encoding='utf-8')
        report = json.loads(output.stdout)
        write_json(project / 'sdk-capabilities.json', report)
        required = TEMPLATES[template]
        missing = [m for m in required if not report['methods'].get(m, {}).get('available')]
        if missing:
            raise RuntimeError('安装包不支持模板：' + ', '.join(missing))
        with tempfile.TemporaryDirectory(prefix='watche-scaffold-') as temporary:
            scaffold = Path(temporary) / 'app'
            args = [cli, 'app', 'init', scaffold]
            if template == 'voice':
                args += ['--template', 'voice']
            try:
                run(args, capture_output=True, text=True, encoding='utf-8')
            except subprocess.CalledProcessError as error:
                print(error.stdout or '', file=sys.stderr)
                print(error.stderr or '', file=sys.stderr)
                raise
            shutil.copytree(scaffold, project, dirs_exist_ok=True)
        # app init 在临时目录创建，改为最终项目身份，避免所有应用叫 local.app。
        manifest_file = project / 'app.json'
        manifest = json.loads(manifest_file.read_text(encoding='utf-8-sig'))
        import re
        slug = re.sub('[^a-z0-9_]', '_', project.name.lower()).strip('_') or 'robot_app'
        manifest['id'] = 'local.' + slug
        manifest['name'] = project.name
        manifest['description'] = {
            'photo': '启动后拍摄一张照片并保存。', 'input': '持续监听背部触摸并点亮侧灯。',
            'button': '通过本机网页按钮拍照并显示照片。', 'voice': '机器人语音应用。',
        }[template]
        # 保留官方生成器的兼容声明；实际安装版本记录在能力报告中，不额外锁定。
        write_json(manifest_file, manifest)
        if template != 'voice':
            files = ['app_support.py', template + '_app.py']
            if template == 'button':
                files.append('photo_app.py')
            for name in files:
                destination = 'app.py' if name == template + '_app.py' else name
                shutil.copyfile(ROOT / 'assets/templates' / name, project / destination)
            usage = {'photo': '启动后自动拍照，保存到 photos 目录，然后退出。',
                     'input': '按下机器人背部触摸传感器，侧灯亮一秒；应用持续监听。',
                     'button': '打开运行日志中的本机 URL，点击拍照；照片保存在 photos 目录。'}[template]
            (project / 'README.md').write_text(
                '# ' + project.name + '\n\n' + usage + '\n\n'
                '在本目录运行（需要目标机器人已连接）：\n\n'
                'Windows PowerShell：\n```powershell\n& ./.venv/Scripts/watcherobot.exe app run .\n```\n\n'
                'macOS/Linux：\n```sh\n./.venv/bin/watcherobot app run .\n```\n\n'
                '不要直接执行 python app.py。用 Ctrl+C 停止本次托管运行。\n'
                '本地检查通过不代表实机效果通过。\n', encoding='utf-8')
        if manifest.get('dependencies'):
            run([py, '-m', 'pip', 'install', *manifest['dependencies']])
        write_json(project / 'sdk-requirements.json', required)
        # 项目检查文件含本地路径，不提交/发布；保留 requirements 作为应用合同。
        for name in ('.gitignore', '.watcherignore'):
            with (project / name).open('a', encoding='utf-8') as output:
                output.write('\n.venv/\n__pycache__/\nphotos/\nskill-setup.json\nsdk-capabilities.json\n')
        run([py, __file__, 'check', '--project', project])
        write_json(project / 'skill-setup.json', {'state': 'ready', 'template': template,
            'sdk': sdk, 'python': str(py), 'cli': str(cli)})
        print('准备完成：' + str(project))
    except BaseException:
        write_json(project / 'skill-setup.json', {'state': 'failed', 'template': template,
            'note': '保留失败现场；不要覆盖此目录。修复原因后换新目录准备。'})
        raise


def check(project, extra, python=None):
    project = project.resolve()
    default_py, default_cli = paths(project)
    py = Path(python).resolve() if python else default_py
    if not py.exists():
        raise FileNotFoundError('缺少项目解释器；已有项目使用 --python 指定原环境')
    if Path(sys.executable).resolve() != py.resolve():
        args = [py, __file__, 'check', '--project', project, '--python', py]
        for name in extra:
            args += ['--require', name]
        run(args)
        return
    import sysconfig
    cli = Path(sysconfig.get_path('scripts')) / default_cli.name
    # 不导入/执行 app.py，也不遍历环境、媒体和第三方目录。
    excluded = {'.venv', 'venv', '.git', 'node_modules', '__pycache__', 'photos'}
    for directory, subdirs, files in os.walk(project):
        subdirs[:] = [p for p in subdirs if p not in excluded and not p.startswith('.')]
        for name in files:
            if name.endswith('.py'):
                path = Path(directory) / name
                ast.parse(path.read_text(encoding='utf-8-sig'), filename=str(path))
    run([py, '-m', 'pip', 'check'])
    run([cli, 'app', 'check', project])
    report = probe()
    write_json(project / 'sdk-capabilities.json', report)
    requirement_file = project / 'sdk-requirements.json'
    if not requirement_file.exists():
        raise FileNotFoundError('缺少 sdk-requirements.json；先按能力列表填写本应用使用的接口')
    required = json.loads(requirement_file.read_text(encoding='utf-8'))
    if not isinstance(required, list) or not all(isinstance(x, str) for x in required):
        raise ValueError('sdk-requirements.json 必须是接口名字符串数组')
    unknown = [m for m in required + extra if m not in report['methods']]
    if unknown:
        raise ValueError('Skill 未登记接口合同：' + ', '.join(unknown)
                         + '；这不代表 SDK 缺少接口。核对源码并补充 api-contract.json 后重查')
    missing = [m for m in required + extra if not report['methods'][m]['available']]
    if missing:
        raise RuntimeError('缺少或签名不兼容：' + ', '.join(missing) + '；见 sdk-capabilities.json')
    print('本地检查通过；未启动 Daemon，未验证设备。')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    create = sub.add_parser('prepare')
    create.add_argument('--project', type=Path, required=True)
    create.add_argument('--template', choices=TEMPLATES, default='photo')
    create.add_argument('--sdk', default=DEFAULT_SDK, help='默认从正式 PyPI 安装最新兼容稳定版 watcherobot')
    create.add_argument('--python')
    verify = sub.add_parser('check')
    verify.add_argument('--project', type=Path, required=True)
    verify.add_argument('--python')
    verify.add_argument('--require', action='append', default=[])
    sub.add_parser('probe')
    args = parser.parse_args()
    try:
        if args.command == 'prepare':
            prepare(args.project, args.template, args.sdk, args.python)
        elif args.command == 'check':
            check(args.project, args.require, args.python)
        else:
            print(json.dumps(probe(), ensure_ascii=False, indent=2))
        return 0
    except (Exception, KeyboardInterrupt) as error:
        print('失败：' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
    raise SystemExit(main())
