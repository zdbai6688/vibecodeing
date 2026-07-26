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
  formatSize: (bytes) => {
    if (!bytes && bytes !== 0) return '-'
    const units = ['B', 'KB', 'MB', 'GB', 'TB', 'PB']
    let size = bytes
    let unitIdx = 0
    while (size >= 1024 && unitIdx < units.length - 1) {
      size /= 1024
      unitIdx++
    }
    return size.toFixed(unitIdx > 0 ? 1 : 0) + ' ' + units[unitIdx]
  },
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
// ========== Phase 3: 运维专家级功能 API ==========

// 防火墙规则管理器 API
contextBridge.exposeInMainWorld('firewallAPI', {
  listRules: () => ipcRenderer.invoke('firewall-list-rules'),
  addRule: (rule) => ipcRenderer.invoke('firewall-add-rule', rule),
  removeRule: (idx) => ipcRenderer.invoke('firewall-remove-rule', idx),
  toggleRule: (idx) => ipcRenderer.invoke('firewall-toggle-rule', idx),
  getStatus: () => ipcRenderer.invoke('firewall-get-status'),
  setStatus: (enable) => ipcRenderer.invoke('firewall-set-status', enable),
  backupRules: () => ipcRenderer.invoke('firewall-backup-rules'),
  restoreRules: () => ipcRenderer.invoke('firewall-restore-rules'),
  addPort: (port, proto) => ipcRenderer.invoke('firewall-add-port', port, proto)
})

// 网络流量监控 API
contextBridge.exposeInMainWorld('netflowAPI', {
  getTraffic: () => ipcRenderer.invoke('netflow-get-traffic'),
  getProcessTraffic: () => ipcRenderer.invoke('netflow-get-process-traffic'),
  getTrafficHistory: () => ipcRenderer.invoke('netflow-get-traffic-history')
})

// 系统安全扫描 API
contextBridge.exposeInMainWorld('secscanAPI', {
  runScan: (type) => ipcRenderer.invoke('secscan-run', type),
  getResults: () => ipcRenderer.invoke('secscan-get-results'),
  installTool: (tool) => ipcRenderer.invoke('secscan-install-tool', tool)
})

// 系统更新历史 API
contextBridge.exposeInMainWorld('updatehistAPI', {
  getHistory: () => ipcRenderer.invoke('updatehist-get-history'),
  rollback: (pkg, version, password) => ipcRenderer.invoke('updatehist-rollback', pkg, version, password),
  getStatus: () => ipcRenderer.invoke('updatehist-get-status')
})

// 系统配置导入导出 API
contextBridge.exposeInMainWorld('configioAPI', {
  exportConfig: (sections, filePath) => ipcRenderer.invoke('configio-export', sections, filePath),
  importConfig: (filePath) => ipcRenderer.invoke('configio-import', filePath),
  previewConfig: (filePath) => ipcRenderer.invoke('configio-preview', filePath),
  compareConfig: (filePath) => ipcRenderer.invoke('configio-compare', filePath)
})

// 系统性能分析 API
contextBridge.exposeInMainWorld('perfAPI', {
  analyzeCPU: () => ipcRenderer.invoke('perf-analyze-cpu'),
  analyzeDisk: () => ipcRenderer.invoke('perf-analyze-disk'),
  analyzeMemory: () => ipcRenderer.invoke('perf-analyze-memory'),
  runPerf: (args) => ipcRenderer.invoke('perf-run', args),
  analyzeCPUHotspot: () => ipcRenderer.invoke('perf-cpu-hotspot'),
  analyzeMemoryLeak: () => ipcRenderer.invoke('perf-memory-leak'),
  straceAnalyze: (pid) => ipcRenderer.invoke('perf-strace', pid),
  generateReport: () => ipcRenderer.invoke('perf-generate-report')
})

// 系统崩溃分析 API
contextBridge.exposeInMainWorld('crashAPI', {
  listCoredumps: () => ipcRenderer.invoke('crash-list-coredumps'),
  analyzeCoredump: (id) => ipcRenderer.invoke('crash-analyze', id),
  getCrashLogs: () => ipcRenderer.invoke('crash-get-logs'),
  getSuggestions: (signal, pkg, exe) => ipcRenderer.invoke('crash-get-suggestions', signal, pkg, exe)
})

