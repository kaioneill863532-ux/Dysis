# 本地小服务器：把灰盒（three.js 原型）那个文件夹当网页开出来，另外收网页 POST 回来的 JSON，存到 golden/ 里。
# 用来从灰盒导出“标准答案”（常数、机关位置、光路取样），UE 这边照着核对。
# 用法：python serve.py <灰盒文件夹> [端口=8765]     网页里：fetch('/save?name=xxx.json', {method:'POST', body: JSON.stringify(data)})
import http.server, os, re, sys, urllib.parse
ROOT = os.path.abspath(sys.argv[1]); PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "golden")
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=ROOT, **k)
    def do_POST(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query)
        name = (q.get("name") or [""])[0]
        if u.path != "/save" or not re.fullmatch(r"[A-Za-z0-9_\-]+\.json", name):
            self.send_error(400, "bad request"); return
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, name), "wb") as f: f.write(body)
        self.send_response(200); self.send_header("Content-Type", "text/plain"); self.end_headers()
        self.wfile.write(("saved %s %d" % (name, len(body))).encode())
    def end_headers(self):
        self.send_header("Cache-Control", "no-store"); super().end_headers()
    def log_message(self, *a): pass
http.server.ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
