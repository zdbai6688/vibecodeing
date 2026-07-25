// ============================================
// UOS运维工具箱 - 预加载脚本 (IPC 通信桥接)
// UOS运维工具箱 - 系统运维工具
// ============================================

const { contextBridge, ipcRenderer } = require('electron')

// 窗口控制 API
contextBridge.exposeInMainWorld('windowControls', {
  minimize: () => ipcRenderer.invoke('window-minimize'),
  maximize: () => ipcRenderer.invoke('window-maximize'),
  close: () => ipcRenderer.invoke('window-close'),
  isMaximized: () => ipcRenderer.invoke('window-is-maximized'),
  getCloseBehavior: () => ipcRenderer.invoke('get-close-behavior'),
  setCloseBehavior: (b) => ipcRenderer.invoke('set-close-behavior', b)
})

// 系统信息 API
contextBridge.exposeInMainWorld('systemAPI', {
  getSystemInfo: () => ipcRenderer.invoke('get-system-info'),
  getSystemMonitor: () => ipcRenderer.invoke('get-system-monitor'),
  getResourceMonitor: () => ipcRenderer.invoke('get-resource-monitor'),
  getMonitorDetail: (type) => ipcRenderer.invoke('get-monitor-detail', type),
  executeOptimization: (type, action, value) => ipcRenderer.invoke('execute-optimization', type, action, value),
  getSysconfig: (id) => ipcRenderer.invoke('get-sysconfig', id),
  setSysconfig: (id, value) => ipcRenderer.invoke('set-sysconfig', id, value),
  executeTerminalScript: (scriptPath, input) => ipcRenderer.invoke('execute-terminal-script', scriptPath, input),
  secbaseCheck: (action, password) => ipcRenderer.invoke('secbase-check', action, password)
})

// 脚本执行 API
contextBridge.exposeInMainWorld('securityAPI', {
  onProgress: (callback) => ipcRenderer.on('secbase-progress', (event, data) => callback(event, data)),
  removeProgress: (callback) => ipcRenderer.removeListener('secbase-progress', callback)
})

contextBridge.exposeInMainWorld('scriptAPI', {
  executeScript: (scriptPath, options) => ipcRenderer.invoke('execute-script', scriptPath, options),
  executeScriptDirect: (content) => ipcRenderer.invoke('execute-script-direct', content),
  listScripts: (category) => ipcRenderer.invoke('list-scripts', category)
})

// 文件操作 API
contextBridge.exposeInMainWorld('fileAPI', {
  selectDirectory: () => ipcRenderer.invoke('select-directory'),
  selectFile: (filters) => ipcRenderer.invoke('select-file', filters),
  saveFileDialog: (name, filters) => ipcRenderer.invoke('save-file-dialog', name, filters),
  saveFile: (options) => ipcRenderer.invoke('save-file', options),
  readFile: (fp) => ipcRenderer.invoke('read-file', fp),
  writeFile: (fp, data) => ipcRenderer.invoke('write-file', fp, data),
  openPath: (fp) => ipcRenderer.invoke('open-file-path', fp),
  saveImage: (dataUrl, name) => ipcRenderer.invoke('save-image', dataUrl, name)
})

// 天气 API
contextBridge.exposeInMainWorld('weatherAPI', {
  getWeather: () => ipcRenderer.invoke('get-weather'),
  toggleFloat: () => ipcRenderer.invoke('weather-float-toggle'),
  showFloat: () => ipcRenderer.invoke('weather-float-show'),
  hideFloat: () => ipcRenderer.invoke('weather-float-hide'),
  closeFloat: () => ipcRenderer.invoke('weather-float-close'),
  isFloatVisible: () => ipcRenderer.invoke('weather-float-is-visible'),
  getFloatStyle: () => ipcRenderer.invoke('weather-float-get-style'),
  setFloatStyle: (s) => ipcRenderer.invoke('weather-float-set-style', s),
  setFloatAlwaysOnTop: (f) => ipcRenderer.invoke('weather-float-set-always-on-top', f),
  setFloatPositionLocked: (l) => ipcRenderer.invoke('weather-float-set-position-locked', l)
})

// 终端 API
contextBridge.exposeInMainWorld('terminalAPI', {
  exec: (cmd) => ipcRenderer.invoke('terminal-exec', cmd)
})


// 实用工具 API
contextBridge.exposeInMainWorld('toolAPI', {
  execute: (tool, params) => ipcRenderer.invoke('execute-tool', tool, params)
})


// 软件包管理器 API
contextBridge.exposeInMainWorld('pkgAPI', {
  listInstalled: () => ipcRenderer.invoke('pkg-manager', 'list-installed'),
  search: (keyword) => ipcRenderer.invoke('pkg-manager', 'search', { keyword }),
  install: (name, password) => ipcRenderer.invoke('pkg-manager', 'install', { name, password }),
  remove: (name, password) => ipcRenderer.invoke('pkg-manager', 'remove', { name, password }),
  info: (name) => ipcRenderer.invoke('pkg-manager', 'info', { name }),
  files: (name) => ipcRenderer.invoke('pkg-manager', 'files', { name })
})

// 缓存管理 API
contextBridge.exposeInMainWorld('cacheAPI', {
  getCacheSize: () => ipcRenderer.invoke('get-cache-size'),
  clearCache: () => ipcRenderer.invoke('clear-cache')
})

// 设置 API
contextBridge.exposeInMainWorld('settingsAPI', {
  get: (key) => ipcRenderer.invoke('get-setting', key),
  set: (key, value) => ipcRenderer.invoke('set-setting', key, value),
  getAll: () => ipcRenderer.invoke('get-all-settings'),
  setAutoStart: (enable) => ipcRenderer.invoke('set-auto-start', enable)
})

// Shell API (用于打开文件/路径)
contextBridge.exposeInMainWorld('shell', {
  openPath: (filePath) => ipcRenderer.invoke('shell-open-path', filePath),
  getResourcePath: () => ipcRenderer.invoke('get-resource-path')
})


// 局域网文件传输 API
contextBridge.exposeInMainWorld('localsendAPI', {
  start: (dir, port) => ipcRenderer.invoke('localsend-start', dir, port),
  stop: () => ipcRenderer.invoke('localsend-stop'),
  getStatus: () => ipcRenderer.invoke('localsend-status'),
  getIP: () => ipcRenderer.invoke('localsend-get-ip')
})

// 软件包管理器 API
contextBridge.exposeInMainWorld('packmgrAPI', {
  list: (search) => ipcRenderer.invoke('packmgr-list', search),
  info: (pkgName) => ipcRenderer.invoke('packmgr-info', pkgName),
  search: (keyword) => ipcRenderer.invoke('packmgr-search', keyword),
  getDebInfo: (filePath) => ipcRenderer.invoke('packmgr-get-deb-info', filePath),
  install: (filePath, password) => ipcRenderer.invoke('packmgr-install', filePath, password),
  uninstall: (pkgName, password) => ipcRenderer.invoke('packmgr-uninstall', pkgName, password)
})
// IPC 事件监听
contextBridge.exposeInMainWorld('ipcRenderer', {
  on: (channel, callback) => {
    if (['script-output', 'script-exit'].includes(channel))
      ipcRenderer.on(channel, (e, ...args) => callback(...args))
  },
  removeAllListeners: (channel) => ipcRenderer.removeAllListeners(channel)
})