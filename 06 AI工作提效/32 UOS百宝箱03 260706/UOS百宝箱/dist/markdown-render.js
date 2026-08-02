/* UOS百宝箱 - Markdown 渲染器
 * 轻量级客户端 Markdown → HTML，支持标题/列表/代码块/表格/引用/任务列表等。
 * 浏览器中挂载为 window.renderMarkdown；Node 测试环境通过 module.exports 导出。
 */
(function (global) {
  'use strict'

  function esc(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
  }

  // 行内格式化（输入为已 HTML 转义的文本）
  function inline(src) {
    let s = String(src)
    // 行内代码（先处理，避免被后续正则破坏）
    s = s.replace(/`([^`]+)`/g, '<code>$1</code>')
    // 图片 ![alt](url)
    s = s.replace(/!\[([^\]]*)\]\(([^)\s]+)\)/g, '<img src="$2" alt="$1" style="max-width:100%">')
    // 链接 [text](url)
    s = s.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>')
    // 加粗
    s = s.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    // 删除线
    s = s.replace(/~~([^~]+)~~/g, '<del>$1</del>')
    // 斜体（避免与加粗冲突）
    s = s.replace(/(^|[^*])\*([^*\s][^*]*)\*(?!\*)/g, '$1<em>$2</em>')
    s = s.replace(/(^|[^_])_([^_\s][^_]*)_(?!_)/g, '$1<em>$2</em>')
    return s
  }

  function parseTableRow(line) {
    return line
      .replace(/^\s*\|/, '')
      .replace(/\|\s*$/, '')
      .split('|')
      .map(function (c) { return c.trim() })
  }

  function parseAlign(sepRow, colCount) {
    const cells = parseTableRow(sepRow)
    const aligns = []
    for (let i = 0; i < colCount; i++) {
      const c = (cells[i] || '').trim()
      if (/^:.*:$/.test(c)) aligns.push('center')
      else if (/:-+$/.test(c)) aligns.push('right')
      else if (/^-+:/.test(c)) aligns.push('left')
      else aligns.push('left')
    }
    return aligns
  }

  function renderMarkdown(md) {
    if (md == null) return ''
    const text = String(md).replace(/\r\n?/g, '\n')
    const lines = text.split('\n')
    let out = ''
    let i = 0
    let para = []

    function flushPara() {
      if (para.length) {
        out += '<p>' + para.map(inline).join('<br>') + '</p>'
        para = []
      }
    }

    while (i < lines.length) {
      const line = lines[i]
      const t = line.trim()

      // 围栏代码块 ```lang
      const fence = t.match(/^```(\w*)/)
      if (fence) {
        flushPara()
        const lang = fence[1] || ''
        i++
        const code = []
        while (i < lines.length && !/^```/.test(lines[i].trim())) {
          code.push(lines[i])
          i++
        }
        i++ // 跳过结束围栏
        out += '<pre><code' + (lang ? ' class="language-' + lang + '"' : '') + '>' + esc(code.join('\n')) + '</code></pre>'
        continue
      }

      // 标题
      const h = t.match(/^(#{1,6})\s+(.*)$/)
      if (h) {
        flushPara()
        const lv = h[1].length
        out += '<h' + lv + '>' + inline(esc(h[2])) + '</h' + lv + '>'
        i++
        continue
      }

      // 分隔线
      if (/^(\s*([-*_])\s*){3,}$/.test(t)) {
        flushPara()
        out += '<hr>'
        i++
        continue
      }

      // 引用
      if (t.indexOf('>') === 0) {
        flushPara()
        const q = []
        while (i < lines.length && lines[i].trim().indexOf('>') === 0) {
          q.push(lines[i].trim().replace(/^>\s?/, ''))
          i++
        }
        out += '<blockquote>' + renderMarkdown(q.join('\n')) + '</blockquote>'
        continue
      }

      // 表格：当前行以 | 开头且下一行是对齐分隔行
      if (t.indexOf('|') === 0 && i + 1 < lines.length && /^[\s|:-]+$/.test(lines[i + 1].trim()) && lines[i + 1].indexOf('-') !== -1) {
        flushPara()
        const headers = parseTableRow(t)
        const aligns = parseAlign(lines[i + 1], headers.length)
        i += 2
        const rows = []
        while (i < lines.length && lines[i].trim().indexOf('|') === 0) {
          rows.push(parseTableRow(lines[i]))
          i++
        }
        out += '<table style="border-collapse:collapse;width:100%"><thead><tr>'
        out += headers.map(function (h, j) {
          return '<th style="border:1px solid #d0d7de;padding:6px 10px;text-align:' + aligns[j] + ';background:#f6f8fa">' + inline(esc(h)) + '</th>'
        }).join('')
        out += '</tr></thead><tbody>'
        out += rows.map(function (r) {
          return '<tr>' + r.map(function (c, j) {
            return '<td style="border:1px solid #d0d7de;padding:6px 10px;text-align:' + (aligns[j] || 'left') + '">' + inline(esc(c)) + '</td>'
          }).join('') + '</tr>'
        }).join('')
        out += '</tbody></table>'
        continue
      }

      // 无序/有序列表（含任务列表）
      const ul = t.match(/^([-*+])\s+(.*)$/)
      const ol = t.match(/^(\d+)[.)]\s+(.*)$/)
      if (ul || ol) {
        flushPara()
        const ordered = !!ol
        const tag = ordered ? 'ol' : 'ul'
        out += '<' + tag + '>'
        while (i < lines.length) {
          const lt = lines[i].trim()
          const m = ordered ? lt.match(/^(\d+)[.)]\s+(.*)$/) : lt.match(/^([-*+])\s+(.*)$/)
          if (!m) break
          const task = m[2].match(/^\[([ xX])\]\s+(.*)$/)
          let content
          if (task) {
            const checked = task[1].toLowerCase() === 'x'
            content = '<input type="checkbox" disabled' + (checked ? ' checked' : '') + '> ' + inline(esc(task[2]))
          } else {
            content = inline(esc(m[2]))
          }
          out += '<li style="margin:2px 0">' + content + '</li>'
          i++
        }
        out += '</' + tag + '>'
        continue
      }

      // 空行
      if (t === '') {
        flushPara()
        i++
        continue
      }

      // 普通段落（先转义 HTML，再交给行内格式化）
      para.push(esc(t))
      i++
    }
    flushPara()
    return out
  }

  const api = { renderMarkdown: renderMarkdown, esc: esc }
  if (typeof module !== 'undefined' && module.exports) module.exports = api
  if (typeof window !== 'undefined') {
    window.renderMarkdown = renderMarkdown
    window.mdEscapeHtml = esc
  }
})(typeof globalThis !== 'undefined' ? globalThis : this)