// 等保合规检查 API
contextBridge.exposeInMainWorld('complianceAPI', {
  runCheck: () => ipcRenderer.invoke('compliance-run-check'),
  getResults: () => ipcRenderer.invoke('compliance-get-results'),
  fixItem: (id, password) => ipcRenderer.invoke('compliance-fix-item', id, password),
  generateReport: () => ipcRenderer.invoke('compliance-generate-report')
})

// NTP 时间同步 API
contextBridge.exposeInMainWorld('ntpAPI', {
  getStatus: () => ipcRenderer.invoke('ntp-get-status'),
  setServer: (server) => ipcRenderer.invoke('ntp-set-server', server),
  syncNow: () => ipcRenderer.invoke('ntp-sync-now'),
  setTimezone: (tz) => ipcRenderer.invoke('ntp-set-timezone', tz),
  getTimezones: () => ipcRenderer.invoke('ntp-get-timezones')
})

// 系统代理配置 API
contextBridge.exposeInMainWorld('proxyAPI', {
  getConfig: () => ipcRenderer.invoke('proxy-get-config'),
  setConfig: (config) => ipcRenderer.invoke('proxy-set-config', config),
  testConnection: (opts) => ipcRenderer.invoke('proxy-test', opts),
  setSystemProxy: (config, password) => ipcRenderer.invoke('proxy-set-system', { config, password })
})

// 截图录屏 API
contextBridge.exposeInMainWorld('captureAPI', {
  screenshot: (mode) => ipcRenderer.invoke('capture-screenshot', mode),
  startRecording: () => ipcRenderer.invoke('capture-start-recording'),
  stopRecording: () => ipcRenderer.invoke('capture-stop-recording'),
  getRecordingStatus: () => ipcRenderer.invoke('capture-recording-status')
})

// 快捷键管理 API
contextBridge.exposeInMainWorld('hotkeyAPI', {
  listShortcuts: () => ipcRenderer.invoke('hotkey-list'),
  setShortcut: (key, cmd) => ipcRenderer.invoke('hotkey-set', key, cmd),
  resetDefault: () => ipcRenderer.invoke('hotkey-reset'),
  exportShortcuts: () => ipcRenderer.invoke('hotkey-export'),
  importShortcuts: (path) => ipcRenderer.invoke('hotkey-import', path)
})

// 主题字体管理 API
contextBridge.exposeInMainWorld('themeAPI', {
  switchTheme: (mode) => ipcRenderer.invoke('theme-switch', mode),
  listFonts: () => ipcRenderer.invoke('theme-list-fonts'),
  installFont: (path) => ipcRenderer.invoke('theme-install-font', path),
  uninstallFont: (name) => ipcRenderer.invoke('theme-uninstall-font', name),
  listIconThemes: () => ipcRenderer.invoke('theme-list-icons'),
  switchIconTheme: (name) => ipcRenderer.invoke('theme-switch-icons', name)
})

// 打印机管理 API
contextBridge.exposeInMainWorld('printerAPI', {
  listPrinters: () => ipcRenderer.invoke('printer-list'),
  addPrinter: (name, options) => ipcRenderer.invoke('printer-add', name, options),
  removePrinter: (name) => ipcRenderer.invoke('printer-remove', name),
  printTestPage: (name) => ipcRenderer.invoke('printer-test-page', name),
  getPrintQueue: (name) => ipcRenderer.invoke('printer-queue', name)
})

// 网络共享管理 API
contextBridge.exposeInMainWorld('netshareAPI', {
  getSambaStatus: () => ipcRenderer.invoke('netshare-samba-status'),
  setSambaShare: (config) => ipcRenderer.invoke('netshare-samba-set', config),
  getNFSStatus: () => ipcRenderer.invoke('netshare-nfs-status'),
  setNFSShare: (config) => ipcRenderer.invoke('netshare-nfs-set', config),
  removeShare: (sharePath, type) => ipcRenderer.invoke('netshare-remove', sharePath, type)
})

