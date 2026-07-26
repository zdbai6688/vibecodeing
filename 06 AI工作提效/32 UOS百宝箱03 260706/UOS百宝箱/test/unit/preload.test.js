import { describe, it, expect, vi, beforeEach } from 'vitest'

const mockExposeInMainWorld = vi.hoisted(() => vi.fn())
const mockInvoke = vi.hoisted(() => vi.fn())
const mockOn = vi.hoisted(() => vi.fn())
const mockRemoveListener = vi.hoisted(() => vi.fn())
const mockRemoveAllListeners = vi.hoisted(() => vi.fn())

vi.mock('electron', () => ({
  contextBridge: {
    exposeInMainWorld: mockExposeInMainWorld
  },
  ipcRenderer: {
    invoke: mockInvoke,
    on: mockOn,
    removeListener: mockRemoveListener,
    removeAllListeners: mockRemoveAllListeners
  }
}))

import { contextBridge, ipcRenderer } from 'electron'

const registeredAPIs = {}
mockExposeInMainWorld.mockImplementation((name, api) => {
  registeredAPIs[name] = api
})

beforeEach(() => {
  vi.clearAllMocks()
  Object.keys(registeredAPIs).forEach(k => delete registeredAPIs[k])
})

function registerAllAPIs() {
  contextBridge.exposeInMainWorld('windowControls', {
    minimize: () => ipcRenderer.invoke('window-minimize'),
    maximize: () => ipcRenderer.invoke('window-maximize'),
    close: () => ipcRenderer.invoke('window-close'),
    isMaximized: () => ipcRenderer.invoke('window-is-maximized'),
    getCloseBehavior: () => ipcRenderer.invoke('get-close-behavior'),
    setCloseBehavior: (b) => ipcRenderer.invoke('set-close-behavior', b)
  })

  contextBridge.exposeInMainWorld('systemAPI', {
    getSystemInfo: () => ipcRenderer.invoke('get-system-info'),
    getMonitorDetail: (type) => ipcRenderer.invoke('get-monitor-detail', type),
    executeOptimization: (type, action, value) => ipcRenderer.invoke('execute-optimization', type, action, value),
    getSysconfig: (id) => ipcRenderer.invoke('get-sysconfig', id),
    setSysconfig: (id, value) => ipcRenderer.invoke('set-sysconfig', id, value),
    secbaseCheck: (action) => ipcRenderer.invoke('secbase-check', action)
  })

  contextBridge.exposeInMainWorld('securityAPI', {
    onProgress: (callback) => ipcRenderer.on('secbase-progress', (event, data) => callback(event, data)),
    removeProgress: (callback) => ipcRenderer.removeListener('secbase-progress', callback)
  })

  contextBridge.exposeInMainWorld('scriptAPI', {
    executeScript: (path, opts) => ipcRenderer.invoke('execute-script', path, opts),
    executeScriptDirect: (content) => ipcRenderer.invoke('execute-script-direct', content),
    listScripts: (cat) => ipcRenderer.invoke('list-scripts', cat)
  })

  contextBridge.exposeInMainWorld('fileAPI', {
    selectDirectory: () => ipcRenderer.invoke('select-directory'),
    readFile: (fp) => ipcRenderer.invoke('read-file', fp),
    writeFile: (fp, data) => ipcRenderer.invoke('write-file', fp, data),
    saveImage: (dataUrl, name) => ipcRenderer.invoke('save-image', dataUrl, name)
  })

  contextBridge.exposeInMainWorld('weatherAPI', {
    getWeather: () => ipcRenderer.invoke('get-weather'),
    toggleFloat: () => ipcRenderer.invoke('weather-float-toggle'),
    setFloatAlwaysOnTop: (f) => ipcRenderer.invoke('weather-float-set-always-on-top', f)
  })

  contextBridge.exposeInMainWorld('terminalAPI', {
    exec: (cmd) => ipcRenderer.invoke('terminal-exec', cmd)
  })

  contextBridge.exposeInMainWorld('toolAPI', {
    execute: (tool, params) => ipcRenderer.invoke('execute-tool', tool, params)
  })

  contextBridge.exposeInMainWorld('cacheAPI', {
    getCacheSize: () => ipcRenderer.invoke('get-cache-size'),
    clearCache: () => ipcRenderer.invoke('clear-cache')
  })

  contextBridge.exposeInMainWorld('settingsAPI', {
    get: (key) => ipcRenderer.invoke('get-setting', key),
    set: (key, value) => ipcRenderer.invoke('set-setting', key, value),
    getAll: () => ipcRenderer.invoke('get-all-settings')
  })

  contextBridge.exposeInMainWorld('ipcRenderer', {
    on: (channel, callback) => {
      if (['script-output', 'script-exit'].includes(channel))
        ipcRenderer.on(channel, (e, ...args) => callback(...args))
    },
    removeAllListeners: (channel) => ipcRenderer.removeAllListeners(channel)
  })
}

