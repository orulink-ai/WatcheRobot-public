"""用 watcherobot app run 启动：拍摄一张照片，保存后结束。"""

import asyncio
from pathlib import Path
from uuid import uuid4

from watcherobot.application import ApplicationContext
from app_support import finish_thread


# 普通拍照为 False；用户明确需要动画和快门反馈时改为 True。
WITH_FEEDBACK = False
OUTPUT_DIR = Path(__file__).resolve().parent / "photos"


def capture_photo(robot, output_dir: Path, *, with_feedback: bool) -> Path:
    """同步设备调用；由 run() 放到工作线程中执行。"""
    method_name = "capture_with_feedback" if with_feedback else "capture"
    capture = getattr(robot.camera, method_name, None)
    if not callable(capture):
        raise RuntimeError(f"当前安装的 SDK 缺少 camera.{method_name}，请先核对安装版本和来源")
    if with_feedback and not robot.supports("camera.capture.feedback.v1"):
        raise RuntimeError("设备未声明 camera.capture.feedback.v1；不会自动改为普通拍照")

    # timeout 单位为秒；拍照失败会抛出异常，不打印或伪造成功结果。
    image = capture(timeout=10)
    output_dir.mkdir(parents=True, exist_ok=True)
    # 每次使用不同的文件名，避免覆盖用户已有照片。
    path = output_dir / f"photo-{uuid4().hex}.jpg"
    image.save(path)
    return path


async def run(app, output_dir: Path = OUTPUT_DIR) -> Path | None:
    """修改这里可替换业务；保持 main() 的受管入口。"""
    if app.shutdown_requested:
        return None
    app.logger.info("开始拍照，模式：%s", "带状态反馈" if WITH_FEEDBACK else "普通")
    path = await finish_thread(
        capture_photo, app.robot, output_dir, with_feedback=WITH_FEEDBACK,
        shutdown_requested=lambda: app.shutdown_requested,
    )
    app.logger.info("照片已保存：%s", path)
    return path


async def main() -> None:
    # Daemon 提供连接上下文；正常退出时释放资源，托管强制停止仍可能截断清理。
    async with ApplicationContext.from_environment() as app:
        await run(app)


if __name__ == "__main__":
    asyncio.run(main())