// Docker 管理 API
contextBridge.exposeInMainWorld('dockerAPI', {
  getStatus: () => ipcRenderer.invoke('docker-status'),
  listContainers: () => ipcRenderer.invoke('docker-list-containers'),
  listImages: () => ipcRenderer.invoke('docker-list-images'),
  startContainer: (id) => ipcRenderer.invoke('docker-start', id),
  stopContainer: (id) => ipcRenderer.invoke('docker-stop', id),
  removeContainer: (id) => ipcRenderer.invoke('docker-remove-container', id),
  getContainerLogs: (id) => ipcRenderer.invoke('docker-logs', id),
  pullImage: (name) => ipcRenderer.invoke('docker-pull', name),
  removeImage: (id) => ipcRenderer.invoke('docker-remove-image', id)
})

// VPN 管理 API
contextBridge.exposeInMainWorld('vpnAPI', {
  listConnections: () => ipcRenderer.invoke('vpn-list'),
  addConnection: (config) => ipcRenderer.invoke('vpn-add', config),
  removeConnection: (name) => ipcRenderer.invoke('vpn-remove', name),
  connect: (name) => ipcRenderer.invoke('vpn-connect', name),
  disconnect: (name) => ipcRenderer.invoke('vpn-disconnect', name),
  getStatus: (name) => ipcRenderer.invoke('vpn-status', name),
  importConfig: (path) => ipcRenderer.invoke('vpn-import', path)
})

// 系统升级助手 API
contextBridge.exposeInMainWorld('upgradeAPI', {
  checkUpgrade: () => ipcRenderer.invoke('upgrade-check'),
  preflightCheck: () => ipcRenderer.invoke('upgrade-preflight'),
  startUpgrade: (password) => ipcRenderer.invoke('upgrade-start', password),
  getProgress: () => ipcRenderer.invoke('upgrade-progress'),
  rollback: (password) => ipcRenderer.invoke('upgrade-rollback', password)
})

// 系统资产清单 API
contextBridge.exposeInMainWorld('assetAPI', {
  scanHardware: () => ipcRenderer.invoke('asset-scan-hardware'),
  scanSoftware: () => ipcRenderer.invoke('asset-scan-software'),
  getInventory: () => ipcRenderer.invoke('asset-get-inventory'),
  exportReport: (format) => ipcRenderer.invoke('asset-export', format)
})

// 远程管理工具 API
contextBridge.exposeInMainWorld('remoteAPI', {
  listSSHKeys: () => ipcRenderer.invoke('remote-list-keys'),
  addSSHKey: (name, key) => ipcRenderer.invoke('remote-add-key', name, key),
  removeSSHKey: (name) => ipcRenderer.invoke('remote-remove-key', name),
  batchExec: (hosts, cmd, password) => ipcRenderer.invoke('remote-batch-exec', hosts, cmd, password),
  transferFile: (host, filePath, remotePath, password) => ipcRenderer.invoke('remote-transfer', host, filePath, remotePath, password)
})

// APT 更新历史 API
contextBridge.exposeInMainWorld('aptHistoryAPI', {
  list: () => ipcRenderer.invoke('apt-history', 'list'),
  rollback: (packageName, targetVersion, password) => ipcRenderer.invoke('apt-history', 'rollback', { packageName, targetVersion, password }),
  packageVersions: (packageName) => ipcRenderer.invoke('apt-history', 'package-versions', { packageName }),
  compare: (packageName, versionA, versionB) => ipcRenderer.invoke('apt-history', 'compare', { packageName, versionA, versionB })
})

// USB 启动盘制作 API
contextBridge.exposeInMainWorld('usbBootAPI', {
  listDevices: () => ipcRenderer.invoke('phase2-usb', 'list'),
  create: (iso, device, password) => ipcRenderer.invoke('phase2-usb', 'create', { iso, device, password })
})


// 系统应用集成 API
contextBridge.exposeInMainWorld('systemApp', {
  launchApp: (appName) => ipcRenderer.invoke('systemapp-launch', appName),
  callDbus: (service, objectPath, method, args) => ipcRenderer.invoke('systemapp-dbus', { service, objectPath, method, args }),
  callCli: (command) => ipcRenderer.invoke('systemapp-cli', command)
})