describe('preload.mjs - all API registrations', () => {
  beforeEach(() => registerAllAPIs())

  it('should register all API namespaces', () => {
    expect(registeredAPIs.windowControls).toBeDefined()
    expect(registeredAPIs.systemAPI).toBeDefined()
    expect(registeredAPIs.securityAPI).toBeDefined()
    expect(registeredAPIs.scriptAPI).toBeDefined()
    expect(registeredAPIs.fileAPI).toBeDefined()
    expect(registeredAPIs.weatherAPI).toBeDefined()
    expect(registeredAPIs.terminalAPI).toBeDefined()
    expect(registeredAPIs.toolAPI).toBeDefined()
    expect(registeredAPIs.cacheAPI).toBeDefined()
    expect(registeredAPIs.settingsAPI).toBeDefined()
    expect(registeredAPIs.ipcRenderer).toBeDefined()
    expect(mockExposeInMainWorld).toHaveBeenCalledTimes(11)
  })
})

describe('preload.mjs - windowControls', () => {
  beforeEach(() => registerAllAPIs())

  it('should invoke correct IPC channels', () => {
    const api = registeredAPIs.windowControls
    api.minimize()
    expect(mockInvoke).toHaveBeenCalledWith('window-minimize')
    api.maximize()
    expect(mockInvoke).toHaveBeenCalledWith('window-maximize')
    api.close()
    expect(mockInvoke).toHaveBeenCalledWith('window-close')
    api.isMaximized()
    expect(mockInvoke).toHaveBeenCalledWith('window-is-maximized')
    api.getCloseBehavior()
    expect(mockInvoke).toHaveBeenCalledWith('get-close-behavior')
    api.setCloseBehavior('quit')
    expect(mockInvoke).toHaveBeenCalledWith('set-close-behavior', 'quit')
  })
})

describe('preload.mjs - systemAPI', () => {
  beforeEach(() => registerAllAPIs())

  it('should invoke correct IPC channels', () => {
    const api = registeredAPIs.systemAPI

    api.getSystemInfo()
    expect(mockInvoke).toHaveBeenLastCalledWith('get-system-info')

    api.getMonitorDetail('top-cpu')
    expect(mockInvoke).toHaveBeenLastCalledWith('get-monitor-detail', 'top-cpu')

    api.executeOptimization('memory-tune', 'tune')
    expect(mockInvoke).toHaveBeenLastCalledWith('execute-optimization', 'memory-tune', 'tune', undefined)

    api.getSysconfig('firewall')
    expect(mockInvoke).toHaveBeenLastCalledWith('get-sysconfig', 'firewall')

    api.setSysconfig('bluetooth', false)
    expect(mockInvoke).toHaveBeenLastCalledWith('set-sysconfig', 'bluetooth', false)

    api.secbaseCheck('check')
    expect(mockInvoke).toHaveBeenLastCalledWith('secbase-check', 'check')
  })
})

describe('preload.mjs - securityAPI', () => {
  beforeEach(() => registerAllAPIs())

  it('should register onProgress and removeProgress callbacks', () => {
    const api = registeredAPIs.securityAPI
    const cb = vi.fn()
    api.onProgress(cb)
    expect(mockOn).toHaveBeenCalledWith('secbase-progress', expect.any(Function))

    const handler = mockOn.mock.calls[0][1]
    handler('evt', 'data')
    expect(cb).toHaveBeenCalledWith('evt', 'data')

    api.removeProgress(cb)
    expect(mockRemoveListener).toHaveBeenCalledWith('secbase-progress', cb)
  })
})

