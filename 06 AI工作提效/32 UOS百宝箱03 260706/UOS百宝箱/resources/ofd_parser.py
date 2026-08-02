#!/usr/bin/env python3
"""UOS运维工具箱 - OFD 文档解析器
解析 GB/T 33190-2016 版式文档（OFD）并将每页内容输出为可渲染的 JSON 结构。
支持：TextObject / ImageObject / PathObject / Rect / Line / Circle / Ellipse / Arc
依赖：python3 内置 zipfile + xml.etree.ElementTree (无第三方依赖)
"""
import sys, json, os, zipfile, base64, re
from xml.etree import ElementTree as ET
from io import BytesIO

OFD_NS = {'ofd': 'http://www.ofdspec.org/2016'}

def _ns(xpath):
    """添加 OFD 命名空间前缀，同时兼容无命名空间的情况"""
    parts = xpath.split('/')
    result = []
    for p in parts:
        if not p:
            continue
        if p.startswith('.') or p.startswith('*'):
            result.append(p)
        else:
            result.append(f'ofd:{p}')
    return '/'.join(result)

def _find(el, path):
    """带命名空间的 find"""
    r = el.find(_ns(path), OFD_NS)
    if r is None and ':' not in path:
        r = el.find(path)
    return r

def _findall(el, path):
    """带命名空间的 findall"""
    r = el.findall(_ns(path), OFD_NS)
    if not r and ':' not in path:
        r = el.findall(path)
    return r

def _text(el, default=''):
    return el.text.strip() if el is not None and el.text else default

def _attr(el, key, default=None):
    return el.get(key) or default

def _parse_boundary(boundary_str):
    """解析 Boundary 属性 (Left Top Right Bottom) -> (x, y, w, h) in mm"""
    if not boundary_str:
        return (0, 0, 0, 0)
    parts = boundary_str.strip().split()
    if len(parts) >= 4:
        left, top, right, bottom = map(float, parts[:4])
        return (left, top, right - left, abs(bottom - top))
    return (0, 0, 0, 0)

def _parse_ctm(ctm_str):
    """解析 CTM 矩阵 "a b c d e f" -> [a,b,c,d,e,f]"""
    if not ctm_str:
        return [1.0, 0.0, 0.0, 1.0, 0.0, 0.0]
    parts = ctm_str.strip().split()
    return [float(x) for x in parts[:6]]

def _apply_ctm(ctm, x, y):
    """应用 CTM 变换: [a,b,c,d,e,f] * (x,y,1) -> (x', y')"""
    a, b, c, d, e, f = ctm[:6]
    return (a * x + c * y + e, b * x + d * y + f)

def _parse_color(color_str):
    """解析颜色值: '#RRGGBB' 或 'R G B' (0-255) 或 'C M Y K' (0-255)"""
    if not color_str:
        return None
    color_str = color_str.strip()
    if color_str.startswith('#'):
        return color_str  # 直接返回 HTML 颜色
    parts = color_str.split()
    if len(parts) == 3:
        try:
            r, g, b = int(float(parts[0])), int(float(parts[1])), int(float(parts[2]))
            return f'#{r:02x}{g:02x}{b:02x}'
        except:
            return '#000000'
    elif len(parts) == 4:
        # CMYK (OFD uses 0-255 range)
        try:
            c, m, y, k = float(parts[0])/255, float(parts[1])/255, float(parts[2])/255, float(parts[3])/255
            r = int(255 * (1 - c) * (1 - k))
            g = int(255 * (1 - m) * (1 - k))
            b = int(255 * (1 - y) * (1 - k))
            return f'#{r:02x}{g:02x}{b:02x}'
        except:
            return '#000000'
    return '#000000'

def _mm_to_px(mm):
    """毫米转像素 (96 DPI)"""
    return mm * 96.0 / 25.4

