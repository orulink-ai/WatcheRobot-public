"""本机网页拍照按钮；入口仍由 watcherobot app run 托管。"""
import asyncio
import base64
import json
import secrets
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from watcherobot.application import ApplicationContext
from app_support import finish_thread
from photo_app import capture_photo

WITH_FEEDBACK = False  # True 需 SDK 方法与 camera.capture.feedback.v1 同时支持。
OUTPUT_DIR = Path(__file__).resolve().parent / 'photos'


def make_server(app, stop):
    token = secrets.token_urlsafe(24)
    camera_lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        def setup(self):
            super().setup()
            self.connection.settimeout(2)  # 限制不完整的本机 HTTP 请求等待时间。

        def log_message(self, *args):
            pass  # 不记录请求或用户照片内容。

        def reply(self, status, body, content_type='application/json; charset=utf-8'):
            raw = body.encode('utf-8')
            self.send_response(status)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(raw)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.end_headers()
            self.wfile.write(raw)

        def do_GET(self):
            if self.path != '/':
                self.reply(404, '{}')
                return
            html = '''<!doctype html><meta charset="utf-8"><title>机器人拍照</title>
<h1>机器人拍照</h1><button id="capture">拍照</button><p id="status">准备就绪</p>
<img id="photo" style="max-width:90vw" alt="拍照成功后在这里显示">
<script>
const button=document.querySelector('#capture'), status=document.querySelector('#status');
button.onclick=async()=>{button.disabled=true;status.textContent='拍照中…';try{
const response=await fetch('/capture',{method:'POST',headers:{'X-App-Token':TOKEN}});
const data=await response.json();if(!response.ok)throw Error(data.error);
document.querySelector('#photo').src='data:image/jpeg;base64,'+data.jpeg;
status.textContent='已保存：'+data.name;
}catch(error){status.textContent='失败：'+error.message;}finally{button.disabled=false;}};
</script>'''.replace('TOKEN', json.dumps(token))
            self.reply(200, html, 'text/html; charset=utf-8')

        def do_POST(self):
            if self.path != '/capture':
                self.reply(404, '{}')
                return
            if self.headers.get('X-App-Token') != token:
                self.reply(403, json.dumps({'error': '请求不属于本应用页面'}, ensure_ascii=False))
                return
            if stop.is_set() or app.shutdown_requested:
                self.reply(503, json.dumps({'error': '应用正在退出'}, ensure_ascii=False))
                return
            if not camera_lock.acquire(blocking=False):
                self.reply(409, json.dumps({'error': '正在拍照，请稍后重试'}, ensure_ascii=False))
                return
            try:
                path = capture_photo(app.robot, OUTPUT_DIR, with_feedback=WITH_FEEDBACK)
                self.reply(200, json.dumps({'name': path.name,
                    'jpeg': base64.b64encode(path.read_bytes()).decode('ascii')}))
            except Exception as error:
                app.logger.error('拍照失败：%s', error)
                self.reply(500, json.dumps({'error': str(error)}, ensure_ascii=False))
            finally:
                camera_lock.release()

    # 只监听本机；由操作系统分配空闲端口，不与用户服务抢固定端口。
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = False  # server_close 必须等待拍照线程完成。
    server.timeout = 0.2
    return server


def serve(app, stop):
    server = make_server(app, stop)
    app.logger.info('打开本机页面：http://127.0.0.1:%s/', server.server_port)
    try:
        while not stop.is_set() and not app.shutdown_requested:
            server.handle_request()
    finally:
        stop.set()
        server.server_close()


async def main():
    async with ApplicationContext.from_environment() as app:
        stop = threading.Event()
        await finish_thread(
            serve, app, stop, cancel_event=stop,
            shutdown_requested=lambda: app.shutdown_requested,
        )


if __name__ == '__main__':
    asyncio.run(main())
