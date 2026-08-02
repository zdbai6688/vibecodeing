#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UOS百宝箱 - 局域网文件共享服务（LocalSend 增强版）
用法: python3 localsend_server.py <端口> <共享目录>

功能:
- GET  /            浏览器访问: 文件列表 + 上传页
- GET  /api/status  返回 JSON 状态（供局域网设备发现扫描）
- GET  /api/files   返回 JSON 文件列表
- POST /            multipart 上传（浏览器表单），保留原始文件名
- POST /upload      原始二进制上传（UOS 设备互传），支持 X-File-Name / X-File-Relative-Path
"""
import http.server
import os
import re
import sys
import json
import time
import socket
import urllib.parse
from http.server import HTTPServer, SimpleHTTPRequestHandler


def esc(s):
    return str(s if s is not None else '').replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('"', '&quot;')


class UploadHandler(SimpleHTTPRequestHandler):
    server_version = 'UOSLocalShare/1.0'

    # ---------- 工具 ----------
    def _json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _safe_name(self, name):
        # 仅保留文件名部分，去除路径分隔符，避免目录穿越
        name = os.path.basename((name or '').replace('\\', '/'))
        return name

    def _unique_path(self, directory, filename):
        base, ext = os.path.splitext(filename)
        candidate = filename
        i = 1
        while os.path.exists(os.path.join(directory, candidate)):
            candidate = '%s (%d)%s' % (base, i, ext)
            i += 1
        return os.path.join(directory, candidate)

    def _save(self, data, filename):
        directory = self.directory or '.'
        path = self._unique_path(directory, filename)
        with open(path, 'wb') as f:
            f.write(data)
        return os.path.basename(path)

    def _list_items(self):
        items = []
        directory = self.directory or '.'
        try:
            for f in sorted(os.listdir(directory)):
                fp = os.path.join(directory, f)
                try:
                    st = os.stat(fp)
                    items.append({'name': f, 'size': st.st_size, 'is_dir': os.path.isdir(fp), 'mtime': int(st.st_mtime)})
                except Exception:
                    pass
        except Exception:
            pass
        return items

    # ---------- 处理 ----------
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == '/api/status':
            self._json({
                'status': 'active',
                'hostname': socket.gethostname(),
                'port': self.server.server_port,
                'type': 'uos-share',
                'app': 'UOS百宝箱'
            })
            return
        if parsed.path == '/api/files':
            self._json({'success': True, 'directory': self.directory, 'files': self._list_items()})
            return
        if parsed.path == '/upload':
            # 兼容 UOS 设备原始二进制上传
            self._handle_raw()
            return
        return SimpleHTTPRequestHandler.do_GET(self)

    def do_POST(self):
        ctype = self.headers.get('content-type', '')
        if 'multipart/form-data' in ctype:
            self._handle_multipart(ctype)
        else:
            self._handle_raw()

    def _handle_raw(self):
        try:
            length = int(self.headers.get('content-length', 0) or 0)
            body = self.rfile.read(length) if length > 0 else b''
            raw_name = self.headers.get('x-file-name', '')
            try:
                raw_name = urllib.parse.unquote(raw_name)
            except Exception:
                pass
            filename = self._safe_name(raw_name) or ('upload_' + str(int(time.time())))
            rel_path = self.headers.get('x-file-relative-path', '')
            if rel_path:
                try:
                    rel_path = urllib.parse.unquote(rel_path)
                except Exception:
                    pass
                rel_path = rel_path.replace('\\', '/').lstrip('/')
                # 防目录穿越
                rel_parts = [p for p in rel_path.split('/') if p not in ('', '.', '..')]
                rel_path = '/'.join(rel_parts)
            directory = self.directory or '.'
            save_dir = directory
            if rel_path and '/' in rel_path:
                save_dir = os.path.normpath(os.path.join(directory, os.path.dirname(rel_path)))
                if not save_dir.startswith(os.path.normpath(directory)):
                    save_dir = directory
                os.makedirs(save_dir, exist_ok=True)
            path = self._unique_path(save_dir, filename)
            with open(path, 'wb') as f:
                f.write(body)
            self._json({'success': True, 'name': filename, 'path': os.path.relpath(path, directory), 'size': len(body)})
        except Exception as e:
            self._json({'success': False, 'error': str(e)}, 500)

    def _handle_multipart(self, ctype):
        try:
            boundary = ''
            for part in ctype.split(';')[1:]:
                if '=' in part:
                    k, v = part.strip().split('=', 1)
                    if k.strip() == 'boundary':
                        boundary = v.strip().strip('"')
            if not boundary:
                self._json({'success': False, 'error': '缺少 multipart boundary'}, 400)
                return
            length = int(self.headers.get('content-length', 0) or 0)
            body = self.rfile.read(length)
            saved = []
            delim = ('--' + boundary).encode('utf-8')
            for part in body.split(delim):
                if part.startswith(b'\r\n'):
                    part = part[2:]
                elif part.startswith(b'--'):
                    continue
                if b'\r\n\r\n' not in part:
                    continue
                head, data = part.split(b'\r\n\r\n', 1)
                if data.endswith(b'\r\n'):
                    data = data[:-2]
                head_text = head.decode('utf-8', 'ignore')
                if 'content-disposition' not in head_text.lower():
                    continue
                filename = ''
                m = re.search(r'filename="([^"]*)"', head_text) or re.search(r"filename='([^']*)'", head_text)
                if m:
                    filename = m.group(1)
                if not filename:
                    continue
                filename = self._safe_name(filename)
                fname = self._save(data, filename)
                saved.append({'name': filename, 'file': fname, 'size': len(data)})
            self._json({'success': True, 'files': saved})
        except Exception as e:
            self._json({'success': False, 'error': str(e)}, 500)

    # ---------- 文件列表页面 ----------
    def list_directory(self, path):
        items = self._list_items()
        html = '<html><head><meta charset="utf-8"><title>UOS 局域网文件共享</title>'
        html += '<style>'
        html += 'body{font-family:sans-serif;max-width:800px;margin:20px auto;padding:0 16px;background:#f5f5f5;color:#333}'
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
        html += '<div class="header-info">主机: ' + esc(socket.gethostname()) + ' ｜ 当前目录: ' + esc(self.directory) + '</div>'
        html += '<div class="upload-box">'
        html += '<form action="/" method="post" enctype="multipart/form-data">'
        html += '<div style="font-size:14px;color:#333;margin-bottom:12px"><strong>📤 上传文件到本机</strong></div>'
        html += '<input type="file" name="file" multiple><br><br>'
        html += '<button type="submit">上传文件</button>'
        html += '</form></div>'
        html += '<h2>📄 文件列表 (' + str(len(items)) + ' 个文件/目录)</h2>'
        for item in items:
            icon = '📁' if item['is_dir'] else '📄'
            size_str = str(item['size']) + ' B'
            if item['size'] > 1024 * 1024:
                size_str = '%.1f MB' % (item['size'] / 1024 / 1024)
            elif item['size'] > 1024:
                size_str = '%.1f KB' % (item['size'] / 1024)
            fname = esc(item['name'])
            urlname = urllib.parse.quote(item['name'])
            target = 'href="' + urlname + '"'
            html += '<div class="file"><span>' + icon + ' <a ' + target + ' download>' + fname + '</a></span><span class="size">' + size_str + '</span></div>'
        html += '<p style="margin-top:30px;color:#aaa;font-size:12px;text-align:center">UOS运维工具箱 - 局域网文件传输</p>'
        html += '</body></html>'
        encoded = html.encode('utf-8')
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)
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
