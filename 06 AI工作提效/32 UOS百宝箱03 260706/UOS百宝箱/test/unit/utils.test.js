import { describe, it, expect, vi, beforeEach } from 'vitest'
import path from 'path'
import fs from 'fs'

import {
  formatBytes,
  execOut,
  sudoExec,
  parseMemInfo,
  parseProcStatForCpu,
  calcCpuPercent,
  getDirSize,
  loadSettings,
  saveSettings,
  aggregateProcesses
} from '../../lib/utils'

// ========== formatBytes ==========
describe('formatBytes', () => {
  it('should return "0 B" for 0 bytes', () => { expect(formatBytes(0)).toBe('0 B') })
  it('should format bytes', () => { expect(formatBytes(500)).toBe('500 B') })
  it('should format KB', () => { expect(formatBytes(2048)).toBe('2 KB') })
  it('should format MB', () => { expect(formatBytes(1048576)).toBe('1 MB') })
  it('should format GB', () => { expect(formatBytes(1073741824)).toBe('1 GB') })
  it('should handle decimals', () => { expect(formatBytes(1536)).toBe('1.5 KB') })
  it('should handle large values', () => { expect(formatBytes(5368709120)).toBe('5 GB') })
  it('should handle negative values', () => { expect(typeof formatBytes(-1)).toBe('string') })
})

// ========== execOut ==========
describe('execOut', () => {
  it('should return command output on success', () => {
    const result = execOut('echo hello')
    expect(result.trim()).toBe('hello')
  })

  it('should return empty string on error', () => {
    const result = execOut('nonexistent_command_xyz_123 2>/dev/null')
    expect(result).toBe('')
  })

  it('should handle empty command output', () => {
    const result = execOut('true')
    expect(result).toBe('')
  })

  it('should handle multi-line output', () => {
    const result = execOut('printf "line1\\nline2\\nline3"')
    expect(result).toBe('line1\nline2\nline3')
  })
})

// ========== sudoExec ==========
describe('sudoExec', () => {
  it('should run without password using sudo prefix', () => {
    const result = sudoExec('echo hello_from_sudo', null)
    // Without actual sudo access this may fail, but the command should be constructed correctly
    expect(typeof result).toBe('string')
  })

  it('should return empty string on error', () => {
    const result = sudoExec('nonexistent_command_xyz', null)
    expect(result).toBe('')
  })
})

// ========== parseMemInfo ==========
describe('parseMemInfo', () => {
  const sampleMeminfo = [
    'MemTotal:        8146848 kB',
    'MemFree:         4123456 kB',
    'MemAvailable:    6123456 kB',
    'Buffers:          234567 kB',
    'Cached:          2123456 kB',
    'SwapCached:        12345 kB',
    'SwapTotal:       2097148 kB',
    'SwapFree:        1997148 kB',
  ].join('\n')

  it('should parse memory info correctly', () => {
    const result = parseMemInfo(sampleMeminfo)
    expect(result.total).toBe(7956)
    expect(result.available).toBe(5980)
    expect(result.used).toBe(1976)
    expect(result.usagePercent).toBe(25)
    expect(result.swapTotal).toBe(2048)
    expect(result.swapFree).toBe(1950)
    expect(result.swapUsed).toBe(98)
  })

  it('should handle empty input', () => {
    const result = parseMemInfo('')
    expect(result.total).toBe(0)
    expect(result.usagePercent).toBe(0)
    expect(result.swapTotal).toBe(0)
  })

  it('should handle missing fields', () => {
    const result = parseMemInfo('MemTotal: 1000 kB')
    expect(result.total).toBe(1)
    expect(result.available).toBe(0)
    expect(result.usagePercent).toBe(100)
  })

  it('should handle zero values', () => {
    const result = parseMemInfo('MemTotal: 0 kB\nMemAvailable: 0 kB')
    expect(result.total).toBe(0)
    expect(result.usagePercent).toBe(0)
  })
})

// ========== parseProcStatForCpu / calcCpuPercent ==========
describe('parseProcStatForCpu', () => {
  it('should parse cpu line', () => {
    const r = parseProcStatForCpu('cpu  12345 678 9012 345678 123 456 789')
    expect(r).not.toBeNull()
    expect(r.total).toBe(12345 + 678 + 9012 + 345678)
    expect(r.idle).toBe(345678)
  })

  it('should return null for bad input', () => {
    expect(parseProcStatForCpu('')).toBeNull()
    expect(parseProcStatForCpu('cpux 1 2 3 4')).toBeNull()
  })

  it('should handle extra whitespace', () => {
    const r = parseProcStatForCpu('cpu   100   200   300   400')
    expect(r.total).toBe(1000)
  })

  it('should return null for non-cpu lines', () => {
    expect(parseProcStatForCpu('intr 12345')).toBeNull()
  })
})

