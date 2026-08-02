import { describe, it, expect } from 'vitest'
import { renderMarkdown, esc } from '../../dist/markdown-render.js'

describe('markdown-render.js - esc', () => {
  it('should escape HTML special characters', () => {
    expect(esc('<script>alert("x")</script>')).toBe('&lt;script&gt;alert(&quot;x&quot;)&lt;/script&gt;')
  })
  it('should handle null/undefined/empty', () => {
    expect(esc(null)).toBe('')
    expect(esc(undefined)).toBe('')
    expect(esc('')).toBe('')
  })
})

describe('markdown-render.js - 标题/段落', () => {
  it('should render headings h1-h6', () => {
    expect(renderMarkdown('# H1')).toBe('<h1>H1</h1>')
    expect(renderMarkdown('###### H6')).toBe('<h6>H6</h6>')
  })
  it('should render paragraphs', () => {
    expect(renderMarkdown('hello world')).toBe('<p>hello world</p>')
  })
  it('should treat consecutive lines as paragraph with <br>', () => {
    expect(renderMarkdown('line1\nline2')).toBe('<p>line1<br>line2</p>')
  })
  it('should handle empty input', () => {
    expect(renderMarkdown('')).toBe('')
    expect(renderMarkdown(null)).toBe('')
    expect(renderMarkdown(undefined)).toBe('')
  })
})

describe('markdown-render.js - 行内格式', () => {
  it('bold', () => {
    expect(renderMarkdown('**bold**')).toContain('<strong>bold</strong>')
  })
  it('italic', () => {
    expect(renderMarkdown('*italic*')).toContain('<em>italic</em>')
  })
  it('inline code', () => {
    expect(renderMarkdown('`code`')).toContain('<code>code</code>')
  })
  it('strikethrough', () => {
    expect(renderMarkdown('~~del~~')).toContain('<del>del</del>')
  })
  it('link', () => {
    expect(renderMarkdown('[text](https://a.b)')).toContain('<a href="https://a.b" target="_blank" rel="noopener">text</a>')
  })
  it('image', () => {
    expect(renderMarkdown('![alt](img.png)')).toContain('<img src="img.png" alt="alt"')
  })
  it('should escape raw HTML in source', () => {
    expect(renderMarkdown('<script>alert(1)</script>')).not.toContain('<script>alert')
  })
})

describe('markdown-render.js - 列表/引用/分隔线', () => {
  it('unordered list', () => {
    const out = renderMarkdown('- a\n- b')
    expect(out).toContain('<ul>')
    expect(out).toContain('<li style="margin:2px 0">a</li>')
    expect(out).toContain('<li style="margin:2px 0">b</li>')
    expect(out).toContain('</ul>')
  })
  it('ordered list', () => {
    const out = renderMarkdown('1. first\n2. second')
    expect(out).toContain('<ol>')
    expect(out).toContain('first')
    expect(out).toContain('</ol>')
  })
  it('task list checked/unchecked', () => {
    const out = renderMarkdown('- [x] done\n- [ ] todo')
    expect(out).toContain('<input type="checkbox" disabled checked>')
    expect(out).toContain('<input type="checkbox" disabled>')
  })
  it('blockquote', () => {
    expect(renderMarkdown('> quote')).toContain('<blockquote><p>quote</p></blockquote>')
  })
  it('horizontal rule', () => {
    expect(renderMarkdown('---')).toContain('<hr>')
  })
})

describe('markdown-render.js - 代码块/表格', () => {
  it('fenced code block', () => {
    const out = renderMarkdown('```js\nconst a = 1\n```')
    expect(out).toContain('<pre><code class="language-js">')
    expect(out).toContain('const a = 1')
    expect(out).toContain('</code></pre>')
  })
  it('table', () => {
    const out = renderMarkdown('| A | B |\n|---|---|\n| 1 | 2 |')
    expect(out).toContain('<table')
    expect(out).toContain('<th')
    expect(out).toContain('<td style="border:1px solid #d0d7de;padding:6px 10px;text-align:left">1</td>')
    expect(out).toContain('</table>')
  })
})
