# 本地小服务器：把灰盒（three.js 原型）那个文件夹当网页开出来，另外收网页 POST 回来的 JSON，存到 golden/ 里。
# 用来从灰盒导出“标准答案”（常数、机关位置、光路取样），UE 这边照着核对。
# 用法：python serve.py <灰盒文件夹> [端口=8765]     网页里：fetch('/save?name=xxx.json', {method:'POST', body: JSON.stringify(data)})
#
# 另外有一个 /inv.html：同一个灰盒，但在送出去之前改了几行（不动原文件），让它把每一块“看不见的碰撞体”记下来
# （灰盒里很多挡人的墙没有模型：瀑布边的挡墙、栏杆上方的高护栏、台地和小岛边缘的护栏……）。
# 记在 window.__inv 里：每项有 flags（w 能踩 / s 挡人 / l 挡光 / c 挡镜头）、是怎么生成的（扇形 / 方块 / 棱柱 / 圆柱和它的参数）、
# 是源文件哪一行建的。改动都在原来的行内，行号和原文件一致。
import http.server, os, re, sys, urllib.parse
ROOT = os.path.abspath(sys.argv[1]); PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "golden")

def patched_index():
    s = open(os.path.join(ROOT, "index.html"), encoding="utf-8").read()
    def rep(a, b):
        nonlocal s
        assert s.count(a) == 1, (a, s.count(a))
        s = s.replace(a, b)
    # 生成几何的四个函数：把参数挂在几何上
    rep("function sector(r0, r1, y0, y1, a0, a1, b0 = a0, b1 = a1, o = {}) {",
        "function sector(...A) { const g = sector0(...A); g.userData.gen = { t: 'sector', r0: A[0], r1: A[1], y0: A[2], y1: A[3], a0: A[4], a1: A[5], b0: A[6] ?? A[4], b1: A[7] ?? A[5] }; return g; } function sector0(r0, r1, y0, y1, a0, a1, b0 = a0, b1 = a1, o = {}) {")
    rep("function planPrism(poly, y0, y1, o = {}) {",
        "function planPrism(...A) { const g = planPrism0(...A); g.userData.gen = { t: 'prism', poly: A[0], y0: A[1], y1: A[2] }; return g; } function planPrism0(poly, y0, y1, o = {}) {")
    rep("function boxGeo(c, w, h, d, yaw = 0) {",
        "function boxGeo(...A) { const g = boxGeo0(...A); g.userData.gen = { t: 'box', c: [A[0].x, A[0].y, A[0].z], w: A[1], h: A[2], d: A[3], yaw: A[4] ?? 0 }; return g; } function boxGeo0(c, w, h, d, yaw = 0) {")
    rep("function cylGeo(c, r0, r1, h, seg = 20) { const g = nonIndexed(new THREE.CylinderGeometry(r1, r0, h, seg)); g.translate(c.x, c.y + h / 2, c.z); return g; }",
        "function cylGeo(c, r0, r1, h, seg = 20) { const g = nonIndexed(new THREE.CylinderGeometry(r1, r0, h, seg)); g.translate(c.x, c.y + h / 2, c.z); g.userData.gen = { t: 'cyl', c: [c.x, c.y, c.z], r0, r1, h }; return g; }")
    # 记下看不见的碰撞体：直接调 proxy() 的，和 addStatic 时材质是空的
    rep("  const m = new THREE.Mesh(geo, COLLIDER); m.updateMatrixWorld(true);",
        "  const m = new THREE.Mesh(geo, COLLIDER); m.updateMatrixWorld(true); if (!window.__inAddStaticVisible) (window.__inv = window.__inv || []).push({ m, flags, gen: geo.userData.gen || null, lines: (new Error().stack.match(/inv\\.html:(\\d+)/g) || []).map((x) => +x.split(':')[1]) });")
    rep("  return flags ? proxy(geo, flags, data) : null;",
        "  window.__inAddStaticVisible = !!mat; const __r = flags ? proxy(geo, flags, data) : null; window.__inAddStaticVisible = false; return __r;")
    return s.encode("utf-8")

class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=ROOT, **k)
    def do_GET(self):
        if urllib.parse.urlparse(self.path).path == "/inv.html":
            body = patched_index()
            self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8"); self.send_header("Content-Length", str(len(body))); self.end_headers()
            self.wfile.write(body); return
        super().do_GET()
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
