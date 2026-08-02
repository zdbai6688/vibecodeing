#!/usr/bin/env python3
"""UOS运维工具箱 - 工具辅助脚本（图片处理、OCR、音频/视频处理）"""
import sys, json, os, subprocess

def check_tesseract():
    """检查 tesseract 是否可用"""
    try:
        r = subprocess.run(['tesseract', '--version'], capture_output=True, text=True, timeout=5)
        return r.returncode == 0
    except:
        return False

def install_tesseract():
    """尝试安装 tesseract（需要管理员权限）"""
    try:
        r = subprocess.run(
            ['apt-get', 'install', '-y', 'tesseract-ocr', 'tesseract-ocr-chi-sim', 'tesseract-ocr-eng'],
            capture_output=True, text=True, timeout=120
        )
        return r.returncode == 0
    except:
        return False

def _to_int(value, default=None):
    """将参数转为正整数，空/非法值返回 default"""
    if value is None or value == '':
        return default
    try:
        v = int(float(value))
        return v if v > 0 else default
    except (ValueError, TypeError):
        return default

def _image_save(img, out, fmt):
    """按目标格式保存图片，JPEG 自动去除 Alpha 通道"""
    if fmt in ('jpg', 'jpeg') and img.mode in ('RGBA', 'LA', 'P'):
        img = img.convert('RGB')
    img.save(out)

def _ff_esc(arg):
    """为 ffmpeg 命令行转义参数"""
    return '"' + str(arg).replace('"', '\\"') + '"'

def _run_ff(args, timeout=300):
    """运行 ffmpeg 命令，args 为字符串列表"""
    cmd = 'ffmpeg -y ' + ' '.join(args)
    subprocess.run(cmd, shell=True, timeout=timeout, capture_output=True, text=True, check=True)

