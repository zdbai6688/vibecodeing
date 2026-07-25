import http.server
import os
import sys
import json
import cgi
import time
from http.server import HTTPServer, SimpleHTTPRequestHandler

class UploadHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        return SimpleHTTPRequestHandler.do_GET(self)
    
    def do_POST(self):
        ctype, pdict = cgi.parse_header(self.headers['content-type'])
        if ctype == 'multipart/form-data':
            pdict['boundary'] = bytes(pdict['boundary'], 'utf-8')
            pdict['CONTENT-LENGTH'] = int(self.headers['content-length'])
            fields = cgi.parse_multipart(self.rfile, pdict)
            saved = []
            for field in fields:
                if field == 'file':
                    for data in fields[field]:
                        ts = str(int(time.time()))
                        fname = 'uploaded_' + ts
                        with open(os.path.join(self.directory or '.', fname), 'wb') as f:
                            f.write(data)
                        saved.append(fname)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'success': True, 'files': saved}).encode())
        else:
            length = int(self.headers.get('content-length', 0))
            body = self.rfile.read(length)
            ts = str(int(time.time()))
            fname = 'upload_' + ts
            with open(os.path.join(self.directory or '.', fname), 'wb') as f:
                f.write(body)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'success': True, 'files': [fname]}).encode())
    
    def list_directory(self, path):
        files = os.listdir(path)
        items = []
        for f in sorted(files):
            fp = os.path.join(path, f)
            try:
                size = os.path.getsize(fp)
                items.append({'name': f, 'size': size, 'is_dir': os.path.isdir(fp)})
            except:
                pass
        html = '<html><head><meta charset="utf-8"><title>UOS 局域网文件共享</title>'
        html += '<style>'
        html += 'body{font-family:sans-serif;max-width:800px;margin:20px auto;padding:0 16px;background:#f5f5f5}'
        html += 'h1{color:#4A6CF7;border-bottom:2px solid #4A6CF7;padding-bottom:10px}'
        html += '.file{padding:10px 8px;border-bottom:1px solid #eee;display:flex;justify-content:space-between;align-items:center}'
        html += '.file:hover{background:#f0f0ff}.file a{color:#4A6CF7;text-decoration:none;font-weight:500}'
        html += '.size{color:#888;font-size:13px}'
        html += '.upload-box{border:2px dashed #4A6CF7;padding:24px;text-align:center;margin:20px 0;border-radius:12px;background:#fff}'
        html += '.upload-box input[type=file]{margin:10px 0}'
        html += '.upload-box button{padding:10px 32px;background:#4A6CF7;color:#fff;border:none;border-radius:6px;cursor:pointer;font-size:14px}'
        html += '.upload-box button:hover{background:#3b5de7}'
        html += '.header-info{color:#666;font-size:13px;margin:10px 0}'
        html += '</style></head><body>'
        html += '<h1>📁 UOS 局域网文件共享</h1>'
        html += '<div class="header-info">当前目录: ' + self.directory + '</div>'
        html += '<div class="upload-box">'
        html += '<form action="/" method="post" enctype="multipart/form-data">'
        html += '<div style="font-size:14px;color:#333;margin-bottom:12px"><strong>📤 上传文件到本机</strong></div>'
        html += '<input type="file" name="file" multiple><br><br>'
        html += '<button type="submit">上传文件</button>'
        html += '</form></div>'
        html += '<h2>📄 文件列表 (' + str(len(items)) + ' 个文件)</h2>'
        for item in items:
            icon = '📁' if item['is_dir'] else '📄'
            size_str = str(item['size']) + ' B'
            if item['size'] > 1024*1024:
                size_str = f"{item['size']/1024/1024:.1f} MB"
            elif item['size'] > 1024:
                size_str = f"{item['size']/1024:.1f} KB"
            fname = item['name'].replace('<','&lt;')
            html += f'<div class="file"><span>{icon} <a href="/{fname}" download>{fname}</a></span><span class="size">{size_str}</span></div>'
        html += '<p style="margin-top:30px;color:#aaa;font-size:12px;text-align:center">UOS运维工具箱 - 局域网文件传输</p>'
        html += '</body></html>'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(html)))
        self.end_headers()
        self.wfile.write(html.encode())
        return

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    directory = sys.argv[2] if len(sys.argv) > 2 else '.'
    os.chdir(directory)
    server = HTTPServer(('0.0.0.0', port), UploadHandler)
    sys.stdout.write('Server started on port ' + str(port) + '\n')
    sys.stdout.flush()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.shutdown()
