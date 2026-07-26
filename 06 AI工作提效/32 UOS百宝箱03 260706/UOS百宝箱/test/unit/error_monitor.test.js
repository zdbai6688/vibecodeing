import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'

// ========== 前端错误监控测试 ==========
describe('前端错误监控', () => {
  beforeEach(() => {
    // Mock localStorage
    const storage = {}
    global.localStorage = {
      getItem: vi.fn((key) => storage[key] || null),
      setItem: vi.fn((key, val) => { storage[key] = val }),
      removeItem: vi.fn((key) => { delete storage[key] }),
      clear: vi.fn(() => { Object.keys(storage).forEach(k => delete storage[k]) })
    }
    global.__ERROR_LOGS_KEY__ = '_uos_test_error_logs'
    global.__MAX_ERROR_LOGS__ = 50
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  function addErrorLog(type, message, source, lineno, colno, stack) {
    try {
      const KEY = global.__ERROR_LOGS_KEY__
      const MAX = global.__MAX_ERROR_LOGS__
      let logs = JSON.parse(localStorage.getItem(KEY) || '[]')
      logs.push({
        type, message, source: source || '', lineno: lineno || 0,
        colno: colno || 0, stack: stack || '',
        time: new Date().toISOString(), version: '1.4.0'
      })
      if (logs.length > MAX) logs = logs.slice(logs.length - MAX)
      localStorage.setItem(KEY, JSON.stringify(logs))
      return true
    } catch(e) { return false }
  }

  function getErrorLogs() {
    try {
      return JSON.parse(localStorage.getItem(global.__ERROR_LOGS_KEY__) || '[]')
    } catch(e) { return [] }
  }

  function clearErrorLogs() {
    try {
      localStorage.removeItem(global.__ERROR_LOGS_KEY__)
      return true
    } catch(e) { return false }
  }

  it('should store error logs in localStorage', () => {
    const result = addErrorLog('render', 'Test error', 'test.js', 10, 5, 'Error stack trace')
    expect(result).toBe(true)
    const logs = JSON.parse(localStorage.getItem('_uos_test_error_logs'))
    expect(logs.length).toBe(1)
    expect(logs[0].type).toBe('render')
    expect(logs[0].message).toBe('Test error')
    expect(logs[0].source).toBe('test.js')
    expect(logs[0].lineno).toBe(10)
    expect(logs[0].colno).toBe(5)
    expect(logs[0].stack).toBe('Error stack trace')
    expect(logs[0].version).toBe('1.4.0')
    expect(logs[0].time).toBeTruthy()
  })

  it('should return empty array when no errors', () => {
    const logs = getErrorLogs()
    expect(logs).toEqual([])
  })

  it('should retrieve stored error logs', () => {
    addErrorLog('render', 'Error 1', 'a.js', 1, 1, 'stack1')
    addErrorLog('promise', 'Error 2', 'b.js', 2, 2, 'stack2')
    const logs = getErrorLogs()
    expect(logs.length).toBe(2)
    expect(logs[0].message).toBe('Error 1')
    expect(logs[1].message).toBe('Error 2')
  })

  it('should clear all error logs', () => {
    addErrorLog('render', 'Test error', 'test.js', 1, 1, 'stack')
    expect(getErrorLogs().length).toBe(1)
    clearErrorLogs()
    expect(getErrorLogs().length).toBe(0)
  })

  it('should handle storage errors gracefully', () => {
    const spy = vi.spyOn(JSON, 'parse').mockImplementationOnce(() => { throw new Error('Parse error') })
    const logs = getErrorLogs()
    expect(logs).toEqual([])
    spy.mockRestore()
  })

  it('should limit error logs to MAX', () => {
    global.__MAX_ERROR_LOGS__ = 3
    for (let i = 0; i < 5; i++) {
      addErrorLog('render', `Error ${i}`, 'test.js', i, 0, `stack${i}`)
    }
    const logs = getErrorLogs()
    expect(logs.length).toBe(3)
    expect(logs[0].message).toBe('Error 2')
    expect(logs[2].message).toBe('Error 4')
  })

  it('should store promise type errors', () => {
    addErrorLog('promise', 'Promise rejection', 'async.js', 5, 3, 'async stack')
    const logs = getErrorLogs()
    expect(logs[0].type).toBe('promise')
  })

  it('should handle empty message', () => {
    addErrorLog('render', '', 'source.js', 0, 0, '')
    const logs = getErrorLogs()
    expect(logs.length).toBe(1)
    expect(logs[0].message).toBe('')
  })

  it('should handle missing optional fields', () => {
    addErrorLog('render', 'test', undefined, undefined, undefined, undefined)
    const logs = getErrorLogs()
    expect(logs[0].source).toBe('')
    expect(logs[0].lineno).toBe(0)
  })

  it('should handle duplicate error logging', () => {
    for (let i = 0; i < 10; i++) {
      addErrorLog('render', 'Same error', 'test.js', 1, 1, 'same stack')
    }
    const logs = getErrorLogs()
    expect(logs.length).toBe(10)
    logs.forEach(log => {
      expect(log.message).toBe('Same error')
    })
  })
})

// ========== 主进程错误监控测试 ==========
describe('主进程错误监控', () => {
  const fs = require('fs')
  const path = require('path')
  const os = require('os')
  const testLogPath = path.join(os.tmpdir(), 'uos-test-error.log')

  beforeEach(() => {
    if (fs.existsSync(testLogPath)) fs.unlinkSync(testLogPath)
    global.__mainErrorLogs = []
  })

  afterEach(() => {
    if (fs.existsSync(testLogPath)) fs.unlinkSync(testLogPath)
    delete global.__mainErrorLogs
  })

  function appendErrorLog(level, source, message, stack) {
    try {
      const timestamp = new Date().toISOString()
      const entry = `[${timestamp}] [${level}] [${source}] ${message}\n${stack ? stack + '\n' : ''}`
      fs.appendFileSync(testLogPath, entry)
      if (!global.__mainErrorLogs) global.__mainErrorLogs = []
      global.__mainErrorLogs.push({
        level, source, message, stack: stack || '', time: timestamp
      })
      if (global.__mainErrorLogs.length > 200) {
        global.__mainErrorLogs = global.__mainErrorLogs.slice(-200)
      }
      return true
    } catch(e) { return false }
  }

  it('should append error log to file', () => {
    const result = appendErrorLog('ERROR', 'test', 'Test error message', 'Error stack')
    expect(result).toBe(true)
    expect(fs.existsSync(testLogPath)).toBe(true)
    const content = fs.readFileSync(testLogPath, 'utf-8')
    expect(content).toContain('[ERROR]')
    expect(content).toContain('[test]')
    expect(content).toContain('Test error message')
  })

  it('should keep in-memory error logs', () => {
    appendErrorLog('FATAL', 'crash', 'Fatal crash', 'crash stack')
    expect(global.__mainErrorLogs.length).toBe(1)
    expect(global.__mainErrorLogs[0].level).toBe('FATAL')
    expect(global.__mainErrorLogs[0].message).toBe('Fatal crash')
  })

  it('should limit in-memory logs to 200', () => {
    for (let i = 0; i < 250; i++) {
      appendErrorLog('ERROR', 'test', `Error ${i}`, `stack${i}`)
    }
    expect(global.__mainErrorLogs.length).toBe(200)
    expect(global.__mainErrorLogs[0].message).toBe('Error 50')
  })

  it('should handle multiple log levels', () => {
    appendErrorLog('ERROR', 'module1', 'error msg', '')
    appendErrorLog('FATAL', 'module2', 'fatal msg', 'fatal stack')
    appendErrorLog('WARN', 'module3', 'warning msg', '')
    expect(global.__mainErrorLogs.length).toBe(3)
    expect(global.__mainErrorLogs.map(l => l.level)).toEqual(['ERROR', 'FATAL', 'WARN'])
  })

  it('should handle empty stack', () => {
    appendErrorLog('ERROR', 'test', 'no stack', '')
    expect(global.__mainErrorLogs[0].stack).toBe('')
  })

  it('should handle file append error gracefully', () => {
    const spy = vi.spyOn(fs, 'appendFileSync').mockImplementationOnce(() => { throw new Error('EACCES') })
    const result = appendErrorLog('ERROR', 'test', 'msg', 'stack')
    expect(result).toBe(false)
    spy.mockRestore()
  })
})
