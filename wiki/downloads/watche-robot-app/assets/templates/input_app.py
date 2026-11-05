"""持续监听背部触摸：按下后侧灯亮一秒；超时继续监听。"""
import asyncio
import threading
from watcherobot.application import ApplicationContext
from app_support import device_session, finish_thread


def listen(app, stop):
    robot = app.robot
    while not stop.is_set() and not app.shutdown_requested:
        try:
            event = robot.inputs.wait(timeout=0.5)  # 秒；定期检查退出请求。
        except TimeoutError:
            continue
        if stop.is_set() or app.shutdown_requested:
            break
        if event.source != 'back_touch' or event.action != 'press':
            continue  # 不把 release/long_press 再算一次点击。
        with device_session(
            lambda: robot.lights.set_color('#4DA3FF', brightness=0.5, zone='side'),
            robot.lights.off,
        ):
            # 最多显示一秒；每 0.1 秒检查一次停止，避免退出时长时间等待。
            for _ in range(10):
                if stop.wait(0.1) or app.shutdown_requested:
                    break
        app.logger.info('触摸事件已处理；灯光效果需实机观察')


async def main():
    async with ApplicationContext.from_environment() as app:
        stop = threading.Event()
        await finish_thread(
            listen, app, stop, cancel_event=stop,
            shutdown_requested=lambda: app.shutdown_requested,
        )


if __name__ == '__main__':
    asyncio.run(main())