def _audio_ext(action):
    """根据音频操作返回输出扩展名"""
    exts = {
        'to-mp3': '.mp3', 'to-wav': '.wav', 'to-flac': '.flac',
        'to-aac': '.aac', 'to-ogg': '.ogg', 'compress': '.mp3'
    }
    return exts.get(action, '.mp3')

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "缺少参数"}))
        return
    
    cmd = sys.argv[1]
    params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    
    try:
        if cmd == 'check-tesseract':
            print(json.dumps({"success": True, "installed": check_tesseract()}))
            
        elif cmd == 'image-resize':
            from PIL import Image
            f = params['file']
            w = _to_int(params.get('width'))
            h = _to_int(params.get('height'))
            base, ext = os.path.splitext(f)
            out_ext = ext.lower() if ext.lower() in ('.png', '.jpg', '.jpeg', '.bmp', '.webp') else '.png'
            out = f"{base}_resized{out_ext}"
            img = Image.open(f)
            ow, oh = img.size
            if w is None and h is None:
                nw, nh = ow, oh
            elif w is None:
                nh = h
                nw = max(1, int(round(ow * nh / oh)))
            elif h is None:
                nw = w
                nh = max(1, int(round(oh * nw / ow)))
            else:
                nw, nh = w, h
            img = img.resize((nw, nh), Image.LANCZOS if hasattr(Image, 'LANCZOS') else Image.ANTIALIAS)
            _image_save(img, out, out_ext.lstrip('.'))
            print(json.dumps({"success": True, "output": out, "width": nw, "height": nh}))
            
        elif cmd == 'image-convert':
            from PIL import Image
            f = params['file']
            fmt = str(params.get('format', 'png')).lower().lstrip('.')
            if fmt not in ('png', 'jpg', 'jpeg', 'bmp', 'webp', 'gif', 'tiff'):
                print(json.dumps({"success": False, "error": f"不支持的目标格式: {fmt}"}))
                return
            base, ext = os.path.splitext(f)
            src_ext = ext.lower().lstrip('.')
            out = f"{base}.{fmt}" if fmt != src_ext else f"{base}_converted.{fmt}"
            img = Image.open(f)
            _image_save(img, out, fmt)
            print(json.dumps({"success": True, "output": out}))

        elif cmd == 'video-process':
            f = params['file']
            action = params.get('action', '')
            output = params.get('output', '')
            start = params.get('start')
            duration = params.get('duration')
            if not f or not os.path.isfile(f):
                print(json.dumps({"success": False, "error": "文件不存在或未选择"}))
                return
            out_path = output or os.path.join('/tmp', 'processed_' + os.path.basename(f))
            if action == 'trim':
                if start is None or duration is None:
                    print(json.dumps({"success": False, "error": "请提供开始时间和时长"}))
                    return
                s = float(start); d = float(duration)
                if s < 0 or d <= 0:
                    print(json.dumps({"success": False, "error": "无效的开始时间或时长"}))
                    return
                base, ext = os.path.splitext(out_path)
                final_ext = ext if ext else '.mp4'
                out = base + '_trim' + final_ext
                _run_ff(['-ss', str(s), '-t', str(d), '-i', _ff_esc(f), '-c', 'copy', _ff_esc(out)])
                print(json.dumps({"success": True, "output": out}))
            elif action == 'compress':
                out = os.path.splitext(out_path)[0] + '_compressed.mp4'
                _run_ff(['-i', _ff_esc(f), '-vcodec', 'libx264', '-crf', '28', _ff_esc(out)])
                print(json.dumps({"success": True, "output": out}))
            elif action == 'to-mp4':
                out = os.path.splitext(out_path)[0] + '.mp4'
                _run_ff(['-i', _ff_esc(f), '-c:v', 'libx264', '-c:a', 'aac', _ff_esc(out)])
                print(json.dumps({"success": True, "output": out}))
            else:
                print(json.dumps({"success": False, "error": f"未知操作: {action}"}))

        elif cmd == 'audio-process':
            f = params['file']
            action = params.get('action', '')
            output = params.get('output', '')
            start = params.get('start')
            duration = params.get('duration')
            if not f or not os.path.isfile(f):
                print(json.dumps({"success": False, "error": "文件不存在或未选择"}))
                return
            out_path = output or os.path.join('/tmp', 'processed_' + os.path.basename(f))
            if action == 'trim':
                if start is None or duration is None:
                    print(json.dumps({"success": False, "error": "请提供开始时间和时长"}))
                    return
                s = float(start); d = float(duration)
                if s < 0 or d <= 0:
                    print(json.dumps({"success": False, "error": "无效的开始时间或时长"}))
                    return
                base, ext = os.path.splitext(out_path)
                final_ext = ext if ext else '.mp3'
                out = base + '_trim' + final_ext
                _run_ff(['-ss', str(s), '-t', str(d), '-i', _ff_esc(f), '-c', 'copy', _ff_esc(out)])
                print(json.dumps({"success": True, "output": out}))
            else:
                enc_ext = _audio_ext(action)
                if not enc_ext:
                    print(json.dumps({"success": False, "error": f"未知操作: {action}"}))
                    return
                out = os.path.splitext(out_path)[0] + enc_ext
                encoder_flags = {
                    'to-mp3': ['-codec:a', 'libmp3lame', '-qscale:a', '2'],
                    'to-wav': ['-codec:a', 'pcm_s16le'],
                    'to-flac': ['-codec:a', 'flac'],
                    'to-aac': ['-codec:a', 'aac', '-b:a', '192k'],
                    'to-ogg': ['-codec:a', 'libvorbis', '-qscale:a', '4'],
                    'compress': ['-codec:a', 'libmp3lame', '-b:a', '128k'],
                }
                flags = encoder_flags.get(action, encoder_flags['compress'])
                _run_ff(['-i', _ff_esc(f)] + flags + [_ff_esc(out)])
                print(json.dumps({"success": True, "output": out}))

        elif cmd == 'scan-effect':
            from PIL import Image, ImageOps, ImageEnhance
            f = params['file']
            base, ext = os.path.splitext(f)
            out = f"{base}_scanned.png"
            img = Image.open(f).convert('L')
            img = ImageOps.autocontrast(img, cutoff=2)
            img = ImageEnhance.Contrast(img).enhance(1.6)
            img = ImageEnhance.Sharpness(img).enhance(1.5)
            img.save(out)
            print(json.dumps({"success": True, "output": out}))

        elif cmd == 'ocr':
            if not check_tesseract():
                print(json.dumps({"success": False, "error": "tesseract-ocr 未安装", "need_install": True}))
                return
            from PIL import Image
            import pytesseract
            f = params['file']
            lang = params.get('lang', 'chi_sim+eng')
            img = Image.open(f)
            text = pytesseract.image_to_string(img, lang=lang)
            print(json.dumps({"success": True, "output": text.strip()}))
            
        elif cmd == 'query-cve':
            cve_id = params if isinstance(params, str) else params.get('cve', '')
            if not cve_id:
                print(json.dumps({"success": False, "error": "请提供 CVE 编号"}))
                return
            import urllib.request
            import json as json_mod
            try:
                url = f"https://services.nvd.nist.gov/rest/json/cves/2.0?cveId={cve_id}"
                req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
                with urllib.request.urlopen(req, timeout=10) as resp:
                    data = json_mod.loads(resp.read().decode())
                vuln = data.get('vulnerabilities', [{}])[0].get('cve', {})
                descs = vuln.get('descriptions', [{}])
                desc = next((d['value'] for d in descs if d.get('lang') == 'en'), descs[0].get('value', ''))
                metrics = vuln.get('metrics', {})
                cvss = metrics.get('cvssMetricV31', [{}])[0].get('cvssData', {}) or metrics.get('cvssMetricV30', [{}])[0].get('cvssData', {}) or {}
                severity = cvss.get('baseSeverity', 'N/A')
                score = cvss.get('baseScore', 'N/A')
                pub = vuln.get('published', '')
                output = f"CVE: {cve_id}\nSeverity: {severity} (Score: {score})\nPublished: {pub}\nDescription: {desc}"
                print(json.dumps({"success": True, "output": output}))
            except Exception as e:
                print(json.dumps({"success": False, "error": str(e)}))
        elif cmd == 'install-tesseract':
            print(json.dumps({"success": False, "error": "请使用系统包管理器安装: sudo apt-get install tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng"}))
            
        else:
            print(json.dumps({"success": False, "error": f"未知命令: {cmd}"}))
            
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))

if __name__ == '__main__':
    main()