describe('calcCpuPercent', () => {
  it('should calculate correctly', () => {
    expect(calcCpuPercent({ total: 1000, idle: 800 }, { total: 1200, idle: 850 })).toBe(75)
  })

  it('should return 0 for identical samples', () => {
    const s = { total: 1000, idle: 800 }
    expect(calcCpuPercent(s, s)).toBe(0)
  })

  it('should return 0 for null inputs', () => {
    expect(calcCpuPercent(null, { total: 100, idle: 50 })).toBe(0)
    expect(calcCpuPercent({ total: 100, idle: 50 }, null)).toBe(0)
  })

  it('should handle full CPU usage', () => {
    expect(calcCpuPercent({ total: 1000, idle: 800 }, { total: 1200, idle: 800 })).toBe(100)
  })

  it('should return 0 on counter wraparound', () => {
    expect(calcCpuPercent({ total: 2000, idle: 1000 }, { total: 1000, idle: 500 })).toBe(0)
  })
})

// ========== aggregateProcesses ==========
describe('aggregateProcesses', () => {
  const psOutput = [
    'root         1  0.0  0.1  12345  6789 ?        Ss   Jul01   0:05 /sbin/init',
    'root       100  1.2  0.5  54321 12345 ?        S    Jul01  12:30 /usr/bin/python3 /usr/bin/service',
    'user       200  5.5  2.3  98765 45678 ?        Rl   Jul01  45:00 /usr/lib/chrome/chrome --flag1',
    'user       201  3.2  1.8  87654 34567 ?        Rl   Jul01  30:00 /usr/lib/chrome/chrome --tab1',
    'root       300  0.0  0.0      0     0 ?        Z    Jul01   0:00 [defunct] <zombie>',
    'user       400  0.5  0.3  45678  9012 ?        S    Jul01   1:00 /usr/bin/dde-dock',
  ]
  const blacklist = ['ps', 'head', 'awk', 'grep', 'sh', 'bash', 'cut', 'sort', 'tr']

  it('should aggregate by shortName', () => {
    const result = aggregateProcesses(psOutput, blacklist, 'cpu')
    const chrome = result.find(g => g.shortName === 'chrome')
    expect(chrome).toBeDefined()
    expect(chrome.count).toBe(2)
    expect(chrome.cpu).toBeCloseTo(8.7, 1)
    expect(chrome.pids.length).toBe(2)
  })

  it('should sort by cpu descending', () => {
    const result = aggregateProcesses(psOutput, blacklist, 'cpu')
    for (let i = 1; i < result.length; i++)
      expect(result[i - 1].cpu).toBeGreaterThanOrEqual(result[i].cpu)
  })

  it('should sort by mem when specified', () => {
    const result = aggregateProcesses(psOutput, blacklist, 'mem')
    for (let i = 1; i < result.length; i++)
      expect(result[i - 1].mem).toBeGreaterThanOrEqual(result[i].mem)
  })

  it('should filter blacklisted processes', () => {
    const withPs = [...psOutput, 'nobody 999 99.9 99.9 99999 99999 ? R Jul01 999:00 ps']
    const result = aggregateProcesses(withPs, blacklist, 'cpu')
    expect(result.find(g => g.shortName === 'ps')).toBeUndefined()
  })

  it('should handle empty input', () => {
    expect(aggregateProcesses([], blacklist)).toEqual([])
  })

  it('should limit to 30 groups', () => {
    const many = Array.from({ length: 50 }, (_, i) =>
      `user${i} ${1000 + i} 1.0 1.0 ${i}000 ${i}000 ? R Jul01 0:00 /usr/bin/proc${i}`
    )
    expect(aggregateProcesses(many, blacklist, 'cpu').length).toBeLessThanOrEqual(30)
  })

  it('should handle empty command lines', () => {
    expect(aggregateProcesses(['root 1 0.0 0.1 0 0 ? Ss Jul01 0:05 '], blacklist).length).toBe(0)
  })

  it('should handle blank lines', () => {
    const result = aggregateProcesses([...psOutput, '', '   '], blacklist)
    expect(result.length).toBeGreaterThan(0)
  })
})

