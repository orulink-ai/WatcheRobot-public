"""应用内部共用：设备会话清理、工作线程退出。不是独立启动入口。"""
import asyncio
import logging
import sys
from contextlib import contextmanager


@contextmanager
def device_session(start, stop):
    """仅用于本应用独占调度的资源；明确拒绝启动时不停止其他所有者。"""
    needs_stop = False
    try:
        try:
            result = start()
        except TimeoutError:
            # 应答丢失时设备可能已经启动，必须尝试清理。
            needs_stop = True
            raise
        else:
            needs_stop = True
        yield result
    finally:
        if needs_stop:
            failed = sys.exc_info()[0] is not None
            try:
                stop()
            except Exception:
                if not failed:
                    raise
                # 保留原始异常，同时明确记录清理失败，不能宣称设备已经停止。
                logging.exception("设备清理失败，请核对资源状态")


async def finish_thread(func, *args, cancel_event=None, shutdown_requested=None, **kwargs):
    """协作退出并尽力等待线程；外部 Runtime 的强制停止可能早于线程完成。"""
    worker = asyncio.create_task(asyncio.to_thread(func, *args, **kwargs))
    try:
        notified = False
        while not worker.done():
            if shutdown_requested is not None and shutdown_requested() and not notified:
                notified = True
                if cancel_event is not None:
                    cancel_event.set()
                logging.warning("收到托管停止请求；正在等待设备任务退出，清理尚未确认，Runtime 可能提前终止进程")
            await asyncio.wait({worker}, timeout=0.1)
        return worker.result()
    except asyncio.CancelledError:
        if cancel_event is not None:
            cancel_event.set()
        # 协程取消时继续尽力等待；无法阻止 Runtime 到期关闭通道或终止进程。
        while not worker.done():
            try:
                await asyncio.shield(worker)
            except asyncio.CancelledError:
                continue
            except Exception:
                break
        if not worker.cancelled() and worker.exception() is not None:
            logging.error("退出期间设备操作失败：%s", worker.exception())
        raise
