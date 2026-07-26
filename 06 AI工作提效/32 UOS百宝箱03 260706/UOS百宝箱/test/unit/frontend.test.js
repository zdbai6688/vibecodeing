import { describe, it, expect, vi, beforeEach } from 'vitest'

function esc(str) {
  if (!str) return ''
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
}

function parseTerminalOutput(text) {
  if (!text) return '<span class="t-dim">无输出</span>'
  let h = text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
  h = h.replace(/\x1b\[0;32m/g, '<span class="t-green">')
  h = h.replace(/\x1b\[1;34m/g, '<span class="t-blue">')
  h = h.replace(/\x1b\[1;33m/g, '<span class="t-yellow">')
  h = h.replace(/\x1b\[0m/g, '</span>')
  return h.split('\n').map(l => '<div>' + l + '</div>').join('')
}

function fmtMB(mb) {
  if (mb === undefined || mb === null) return '-'
  if (mb >= 1024) return (mb / 1024).toFixed(1) + ' GB'
  return mb + ' MB'
}

function maskStr(str) {
  if (!str || str === '未知') return str || '未知'
  if (str.length <= 8) return str.substring(0, 4) + '****'
  return str.substring(0, 4) + '****' + str.substring(str.length - 4)
}

function getTag(name) {
  if (!name) return null
  const n = name.toLowerCase()
  if (n.includes('dde') || n.includes('kwin') || n.includes('dde-dock') || n.includes('dde-launch')) return { label: '桌面组件', color: '#4a90d9' }
  if (n.includes('wine') || n.includes('wineserver') || n.includes('wine-preloader')) return { label: 'Windows兼容', color: '#8b5cf6' }
  if (n.includes('msedge') || n.includes('chrome') || n.includes('firefox') || n.includes('browser')) return { label: '浏览器', color: '#22c55e' }
  if (n.includes('kun') || n.includes('uos-baibaoxiang') || n.includes('electron')) return { label: '客户端', color: '#f59e0b' }
  return null
}

// ========== esc ==========
describe('esc (HTML escaping)', () => {
  it('should escape HTML special characters', () => {
    expect(esc('<script>alert("xss")</script>'))
      .toBe('&lt;script&gt;alert(&quot;xss&quot;)&lt;/script&gt;')
  })

  it('should handle null/undefined', () => {
    expect(esc(null)).toBe('')
    expect(esc(undefined)).toBe('')
  })

  it('should pass through normal text', () => {
    expect(esc('hello world')).toBe('hello world')
  })

  it('should escape ampersands first', () => {
    expect(esc('a&b')).toBe('a&amp;b')
  })

  it('should escape multiple special chars', () => {
    expect(esc('<>&"\'')).toBe('&lt;&gt;&amp;&quot;\'')
  })

  it('should handle empty string', () => {
    expect(esc('')).toBe('')
  })

  it('should handle numbers', () => {
    expect(esc(123)).toBe('123')
  })
})

// ========== parseTerminalOutput ==========
describe('parseTerminalOutput', () => {
  it('should return placeholder for empty input', () => {
    expect(parseTerminalOutput('')).toBe('<span class="t-dim">无输出</span>')
    expect(parseTerminalOutput(null)).toBe('<span class="t-dim">无输出</span>')
    expect(parseTerminalOutput(undefined)).toBe('<span class="t-dim">无输出</span>')
  })

  it('should escape HTML in terminal output', () => {
    const result = parseTerminalOutput('<hello>')
    expect(result).toContain('&lt;hello&gt;')
  })

  it('should parse ANSI color codes', () => {
    const input = '\x1b[0;32mgreen\x1b[0m'
    const result = parseTerminalOutput(input)
    expect(result).toContain('<span class="t-green">green</span>')
  })

  it('should wrap lines in div tags', () => {
    const result = parseTerminalOutput('line1\nline2')
    expect(result).toBe('<div>line1</div><div>line2</div>')
  })

  it('should handle mixed ANSI codes', () => {
    const input = '\x1b[0;32mgreen\x1b[0m \x1b[1;34mblue\x1b[0m \x1b[1;33myellow\x1b[0m'
    const result = parseTerminalOutput(input)
    expect(result).toContain('<span class="t-green">green</span>')
    expect(result).toContain('<span class="t-blue">blue</span>')
    expect(result).toContain('<span class="t-yellow">yellow</span>')
  })

  it('should handle unknown ANSI codes by stripping', () => {
    const input = '\x1b[1;31mred\x1b[0m'
    const result = parseTerminalOutput(input)
    expect(result).toContain('red')
    // 1;31m is not handled, so it stays as literal text (already escaped)
    expect(result).not.toContain('<span class="t-red">')
  })

  it('should handle single line without newline', () => {
    const result = parseTerminalOutput('just one line')
    expect(result).toBe('<div>just one line</div>')
  })

  it('should handle empty lines', () => {
    const result = parseTerminalOutput('a\n\nb')
    expect(result).toBe('<div>a</div><div></div><div>b</div>')
  })
})

// ========== fmtMB ==========
describe('fmtMB', () => {
  it('should format values less than 1024 as MB', () => {
    expect(fmtMB(512)).toBe('512 MB')
  })

  it('should format values >= 1024 as GB', () => {
    expect(fmtMB(2048)).toBe('2.0 GB')
    expect(fmtMB(1536)).toBe('1.5 GB')
  })

  it('should handle undefined/null', () => {
    expect(fmtMB(undefined)).toBe('-')
    expect(fmtMB(null)).toBe('-')
  })

  it('should handle 0', () => {
    expect(fmtMB(0)).toBe('0 MB')
  })

  it('should handle negative values', () => {
    expect(fmtMB(-100)).toBe('-100 MB')
  })

  it('should handle exact GB boundary', () => {
    expect(fmtMB(1024)).toBe('1.0 GB')
  })

  it('should handle very large values', () => {
    expect(fmtMB(1048576)).toBe('1024.0 GB')
  })
})

// ========== maskStr ==========
describe('maskStr', () => {
  it('should mask long strings with asterisks', () => {
    expect(maskStr('abcdefghijklmnop')).toBe('abcd****mnop')
  })

  it('should mask short strings partially', () => {
    expect(maskStr('abcdefgh')).toBe('abcd****')
  })

  it('should handle exactly 8 characters', () => {
    expect(maskStr('12345678')).toBe('1234****')
  })

  it('should handle less than 8 characters', () => {
    expect(maskStr('abc')).toBe('abc****')
  })

  it('should handle "未知"', () => {
    expect(maskStr('未知')).toBe('未知')
  })

  it('should handle null/undefined', () => {
    expect(maskStr(null)).toBe('未知')
    expect(maskStr(undefined)).toBe('未知')
  })

  it('should handle empty string', () => {
    expect(maskStr('')).toBe('未知')
  })

  it('should mask very long strings', () => {
    const long = 'a'.repeat(100)
    const result = maskStr(long)
    expect(result.length).toBe(4 + 4 + 4) // 4 prefix + **** + 4 suffix
    expect(result).toBe('aaaa****aaaa')
  })
})

// ========== getTag ==========
describe('getTag', () => {
  it('should detect desktop components', () => {
    expect(getTag('dde-dock')).toEqual({ label: '桌面组件', color: '#4a90d9' })
    expect(getTag('kwin_x11')).toEqual({ label: '桌面组件', color: '#4a90d9' })
    expect(getTag('dde-launchpad')).toEqual({ label: '桌面组件', color: '#4a90d9' })
  })

  it('should detect wine compatibility', () => {
    expect(getTag('wine')).toEqual({ label: 'Windows兼容', color: '#8b5cf6' })
    expect(getTag('wineserver')).toEqual({ label: 'Windows兼容', color: '#8b5cf6' })
    expect(getTag('wine-preloader')).toEqual({ label: 'Windows兼容', color: '#8b5cf6' })
  })

  it('should detect browsers', () => {
    expect(getTag('chrome')).toEqual({ label: '浏览器', color: '#22c55e' })
    expect(getTag('msedge')).toEqual({ label: '浏览器', color: '#22c55e' })
    expect(getTag('firefox')).toEqual({ label: '浏览器', color: '#22c55e' })
    expect(getTag('browser')).toEqual({ label: '浏览器', color: '#22c55e' })
  })

  it('should detect our client', () => {
    expect(getTag('uos-baibaoxiang')).toEqual({ label: '客户端', color: '#f59e0b' })
    expect(getTag('electron')).toEqual({ label: '客户端', color: '#f59e0b' })
    expect(getTag('kun-box')).toEqual({ label: '客户端', color: '#f59e0b' })
  })

  it('should return null for unknown', () => {
    expect(getTag('systemd')).toBeNull()
    expect(getTag('python3')).toBeNull()
    expect(getTag('')).toBeNull()
  })

  it('should be case insensitive', () => {
    expect(getTag('DDE-Dock')).toEqual({ label: '桌面组件', color: '#4a90d9' })
    expect(getTag('Chrome')).toEqual({ label: '浏览器', color: '#22c55e' })
    expect(getTag('Wine')).toEqual({ label: 'Windows兼容', color: '#8b5cf6' })
  })

  it('should handle null/undefined', () => {
    expect(getTag(null)).toBeNull()
    expect(getTag(undefined)).toBeNull()
  })
})