class OFDParser:
    def __init__(self, filepath):
        self.filepath = filepath
        self.zf = None
        self.resources = {}  # 资源缓存: {res_id: resource_dict}
        self.page_resources = {}  # 页面级资源
        self.doc_dir = ''
        self.common_data = None
        self.page_w = 0
        self.page_h = 0
        self.pages = []
        self.fonts = {}  # {font_id: font_info}
        self.draw_params = {}  # {param_id: {fill, stroke, line_width, ...}}
        self.images = {}  # {res_id: {type, data_url}}
    
    def parse(self):
        try:
            with zipfile.ZipFile(self.filepath, 'r') as self.zf:
                return self._do_parse()
        except zipfile.BadZipFile:
            return {'success': False, 'error': '无效的 OFD 文件格式'}
        except Exception as e:
            return {'success': False, 'error': f'解析失败: {str(e)}'}
    
    def _do_parse(self):
        # 1. 读取 OFD.xml
        ofd_xml = self.zf.read('OFD.xml')
        ofd_root = ET.fromstring(ofd_xml)
        doc_body = _find(ofd_root, 'DocBody')
        if doc_body is None:
            return {'success': False, 'error': '未找到 DocBody'}
        doc_root = doc_body.find('ofd:DocRoot', OFD_NS)
        if doc_root is None:
            doc_root = doc_body.find('DocRoot')
        if doc_root is None or not doc_root.text:
            return {'success': False, 'error': '未找到 DocRoot'}
        
        doc_path = doc_root.text.strip()
        self.doc_dir = os.path.dirname(doc_path)
        
        # 2. 读取 Document.xml
        doc_xml = self.zf.read(doc_path)
        doc = ET.fromstring(doc_xml)
        
        # 3. 解析 CommonData
        cd = _find(doc, 'CommonData')
        if cd is None:
            return {'success': False, 'error': '未找到 CommonData'}
        self.common_data = cd
        
        # 页面区域
        pa = _find(cd, 'PageArea')
        pb = _find(pa, 'PhysicalBox') if pa is not None else None
        if pb is not None and pb.text:
            parts = pb.text.strip().split()
            if len(parts) >= 4:
                self.page_w = float(parts[2]) - float(parts[0])
                self.page_h = float(parts[3]) - float(parts[1])
        if self.page_w <= 0:
            self.page_w = 210  # A4 default
        if self.page_h <= 0:
            self.page_h = 297
        
        # 4. 解析文档级资源 (DocumentRes)
        doc_res = _find(cd, 'DocumentRes')
        if doc_res is not None:
            res_base = doc_res.get('BaseLoc')
            if res_base:
                self._load_resources(os.path.normpath(os.path.join(self.doc_dir, res_base)), 'document')
        
        # 5. 获取页面列表
        pages_el = _find(doc, 'Pages')
        page_refs = _findall(pages_el, 'Page') if pages_el is not None else []
        # 也检查 CommonData 中的页面引用
        if not page_refs:
            page_refs = _findall(cd, 'Page')
        
        for idx, pg in enumerate(page_refs):
            base_loc = pg.get('BaseLoc')
            if not base_loc:
                continue
            content_path = os.path.normpath(os.path.join(self.doc_dir, base_loc))
            # 页面级资源目录
            page_res_dir = os.path.dirname(content_path)
            
            # 加载页面级资源
            self.page_resources = {}
            self._load_page_res(page_res_dir)
            
            # 解析页面内容
            page_data = self._parse_page(content_path, idx + 1)
            self.pages.append(page_data)
        
        result_pages = []
        for pg in self.pages:
            result_pages.append({
                'index': pg['index'],
                'width': self.page_w,
                'height': self.page_h,
                'elements': pg['elements']
            })
        
        return {
            'success': True,
            'file': os.path.basename(self.filepath),
            'pageCount': len(result_pages),
            'pageWidth': self.page_w,
            'pageHeight': self.page_h,
            'pages': result_pages
        }
    
    def _load_page_res(self, page_dir):
        """加载页面级资源文件"""
        res_path = os.path.join(page_dir, 'PageRes.xml')
        if res_path not in self.zf.namelist():
            # 尝试不同大小写
            namelist = self.zf.namelist()
            for n in namelist:
                if n.lower() == res_path.lower():
                    res_path = n
                    break
            else:
                return
        try:
            res_xml = self.zf.read(res_path)
            res_root = ET.fromstring(res_xml)
            self._parse_resources(res_root, 'page')
        except:
            pass
    
    def _load_resources(self, res_path, scope='document'):
        """加载资源文件"""
        try:
            res_xml = self.zf.read(res_path)
            res_root = ET.fromstring(res_xml)
            self._parse_resources(res_root, scope)
        except:
            pass
    
    def _parse_resources(self, res_root, scope):
        """解析资源内容"""
        # 字体
        fonts_el = _find(res_root, 'Fonts')
        if fonts_el is not None:
            for font in _findall(fonts_el, 'Font'):
                fid = font.get('ID')
                if fid:
                    self.fonts[fid] = {
                        'fontName': _attr(font, 'FontName', ''),
                        'familyName': _attr(font, 'FamilyName', 'SimSun'),
                        'fontSize': float(_attr(font, 'FontSize', '12')),
                        'bold': _attr(font, 'Bold', 'false') == 'true',
                        'italic': _attr(font, 'Italic', 'false') == 'true',
                    }
        
        # 绘制参数
        dp_el = _find(res_root, 'DrawParams')
        if dp_el is not None:
            for dp in _findall(dp_el, 'DrawParam'):
                fid = dp.get('ID')
                if fid:
                    fill = _find(dp, 'FillColor')
                    stroke = _find(dp, 'StrokeColor')
                    self.draw_params[fid] = {
                        'fillColor': _parse_color(_attr(fill, 'ColorValue', '')),
                        'strokeColor': _parse_color(_attr(stroke, 'ColorValue', '')),
                        'lineWidth': float(_attr(_find(dp, 'LineWidth'), 'Value', '0') or 0),
                        'dashPattern': _attr(_find(dp, 'DashOffset'), 'Pattern', ''),
                        'joinType': _attr(_find(dp, 'JoinType'), 'Value', ''),
                        'capType': _attr(_find(dp, 'CapType'), 'Value', ''),
                    }
        
        # 多媒体资源（图片）
        mm_el = _find(res_root, 'MultiMedias')
        if mm_el is not None:
            for mm in _findall(mm_el, 'MultiMedia'):
                mid = mm.get('ID')
                if mid:
                    media_type = _attr(mm, 'Type', 'Image')
                    media_file = _attr(mm, 'MediaFile', '')
                    if media_file and media_type == 'Image':
                        # 相对于资源文件所在目录
                        res_dir = os.path.dirname(self.res_path if hasattr(self, 'res_path') else '')
                        img_path = media_file
                        # 如果是相对路径，尝试在页面目录和文档目录查找
                        self.images[mid] = {'file': img_path, 'type': 'Image'}
    
    def _resolve_image(self, res_id):
        """获取图片的 data URL"""
        if res_id not in self.images:
            return None
        img_info = self.images[res_id]
        img_path = img_info['file']
        
        # 在多个位置查找图片文件
        candidates = [
            img_path,
            os.path.join(self.doc_dir, img_path),
            os.path.join(self.doc_dir, 'Res', os.path.basename(img_path)),
            os.path.join(self.doc_dir, 'res', os.path.basename(img_path)),
        ]
        
        namelist = self.zf.namelist()
        for c in candidates:
            c = c.replace('\\', '/')
            if c in namelist:
                try:
                    data = self.zf.read(c)
                    ext = os.path.splitext(c)[1].lower()
                    mime = {'png': 'image/png', 'jpg': 'image/jpeg', 'jpeg': 'image/jpeg',
                            'gif': 'image/gif', 'bmp': 'image/bmp', 'webp': 'image/webp',
                            'tif': 'image/tiff', 'tiff': 'image/tiff'}.get(ext.lstrip('.'), 'image/png')
                    return f'data:{mime};base64,{base64.b64encode(data).decode()}'
                except:
                    return None
            # 也尝试不区分大小写
            for n in namelist:
                if n.lower() == c.lower():
                    try:
                        data = self.zf.read(n)
                        ext = os.path.splitext(n)[1].lower()
                        mime = {'png': 'image/png', 'jpg': 'image/jpeg', 'jpeg': 'image/jpeg',
                                'gif': 'image/gif', 'bmp': 'image/bmp', 'webp': 'image/webp',
                                'tif': 'image/tiff', 'tiff': 'image/tiff'}.get(ext.lstrip('.'), 'image/png')
                        return f'data:{mime};base64,{base64.b64encode(data).decode()}'
                    except:
                        return None
        return None
    
    def _parse_page(self, content_path, page_index):
        """解析单页内容"""
        elements = []
        try:
            content_xml = self.zf.read(content_path)
            page_root = ET.fromstring(content_xml)
            
            # 页面内容可能在 <Content> 中，也可能直接在 <Page> 下
            content_el = _find(page_root, 'Content')
            if content_el is None:
                content_el = page_root
            
            self._parse_layer(content_el, elements, [1.0, 0.0, 0.0, 1.0, 0.0, 0.0])
        except Exception as e:
            elements.append({'type': 'text', 'x': 10, 'y': 10, 'w': 200, 'h': 20,
                           'text': f'[页面 {page_index} 解析错误: {str(e)}]',
                           'fontSize': 12, 'color': '#ff0000'})
        
        return {'index': page_index, 'elements': elements}
    
    def _parse_layer(self, parent_el, elements, parent_ctm):
        """递归解析页面层 (Layer / PageBlock)"""
        for layer in _findall(parent_el, 'Layer'):
            self._parse_page_block(layer, elements, parent_ctm)
        
        # 处理直接子元素（如果没有 Layer 包装）
        if not _findall(parent_el, 'Layer'):
            self._parse_page_block(parent_el, elements, parent_ctm)
    
    def _parse_page_block(self, block_el, elements, parent_ctm):
        """解析 PageBlock 中的元素"""
        # CTM 继承
        block_ctm_str = _attr(block_el, 'CTM', '')
        block_ctm = _parse_ctm(block_ctm_str) if block_ctm_str else parent_ctm
        
        for child in block_el:
            tag = child.tag
            # 去除命名空间
            if '}' in tag:
                tag = tag.split('}', 1)[1]
            
            if tag == 'TextObject':
                el = self._parse_text_object(child)
                if el:
                    elements.append(el)
            elif tag == 'ImageObject':
                el = self._parse_image_object(child)
                if el:
                    elements.append(el)
            elif tag == 'PathObject':
                self._parse_path_object(child, elements, block_ctm)
            elif tag == 'Rect':
                el = self._parse_rect(child, block_ctm)
                if el:
                    elements.append(el)
            elif tag == 'Line':
                el = self._parse_line(child, block_ctm)
                if el:
                    elements.append(el)
            elif tag == 'Circle':
                el = self._parse_circle(child, block_ctm)
                if el:
                    elements.append(el)
            elif tag == 'Ellipse':
                el = self._parse_ellipse(child, block_ctm)
                if el:
                    elements.append(el)
            elif tag == 'Arc':
                el = self._parse_arc(child, block_ctm)
                if el:
                    elements.append(el)
            elif tag == 'PageBlock':
                # 嵌套块
                self._parse_page_block(child, elements, block_ctm)
            elif tag == 'CompositeObject':
                # 复合对象，包含一个 PageBlock
                for sub in child:
                    self._parse_page_block(sub, elements, block_ctm)
    
    def _resolve_draw_param(self, param_id):
        """解析绘制参数引用"""
        if not param_id:
            return {}
        dp = self.draw_params.get(param_id, {})
        return {
            'fillColor': dp.get('fillColor'),
            'strokeColor': dp.get('strokeColor'),
            'lineWidth': dp.get('lineWidth', 0),
            'dashPattern': dp.get('dashPattern', ''),
        }
    
    def _parse_text_object(self, el):
        """解析文本对象"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        # CTM
        ctm = _parse_ctm(_attr(el, 'CTM', ''))
        
        # 字体
        font_id = _attr(el, 'Font', '')
        if not font_id:
            font_el = _find(el, 'Font')
            if font_el is not None and font_el.text:
                font_id = font_el.text.strip()
        if not font_id:
            font_id = _attr(el, 'FontID', '')
        # Font size from attribute or child element
        font_size_str = _attr(el, 'Size', '')
        if not font_size_str:
            size_el = _find(el, 'Size')
            if size_el is not None and size_el.text:
                font_size_str = size_el.text.strip()
        font_info = self.fonts.get(font_id, {})
        family_name = font_info.get('familyName', 'SimSun')
        font_size = float(font_size_str or 0) or font_info.get('fontSize', 12)
        bold = font_info.get('bold', False) or _attr(el, 'Weight', '') == '700'
        italic = font_info.get('italic', False)
        
        # 颜色
        fill_el = _find(el, 'FillColor')
        if fill_el is None:
            # 可能通过 DrawParam 引用
            dp_id = _attr(el, 'DrawParam', '')
            dp = self.draw_params.get(dp_id, {})
            color = dp.get('fillColor', '#000000')
        else:
            color = _parse_color(_attr(fill_el, 'ColorValue', '')) or '#000000'
        
        # 水平缩放
        hscale = float(_attr(el, 'HScale', '1') or 1)
        
        # 文字编码
        text_codes = _findall(el, 'TextCode')
        text_parts = []
        
        for tc in text_codes:
            tx = float(_attr(tc, 'X', '0') or 0)
            ty = float(_attr(tc, 'Y', '0') or 0)
            delta_x = _attr(tc, 'DeltaX', '')
            delta_y = _attr(tc, 'DeltaY', '')
            text = tc.text or ''
            
            char_deltas = []
            if delta_x:
                char_deltas = [float(x) for x in delta_x.strip().split()]
            
            text_parts.append({
                'x': bx + tx,
                'y': by + ty,
                'text': text,
                'deltaX': char_deltas,
                'deltaY': list(map(float, delta_y.strip().split())) if delta_y else []
            })
        
        if not text_parts:
            return None
        
        # 合并所有文本部分
        full_text = ''.join(p['text'] for p in text_parts)
        if not full_text.strip():
            return None
        
        # 使用第一个 TextCode 的位置
        first = text_parts[0]
        pos_x = first['x']
        pos_y = first['y']
        
        return {
            'type': 'text',
            'x': pos_x,
            'y': pos_y,
            'w': bw,
            'h': bh,
            'text': full_text,
            'fontSize': font_size,
            'fontFamily': family_name,
            'color': color,
            'bold': bold,
            'italic': italic,
            'hscale': hscale
        }
    
    def _parse_image_object(self, el):
        """解析图片对象"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        res_id = _attr(el, 'ResourceID', '')
        data_url = self._resolve_image(res_id)
        if data_url is None:
            return {
                'type': 'rect',
                'x': bx, 'y': by, 'w': bw, 'h': bh,
                'fill': '#f0f0f0',
                'stroke': '#cccccc',
                'strokeWidth': 0.5
            }
        
        return {
            'type': 'image',
            'x': bx,
            'y': by,
            'w': bw,
            'h': bh,
            'dataUrl': data_url
        }
    
    def _parse_path_object(self, el, elements, parent_ctm):
        """解析路径对象 -> 可能产生 path/rect/line 等元素"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        # 绘制参数
        draw_param = {}
        dp_ref = _attr(el, 'DrawParam', '')
        if dp_ref:
            draw_param = self._resolve_draw_param(dp_ref)
        
        # 直接子绘制参数
        fill_el = _find(el, 'FillColor')
        stroke_el = _find(el, 'StrokeColor')
        if fill_el is not None:
            draw_param['fillColor'] = _parse_color(_attr(fill_el, 'ColorValue', ''))
        if stroke_el is not None:
            draw_param['strokeColor'] = _parse_color(_attr(stroke_el, 'ColorValue', ''))
        
        line_width_el = _find(el, 'LineWidth')
        if line_width_el is not None:
            draw_param['lineWidth'] = float(_attr(line_width_el, 'Value', '0') or 0)
        
        fill = draw_param.get('fillColor')
        stroke = draw_param.get('strokeColor')
        line_width = draw_param.get('lineWidth', 0)
        stroke_width = line_width if line_width > 0 else 0
        
        # 判断是否填充/描边
        stroke_enabled = _attr(el, 'Stroke', 'false') == 'true'
        fill_enabled = _attr(el, 'Fill', 'false') == 'true'
        if not stroke_enabled:
            stroke_el = _find(el, 'Stroke')
            if stroke_el is not None:
                stroke_enabled = (stroke_el.text or '').strip() == 'true'
        if not fill_enabled:
            fill_el_check = _find(el, 'Fill')
            if fill_el_check is not None:
                fill_enabled = (fill_el_check.text or '').strip() == 'true'
        rule = _attr(el, 'Rule', 'NonZero')  # EvenOdd 或 NonZero
        
        if not stroke_enabled:
            stroke = None
        if not fill_enabled:
            fill = None
        
        # 路径数据
        abbr = _find(el, 'AbbreviatedData')
        if abbr is not None and abbr.text:
            path_data = abbr.text.strip()
            # 尝试渲染为 SVG path
            elements.append({
                'type': 'path',
                'd': path_data,
                'x': bx, 'y': by, 'w': bw, 'h': bh,
                'fill': fill,
                'stroke': stroke,
                'strokeWidth': stroke_width,
                'rule': rule,
                'boundary': _attr(el, 'Boundary', '')
            })
    
    def _parse_rect(self, el, parent_ctm):
        """解析矩形"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        fill_el = _find(el, 'FillColor')
        stroke_el = _find(el, 'StrokeColor')
        
        # Check Fill/Stroke flags as child elements
        fill_enabled = _attr(el, 'Fill', 'false') == 'true'
        if not fill_enabled:
            fill_flag = _find(el, 'Fill')
            if fill_flag is not None:
                fill_enabled = (fill_flag.text or '').strip() == 'true'
        stroke_enabled = _attr(el, 'Stroke', 'false') == 'true'
        if not stroke_enabled:
            stroke_flag = _find(el, 'Stroke')
            if stroke_flag is not None:
                stroke_enabled = (stroke_flag.text or '').strip() == 'true'
        
        dp_ref = _attr(el, 'DrawParam', '')
        fill = None
        stroke = None
        line_width = 0
        
        if dp_ref:
            dp = self._resolve_draw_param(dp_ref)
            fill = dp.get('fillColor')
            stroke = dp.get('strokeColor')
            line_width = dp.get('lineWidth', 0)
        
        if fill_el is not None:
            fill = _parse_color(_attr(fill_el, 'ColorValue', ''))
        if stroke_el is not None:
            stroke = _parse_color(_attr(stroke_el, 'ColorValue', ''))
        
        if not fill_enabled:
            fill = None
        if not stroke_enabled:
            stroke = None
        
        return {
            'type': 'rect',
            'x': bx, 'y': by, 'w': bw, 'h': bh,
            'fill': fill,
            'stroke': stroke or (None if not fill else '#000000'),
            'strokeWidth': line_width if line_width > 0 else 0.5
        }
    
    def _parse_line(self, el, parent_ctm):
        """解析直线"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        p1_str = _attr(el, 'Point1', '')
        p2_str = _attr(el, 'Point2', '')
        if not p1_str:
            p1_el = _find(el, 'Point1')
            if p1_el is not None and p1_el.text:
                p1_str = p1_el.text.strip()
        if not p2_str:
            p2_el = _find(el, 'Point2')
            if p2_el is not None and p2_el.text:
                p2_str = p2_el.text.strip()
        
        if p1_str and p2_str:
            p1 = list(map(float, p1_str.strip().split()))
            p2 = list(map(float, p2_str.strip().split()))
            if len(p1) >= 2 and len(p2) >= 2:
                # 转换 Boundary 相对坐标到绝对坐标
                x1, y1 = bx + p1[0], by + p1[1]
                x2, y2 = bx + p2[0], by + p2[1]
            else:
                x1, y1 = bx, by
                x2, y2 = bx + bw, by + bh
        else:
            x1, y1 = bx, by
            x2, y2 = bx + bw, by + bh
        
        stroke_el = _find(el, 'StrokeColor')
        color = None
        if stroke_el is not None:
            color = _parse_color(_attr(stroke_el, 'ColorValue', ''))
        if not color:
            color = '#000000'
        
        return {
            'type': 'line',
            'x1': x1, 'y1': y1,
            'x2': x2, 'y2': y2,
            'stroke': color,
            'strokeWidth': 0.5
        }
    
    def _parse_circle(self, el, parent_ctm):
        """解析圆"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        center_str = _attr(el, 'Center', '')
        if center_str:
            center = list(map(float, center_str.strip().split()))
            cx, cy = bx + center[0], by + center[1]
        else:
            cx, cy = bx + bw / 2, by + bh / 2
        
        r = float(_attr(el, 'Radius', str(min(bw, bh) / 2)))
        
        fill_el = _find(el, 'FillColor')
        stroke_el = _find(el, 'StrokeColor')
        fill = _parse_color(_attr(fill_el, 'ColorValue', '')) if fill_el is not None else None
        stroke = _parse_color(_attr(stroke_el, 'ColorValue', '')) if stroke_el is not None else '#000000'
        
        return {
            'type': 'circle',
            'cx': bx + cx if center_str else cx,
            'cy': by + cy if center_str else cy,
            'r': r,
            'fill': fill,
            'stroke': stroke,
            'strokeWidth': 0.5
        }
    
    def _parse_ellipse(self, el, parent_ctm):
        """解析椭圆"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        center_str = _attr(el, 'Center', '')
        if center_str:
            center = list(map(float, center_str.strip().split()))
            cx, cy = bx + center[0], by + center[1]
        else:
            cx, cy = bx + bw / 2, by + bh / 2
        
        fill_el = _find(el, 'FillColor')
        stroke_el = _find(el, 'StrokeColor')
        fill = _parse_color(_attr(fill_el, 'ColorValue', '')) if fill_el is not None else None
        stroke = _parse_color(_attr(stroke_el, 'ColorValue', '')) if stroke_el is not None else '#000000'
        
        return {
            'type': 'ellipse',
            'cx': cx, 'cy': cy,
            'rx': bw / 2, 'ry': bh / 2,
            'fill': fill,
            'stroke': stroke,
            'strokeWidth': 0.5
        }
    
    def _parse_arc(self, el, parent_ctm):
        """解析弧"""
        boundary = _parse_boundary(_attr(el, 'Boundary', ''))
        bx, by, bw, bh = boundary
        
        center_str = _attr(el, 'Center', '')
        start_angle = float(_attr(el, 'StartAngle', '0') or 0)
        end_angle = float(_attr(el, 'EndAngle', '360') or 360)
        
        if center_str:
            center = list(map(float, center_str.strip().split()))
            cx, cy = bx + center[0], by + center[1]
        else:
            cx, cy = bx + bw / 2, by + bh / 2
        
        rx = float(_attr(el, 'SemiAxisX', str(bw / 2)) or (bw / 2))
        ry = float(_attr(el, 'SemiAxisY', str(bh / 2)) or (bh / 2))
        
        # Convert start/end angles to radians
        import math
        sa = math.radians(start_angle)
        ea = math.radians(end_angle)
        
        x1 = cx + rx * math.cos(sa)
        y1 = cy + ry * math.sin(sa)
        x2 = cx + rx * math.cos(ea)
        y2 = cy + ry * math.sin(ea)
        
        large_arc = 1 if (end_angle - start_angle) % 360 > 180 else 0
        sweep = 1 if end_angle > start_angle else 0
        
        path_d = f"M {x1} {y1} A {rx} {ry} 0 {large_arc} {sweep} {x2} {y2}"
        
        stroke_el = _find(el, 'StrokeColor')
        color = _parse_color(_attr(stroke_el, 'ColorValue', '')) if stroke_el is not None else '#000000'
        
        return {
            'type': 'path',
            'd': path_d,
            'x': bx, 'y': by, 'w': bw, 'h': bh,
            'fill': 'none',
            'stroke': color,
            'strokeWidth': 0.5
        }


def main():
    if len(sys.argv) < 2:
        print(json.dumps({'success': False, 'error': '缺少参数'}))
        return
    
    cmd = sys.argv[1]
    params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    
    if cmd == 'ofd-parse':
        filepath = params.get('file', '')
        if not filepath:
            print(json.dumps({'success': False, 'error': '请指定 OFD 文件路径'}))
            return
        if not os.path.exists(filepath):
            print(json.dumps({'success': False, 'error': f'文件不存在: {filepath}'}))
            return
        
        parser = OFDParser(filepath)
        result = parser.parse()
        print(json.dumps(result))
    elif cmd == 'ofd-info':
        """快速获取 OFD 文件基本信息（不解析页面内容）"""
        filepath = params.get('file', '')
        if not filepath:
            print(json.dumps({'success': False, 'error': '请指定 OFD 文件路径'}))
            return
        try:
            with zipfile.ZipFile(filepath, 'r') as zf:
                files = zf.namelist()
                print(json.dumps({
                    'success': True,
                    'file': os.path.basename(filepath),
                    'size': os.path.getsize(filepath),
                    'entries': len(files),
                    'files': files[:50],  # 前 50 个文件
                    'total': len(files)
                }))
        except Exception as e:
            print(json.dumps({'success': False, 'error': str(e)}))
    else:
        print(json.dumps({'success': False, 'error': f'未知命令: {cmd}'}))


if __name__ == '__main__':
    main()