// ========== 错误监控 API ==========
contextBridge.exposeInMainWorld('errorAPI', {
  getLogs: () => ipcRenderer.invoke('get-error-logs'),
  clearLogs: () => ipcRenderer.invoke('clear-error-logs')
})
// 磁盘分析器 API (Stage 2)
contextBridge.exposeInMainWorld('diskAPI', {
  getDiskUsage: () => ipcRenderer.invoke('disk-analyzer', 'usage'),
  getBigFiles: () => ipcRenderer.invoke('disk-analyzer', 'big-files')
})

// 日志查看器 API (Stage 2)
contextBridge.exposeInMainWorld('logAPI', {
  search: (opts) => ipcRenderer.invoke('syslog-viewer', 'search', opts),
  tail: () => ipcRenderer.invoke('syslog-viewer', 'tail')
})


// 远程桌面 API (Stage 4)
contextBridge.exposeInMainWorld('remoteDesktopAPI', {
  check: () => ipcRenderer.invoke('remote-desktop', 'check'),
  connect: (params) => ipcRenderer.invoke('remote-desktop', 'connect', params),
  listConnections: () => ipcRenderer.invoke('remote-desktop', 'list-connections'),
  saveConnection: (params) => ipcRenderer.invoke('remote-desktop', 'save-connection', params),
  deleteConnection: (name) => ipcRenderer.invoke('remote-desktop', 'delete-connection', { name: name }),
})


// 进程管理器 API
contextBridge.exposeInMainWorld('processAPI', {
  getProcessTree: () => ipcRenderer.invoke('process-tree'),
  searchProcess: (query) => ipcRenderer.invoke('process-search', query),
  killProcess: (pid, signal) => ipcRenderer.invoke('kill-process', pid, signal),
  reniceProcess: (pid, priority) => ipcRenderer.invoke('renice-process', pid, priority),
  getProcessDetail: (pid) => ipcRenderer.invoke('process-detail', pid)
})


// VPNa 管理 API

// 性能基准测试 API (Stage 3)
contextBridge.exposeInMainWorld('benchmarkAPI', {
  run: (action, options) => ipcRenderer.invoke('benchmark', action, options)
})

// 系统备份与还原 API (Stage 3)
contextBridge.exposeInMainWorld('phase2API', {
  backup: (action, params) => ipcRenderer.invoke('system-backup', action, params),
  startup: (action, params) => ipcRenderer.invoke('system-backup', action, params),
  health: () => ipcRenderer.invoke('system-backup', 'backup', {}),
  usb: (action, params) => ipcRenderer.invoke('system-backup', action, params),
  driver: (action, params) => ipcRenderer.invoke('phase2-driver', action, params),
  remote: (action, params) => ipcRenderer.invoke('remote-desktop', action, params || {}),
  search: (params) => ipcRenderer.invoke('system-backup', 'backup', params),
  benchmark: (type) => ipcRenderer.invoke('system-backup', 'backup', {})
})

// 网络诊断 API (Stage 4)
contextBridge.exposeInMainWorld('netDiagAPI', {
  run: (params) => ipcRenderer.invoke('net-diag', params)
})

// 服务管理 API (Stage 4)
contextBridge.exposeInMainWorld('svcAPI', {
  list: (opts) => ipcRenderer.invoke('svc-mgr', 'list', opts),
  action: (opts) => ipcRenderer.invoke('svc-mgr', 'action', opts)
})

// 用户管理 API (Stage 4)
contextBridge.exposeInMainWorld('userAPI', {
  list: () => ipcRenderer.invoke('user-mgr', 'list'),
  add: (opts) => ipcRenderer.invoke('user-mgr', 'add', opts),
  remove: (opts) => ipcRenderer.invoke('user-mgr', 'remove', opts),
  changePwd: (opts) => ipcRenderer.invoke('user-mgr', 'change-pwd', opts),
  loginHistory: (opts) => ipcRenderer.invoke('user-mgr', 'login-history', opts)
})

// Cron 管理 API (Stage 4)
contextBridge.exposeInMainWorld('cronAPI', {
  list: () => ipcRenderer.invoke('cron-mgr', 'list'),
  add: (opts) => ipcRenderer.invoke('cron-mgr', 'add', opts),
  remove: (opts) => ipcRenderer.invoke('cron-mgr', 'remove', opts)
})