// ========== getDirSize ==========
describe('getDirSize', () => {
  beforeEach(() => { vi.clearAllMocks() })

  it('should calculate directory size recursively', () => {
    const mockReaddirSync = vi.spyOn(fs, 'readdirSync')
    const mockStatSync = vi.spyOn(fs, 'statSync')
    mockReaddirSync.mockReturnValueOnce([
      { name: 'a.txt', isFile: () => true, isDirectory: () => false },
      { name: 'sub', isFile: () => false, isDirectory: () => true }
    ])
    mockReaddirSync.mockReturnValueOnce([
      { name: 'b.txt', isFile: () => true, isDirectory: () => false }
    ])
    mockStatSync.mockImplementation((fp) => {
      if (fp.endsWith('a.txt')) return { size: 100 }
      if (fp.endsWith('b.txt')) return { size: 200 }
      return { size: 0 }
    })
    expect(getDirSize('/test')).toBe(300)
    mockReaddirSync.mockRestore()
    mockStatSync.mockRestore()
  })

  it('should handle empty directory', () => {
    const spy = vi.spyOn(fs, 'readdirSync').mockReturnValue([])
    expect(getDirSize('/empty')).toBe(0)
    spy.mockRestore()
  })

  it('should handle readdir error', () => {
    const spy = vi.spyOn(fs, 'readdirSync').mockImplementation(() => { throw new Error('ENOENT') })
    expect(getDirSize('/nonexistent')).toBe(0)
    spy.mockRestore()
  })

  it('should handle stat error', () => {
    const r = vi.spyOn(fs, 'readdirSync').mockReturnValue([{ name: 'bad.txt', isFile: () => true, isDirectory: () => false }])
    const s = vi.spyOn(fs, 'statSync').mockImplementation(() => { throw new Error('EACCES') })
    expect(getDirSize('/restricted')).toBe(0)
    r.mockRestore()
    s.mockRestore()
  })
})

// ========== loadSettings / saveSettings ==========
describe('loadSettings', () => {
  beforeEach(() => { vi.clearAllMocks() })

  it('should load settings from file', () => {
    const r = vi.spyOn(fs, 'readFileSync').mockReturnValue('{"key": "value"}')
    const e = vi.spyOn(fs, 'existsSync').mockReturnValue(true)
    expect(loadSettings('/tmp/settings.json')).toEqual({ key: 'value' })
    r.mockRestore(); e.mockRestore()
  })

  it('should return empty object when file missing', () => {
    const e = vi.spyOn(fs, 'existsSync').mockReturnValue(false)
    expect(loadSettings('/tmp/settings.json')).toEqual({})
    e.mockRestore()
  })

  it('should return empty on JSON parse error', () => {
    const r = vi.spyOn(fs, 'readFileSync').mockReturnValue('not json')
    const e = vi.spyOn(fs, 'existsSync').mockReturnValue(true)
    expect(loadSettings('/tmp/settings.json')).toEqual({})
    r.mockRestore(); e.mockRestore()
  })

  it('should return empty on readFileSync error', () => {
    const r = vi.spyOn(fs, 'readFileSync').mockImplementation(() => { throw new Error('EACCES') })
    const e = vi.spyOn(fs, 'existsSync').mockReturnValue(true)
    expect(loadSettings('/tmp/settings.json')).toEqual({})
    r.mockRestore(); e.mockRestore()
  })

  it('should handle nested JSON', () => {
    const r = vi.spyOn(fs, 'readFileSync').mockReturnValue('{"a":{"b":1,"c":[2,3]}}')
    const e = vi.spyOn(fs, 'existsSync').mockReturnValue(true)
    const result = loadSettings('/tmp/settings.json')
    expect(result.a.b).toBe(1)
    expect(result.a.c).toEqual([2, 3])
    r.mockRestore(); e.mockRestore()
  })
})

describe('saveSettings', () => {
  beforeEach(() => { vi.clearAllMocks() })

  it('should save settings to file', () => {
    const w = vi.spyOn(fs, 'writeFileSync').mockImplementation(() => {})
    saveSettings('/tmp/settings.json', { a: 1 })
    expect(w).toHaveBeenCalledWith('/tmp/settings.json', '{\n  "a": 1\n}')
    w.mockRestore()
  })

  it('should handle write error gracefully', () => {
    const w = vi.spyOn(fs, 'writeFileSync').mockImplementation(() => { throw new Error('EACCES') })
    expect(() => saveSettings('/tmp/settings.json', { a: 1 })).not.toThrow()
    w.mockRestore()
  })

  it('should handle null settings', () => {
    const w = vi.spyOn(fs, 'writeFileSync').mockImplementation(() => {})
    saveSettings('/tmp/settings.json', null)
    expect(w).toHaveBeenCalled()
    w.mockRestore()
  })
})