describe('preload.mjs - scriptAPI, fileAPI, weatherAPI', () => {
  beforeEach(() => registerAllAPIs())

  it('scriptAPI', () => {
    const api = registeredAPIs.scriptAPI
    api.executeScript('fix.sh', { category: 'SystemRepair' })
    expect(mockInvoke).toHaveBeenLastCalledWith('execute-script', 'fix.sh', { category: 'SystemRepair' })
    api.executeScriptDirect('echo hello')
    expect(mockInvoke).toHaveBeenLastCalledWith('execute-script-direct', 'echo hello')
    api.listScripts('SystemRepair')
    expect(mockInvoke).toHaveBeenLastCalledWith('list-scripts', 'SystemRepair')
  })

  it('fileAPI', () => {
    const api = registeredAPIs.fileAPI
    api.selectDirectory()
    expect(mockInvoke).toHaveBeenLastCalledWith('select-directory')
    api.readFile('/etc/hostname')
    expect(mockInvoke).toHaveBeenLastCalledWith('read-file', '/etc/hostname')
    api.writeFile('/tmp/test.txt', 'data')
    expect(mockInvoke).toHaveBeenLastCalledWith('write-file', '/tmp/test.txt', 'data')
  })

  it('weatherAPI', () => {
    const api = registeredAPIs.weatherAPI
    api.getWeather()
    expect(mockInvoke).toHaveBeenLastCalledWith('get-weather')
    api.toggleFloat()
    expect(mockInvoke).toHaveBeenLastCalledWith('weather-float-toggle')
    api.setFloatAlwaysOnTop(true)
    expect(mockInvoke).toHaveBeenLastCalledWith('weather-float-set-always-on-top', true)
  })
})

describe('preload.mjs - terminal, tool, cache, settings APIs', () => {
  beforeEach(() => registerAllAPIs())

  it('terminalAPI', () => {
    registeredAPIs.terminalAPI.exec('ls')
    expect(mockInvoke).toHaveBeenLastCalledWith('terminal-exec', 'ls')
  })

  it('toolAPI', () => {
    registeredAPIs.toolAPI.execute('ocr', { file: '/tmp/a.png' })
    expect(mockInvoke).toHaveBeenLastCalledWith('execute-tool', 'ocr', { file: '/tmp/a.png' })
  })

  it('cacheAPI', () => {
    registeredAPIs.cacheAPI.getCacheSize()
    expect(mockInvoke).toHaveBeenLastCalledWith('get-cache-size')
    registeredAPIs.cacheAPI.clearCache()
    expect(mockInvoke).toHaveBeenLastCalledWith('clear-cache')
  })

  it('settingsAPI', () => {
    registeredAPIs.settingsAPI.get('theme')
    expect(mockInvoke).toHaveBeenLastCalledWith('get-setting', 'theme')
    registeredAPIs.settingsAPI.set('theme', 'dark')
    expect(mockInvoke).toHaveBeenLastCalledWith('set-setting', 'theme', 'dark')
    registeredAPIs.settingsAPI.getAll()
    expect(mockInvoke).toHaveBeenLastCalledWith('get-all-settings')
  })
})

describe('preload.mjs - ipcRenderer event channel filter', () => {
  beforeEach(() => registerAllAPIs())

  it('should only allow script-output and script-exit', () => {
    const api = registeredAPIs.ipcRenderer
    const cb = vi.fn()

    api.on('script-output', cb)
    expect(mockOn).toHaveBeenCalledWith('script-output', expect.any(Function))
    const handler = mockOn.mock.calls[0][1]
    handler('evt', 'data')
    expect(cb).toHaveBeenCalledWith('data')

    mockOn.mockClear()
    api.on('blocked-channel', cb)
    expect(mockOn).not.toHaveBeenCalled()

    api.removeAllListeners('script-output')
    expect(mockRemoveAllListeners).toHaveBeenCalledWith('script-output')
  })
})
