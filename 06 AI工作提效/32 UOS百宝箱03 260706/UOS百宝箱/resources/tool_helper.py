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

def _get_page_sizes(pdf_path):
    """Get page dimensions (width, height) in points for each page of a PDF."""
    try:
        r = subprocess.run(['pdfinfo', pdf_path], capture_output=True, text=True, timeout=30)
        info = {}
        for line in r.stdout.split('\n'):
            if ':' in line:
                k, v = line.split(':', 1)
                info[k.strip()] = v.strip()
        # Page size: e.g. "612 x 792 pts (letter)"
        pagestr = info.get('Page size', '595 x 842 pts')
        m = re.match(r'(\d+)\s*x\s*(\d+)', pagestr)
        if m:
            w = float(m.group(1))
            h = float(m.group(2))
            pages = int(info.get('Pages', '1'))
            return [(w, h)] * pages
    except:
        pass
    return [(595, 842)]

def _images_to_pdf(image_paths, output_pdf, page_sizes=None, dpi=150):
    """Convert PNG images to a multi-page PDF via PostScript pipeline.
    
    Each page size is set to match the original document.
    """
    import tempfile
    ps_parts = [
        '%!PS-Adobe-3.0',
        f'%%Pages: {len(image_paths)}',
        '%%EndComments'
    ]
    
    for idx, img_path in enumerate(image_paths):
        from PIL import Image
        im = Image.open(img_path)
        w_px, h_px = im.size
        
        if page_sizes and idx < len(page_sizes):
            w_pt, h_pt = page_sizes[idx]
        else:
            w_pt = w_px / dpi * 72
            h_pt = h_px / dpi * 72
        
        scale = 72.0 / dpi
        
        # Convert PNG → PPM → PS
        ps_raw = subprocess.run(
            ['pngtopnm', img_path],
            capture_output=True, timeout=30
        ).stdout
        
        ps_result = subprocess.run(
            ['pnmtops', '-noturn', '-scale', str(scale)],
            input=ps_raw, capture_output=True, timeout=30
        ).stdout
        
        ps_text = ps_result.decode('latin-1')
        
        # Insert page size setting before page content
        # Remove the EPS header, keep only the page content
        ps_lines = ps_text.split('\n')
        page_lines = []
        in_page = False
        for line in ps_lines:
            if line.startswith('%%Page:'):
                page_lines.append(f'<< /PageSize [{int(round(w_pt))} {int(round(h_pt))}] >> setpagedevice')
                page_lines.append('gsave')
                in_page = True
            elif in_page:
                page_lines.append(line)
            elif line.startswith('gsave'):
                page_lines.append(f'<< /PageSize [{int(round(w_pt))} {int(round(h_pt))}] >> setpagedevice')
                page_lines.append(line)
                in_page = True
        
        # Add grestore and showpage at end
        page_lines.append('grestore')
        page_lines.append('showpage')
        
        ps_parts.extend(page_lines)
    
    ps_content = '\n'.join(ps_parts)
    
    fd, tmp_ps = tempfile.mkstemp(suffix='.ps', prefix='uos_pdf_')
    os.close(fd)
    with open(tmp_ps, 'w', encoding='latin-1') as f:
        f.write(ps_content)
    
    try:
        result = subprocess.run(
            ['ps2pdf', tmp_ps, output_pdf],
            capture_output=True, timeout=120
        )
        return result.returncode == 0
    finally:
        try:
            os.unlink(tmp_ps)
        except:
            pass

def _pdf_bleach_impl(params):
    """PDF 漂白去底色 — 去除扫描件灰色背景"""
    from PIL import Image
    src = params['file']
    output = params.get('output', '')
    threshold = int(params.get('threshold', 200))
    gray = bool(params.get('grayscale', True))
    dpi = int(params.get('dpi', 150))
    
    if not src or not os.path.isfile(src):
        return {"success": False, "error": "文件不存在"}
    
    if not output:
        base, ext = os.path.splitext(src)
        output = f"{base}_bleached.pdf"
    
    # Create temp dir for page images
    tmpdir = tempfile.mkdtemp(prefix='uos_bleach_')
    
    try:
        # Get page sizes
        page_sizes = _get_page_sizes(src)
        
        # Render pages to images with pdftoppm
        base_name = os.path.basename(src).replace('.pdf', '')
        render_cmd = [
            'pdftoppm', '-png', '-r', str(dpi),
            src, os.path.join(tmpdir, base_name + '-page')
        ]
        subprocess.run(render_cmd, check=True, timeout=120)
        
        # Collect rendered page files
        rendered = sorted([
            os.path.join(tmpdir, f)
            for f in os.listdir(tmpdir)
            if f.startswith(base_name + '-page') and f.endswith('.png')
        ])
        
        if not rendered:
            return {"success": False, "error": "PDF 渲染失败"}
        
        # Process each page — whiten background
        processed = []
        for page_path in rendered:
            img = Image.open(page_path).convert('RGB')
            pixels = img.load()
            w, h = img.size
            
            # Sample corners to estimate background color
            corners = [
                pixels[0, 0], pixels[w-1, 0],
                pixels[0, h-1], pixels[w-1, h-1]
            ]
            bg_r = sum(c[0] for c in corners) // 4
            bg_g = sum(c[1] for c in corners) // 4
            bg_b = sum(c[2] for c in corners) // 4
            
            # Whiten pixels close to background color
            for y in range(h):
                for x in range(w):
                    r, g, b = pixels[x, y]
                    dr, dg, db = abs(r - bg_r), abs(g - bg_g), abs(b - bg_b)
                    dist = max(dr, dg, db)
                    if dist < (255 - threshold):
                        pixels[x, y] = (255, 255, 255)
            
            if gray:
                img = img.convert('L').convert('RGB')
            
            proc_path = os.path.join(tmpdir, f'proc_{os.path.basename(page_path)}')
            img.save(proc_path)
            processed.append(proc_path)
        
        if not processed:
            return {"success": False, "error": "处理失败"}
        
        # Rebuild PDF from processed images
        ok = _images_to_pdf(processed, output, page_sizes, dpi)
        if not ok:
            return {"success": False, "error": "PDF 重建失败"}
        
        return {"success": True, "output": output, "pages": len(processed)}
    
    finally:
        # Cleanup temp dir
        try:
            import shutil
            shutil.rmtree(tmpdir)
        except:
            pass

def _pdf_add_pagenum_impl(params):
    """PDF 添加页码 — 每页底部标注页码"""
    from PIL import Image, ImageDraw, ImageFont
    src = params['file']
    output = params.get('output', '')
    dpi = int(params.get('dpi', 150))
    position = params.get('position', 'bottom')  # bottom, top
    fmt = params.get('format', '第{n}页 / 共{total}页')  # page number format
    
    if not src or not os.path.isfile(src):
        return {"success": False, "error": "文件不存在"}
    
    if not output:
        base, ext = os.path.splitext(src)
        output = f"{base}_pagenum.pdf"
    
    tmpdir = tempfile.mkdtemp(prefix='uos_pgnum_')
    
    try:
        # Get page count and sizes
        page_sizes = _get_page_sizes(src)
        total_pages = len(page_sizes)
        
        # Render pages to images
        base_name = os.path.basename(src).replace('.pdf', '')
        render_cmd = [
            'pdftoppm', '-png', '-r', str(dpi),
            src, os.path.join(tmpdir, base_name + '-page')
        ]
        subprocess.run(render_cmd, check=True, timeout=120)
        
        rendered = sorted([
            os.path.join(tmpdir, f)
            for f in os.listdir(tmpdir)
            if f.startswith(base_name + '-page') and f.endswith('.png')
        ])
        
        if not rendered:
            return {"success": False, "error": "PDF 渲染失败"}
        
        processed = []
        for i, page_path in enumerate(rendered):
            img = Image.open(page_path).convert('RGB')
            draw = ImageDraw.Draw(img)
            w, h = img.size
            
            # Generate page number text
            page_text = fmt.replace('{n}', str(i + 1)).replace('{total}', str(total_pages))
            
            # Try to find a usable font
            fonts_to_try = [
                '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
                '/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc',
                '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc',
                '/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc',
                '/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf',
            ]
            font = None
            for fp in fonts_to_try:
                if os.path.exists(fp):
                    try:
                        font = ImageFont.truetype(fp, max(14, dpi // 8))
                        break
                    except:
                        pass
            if font is None:
                font = ImageFont.load_default()
            
            # Draw page number at bottom center
            bbox = draw.textbbox((0, 0), page_text, font=font)
            tw = bbox[2] - bbox[0]
            th = bbox[3] - bbox[1]
            
            margin = int(dpi * 0.5)  # 0.5 inch margin
            x = (w - tw) // 2
            if position == 'top':
                y = margin
            else:
                y = h - margin - th
            
            # Draw background rectangle for readability
            draw.rectangle(
                [x - 6, y - 2, x + tw + 6, y + th + 4],
                fill=(255, 255, 255)
            )
            draw.text((x, y), page_text, fill=(0, 0, 0), font=font)
            
            proc_path = os.path.join(tmpdir, f'proc_{os.path.basename(page_path)}')
            img.save(proc_path)
            processed.append(proc_path)
        
        # Rebuild PDF
        ok = _images_to_pdf(processed, output, page_sizes, dpi)
        if not ok:
            return {"success": False, "error": "PDF 重建失败"}
        
        return {"success": True, "output": output, "pages": len(processed)}
    
    finally:
        try:
            import shutil
            shutil.rmtree(tmpdir)
        except:
            pass

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

        elif cmd == 'pdf-bleach':
            result = _pdf_bleach_impl(params)
            print(json.dumps(result))
            
        elif cmd == 'pdf-add-pagenum':
            result = _pdf_add_pagenum_impl(params)
            print(json.dumps(result))
            
        elif cmd == 'install-tesseract':
            print(json.dumps({"success": False, "error": "请使用系统包管理器安装: sudo apt-get install tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng"}))
            
        else:
            print(json.dumps({"success": False, "error": f"未知命令: {cmd}"}))
            
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))

if __name__ == '__main__':
    main()
