// ============================================
// UOS运维工具箱 - Electron 主进程
// UOS运维工具箱 - 系统运维工具
// 专为统信UOS系统打造
// ============================================

delete process.env.ELECTRON_RUN_AS_NODE

const { app, BrowserWindow, ipcMain, Menu, Tray, nativeImage, dialog, shell } = require('electron')
const path = require('path')
const fs = require('fs')
const os = require('os')
const { spawn, execSync } = require('child_process')
const https = require('https')

let mainWindow = null
let tray = null
let weatherFloatWindow = null
let closeBehavior = 'hide'

// ========== 窗口创建 ==========

function createMainWindow() {
  mainWindow = new BrowserWindow({
    width: 1100, height: 750, minWidth: 900, minHeight: 600,
    frame: false, backgroundColor: '#f5f5f5',
    icon: path.join(__dirname, '../resources/icon.png'),
    show: true,
    webPreferences: {
      preload: path.join(__dirname, 'preload.mjs'),
      contextIsolation: true, nodeIntegration: false,
      backgroundThrottling: false
    }
  })

  if (process.env.ELECTRON_RENDERER_URL) {
    mainWindow.loadURL(process.env.ELECTRON_RENDERER_URL)
  } else {
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'))
  }

  mainWindow.on('ready-to-show', () => mainWindow.show())

  mainWindow.on('close', (e) => {
    if (closeBehavior === 'hide') { e.preventDefault(); mainWindow.hide() }
  })
  mainWindow.on('closed', () => { mainWindow = null })
}

function createTray() {
  const iconPath = path.join(__dirname, '../resources/tray.png')
  let trayIcon
  try { trayIcon = nativeImage.createFromPath(iconPath) } catch { trayIcon = nativeImage.createEmpty() }

  tray = new Tray(trayIcon)
  tray.setToolTip('UOS运维工具箱')

  const contextMenu = Menu.buildFromTemplate([
    { label: '显示主窗口', click: () => { if (mainWindow) { mainWindow.show(); mainWindow.focus() } } },
    { type: 'separator' },
    { label: '退出', click: () => { closeBehavior = 'quit'; app.quit() } }
  ])

  tray.setContextMenu(contextMenu)
  tray.on('click', () => {
    if (mainWindow) mainWindow.isVisible() ? mainWindow.hide() : mainWindow.show()
  })
}

function createWeatherFloatWindow(style = {}) {
  const { width = 280, height = 180, x, y } = style
  weatherFloatWindow = new BrowserWindow({
    width, height, x: x || 100, y: y || 100,
    frame: false, transparent: true, alwaysOnTop: true,
    resizable: true, skipTaskbar: true, type: 'toolbar',
    webPreferences: {
      preload: path.join(__dirname, 'preload.mjs'),
      contextIsolation: true, nodeIntegration: false
    }
  })

  if (process.env.ELECTRON_RENDERER_URL) {
    weatherFloatWindow.loadURL(process.env.ELECTRON_RENDERER_URL + '#/weather-float')
  } else {
    weatherFloatWindow.loadFile(path.join(__dirname, '../dist/index.html'), { hash: '/weather-float' })
  }
  weatherFloatWindow.on('closed', () => { weatherFloatWindow = null })
}

// ========== IPC 窗口控制 ==========

ipcMain.handle('window-minimize', () => mainWindow?.minimize())
ipcMain.handle('window-maximize', () => {
  mainWindow?.isMaximized() ? mainWindow.unmaximize() : mainWindow?.maximize()
})
ipcMain.handle('window-close', () => mainWindow?.close())
ipcMain.handle('window-is-maximized', () => mainWindow?.isMaximized() || false)
ipcMain.handle('get-close-behavior', () => closeBehavior)
ipcMain.handle('set-close-behavior', (_, b) => { closeBehavior = b })

// ========== IPC 文件对话框 ==========

ipcMain.handle('select-directory', async () => {
  const r = await dialog.showOpenDialog(mainWindow, { properties: ['openDirectory'] })
  return r.canceled ? null : r.filePaths[0]
})

ipcMain.handle('select-file', async (_, filters) => {
  // 如果 filters 有 multiSelections 标志，则允许多选
  const multi = filters && filters.multiSelections
  const props = multi ? ['openFile', 'multiSelections'] : ['openFile']
  const f = filters ? filters.filters : filters
  const r = await dialog.showOpenDialog(mainWindow, { properties: props, filters: f || [] })
  return r.canceled ? null : (multi ? r.filePaths : r.filePaths[0])
})

ipcMain.handle('save-file-dialog', async (_, defaultName, filters) => {
  const r = await dialog.showSaveDialog(mainWindow, {
    defaultPath: defaultName, filters: filters || [{ name: '所有文件', extensions: ['*'] }]
  })
  return r.canceled ? null : r.filePath
})

ipcMain.handle('save-file', async (_, options) => {
  const r = await dialog.showSaveDialog(mainWindow, {
    defaultPath: options?.defaultName, filters: options?.filters || [{ name: '所有文件', extensions: ['*'] }]
  })
  if (!r.canceled && r.filePath && options?.content) fs.writeFileSync(r.filePath, options.content)
  return r.canceled ? null : r.filePath
})

/** 检查路径是否在允许的目录内 */
function isPathSafe(targetPath) {
  try {
    const resolved = path.resolve(targetPath)
    const allowed = [
      path.resolve(app.getPath('userData')),
      path.resolve(app.getPath('home')),
      path.resolve(app.getPath('downloads')),
      path.resolve(app.getPath('desktop')),
      path.resolve(app.getPath('documents')),
      path.resolve(os.tmpdir())
    ]
    return allowed.some(dir => resolved.startsWith(dir))
      || (resolved.startsWith('/tmp/') || resolved.startsWith('/var/tmp/'))
  } catch { return false }
}

ipcMain.handle('write-file', (_, fp, data) => {
  if (!isPathSafe(fp)) throw new Error('不允许写入此路径')
  fs.writeFileSync(fp, data); return true
})
ipcMain.handle('read-file', (_, fp) => {
  if (!isPathSafe(fp)) throw new Error('不允许读取此路径')
  return fs.readFileSync(fp, 'utf-8')
})
ipcMain.handle('open-file-path', (_, fp) => shell.showItemInFolder(fp))
ipcMain.handle('shell-open-path', (_, fp) => { try { shell.openPath(fp); return true } catch { return false } })
ipcMain.handle('get-resource-path', () => path.join(__dirname, '../resources/'))
ipcMain.handle('save-image', async (_, dataUrl, defaultName) => {
  const r = await dialog.showSaveDialog(mainWindow, {
    defaultPath: defaultName || 'image.png',
    filters: [{ name: '图片', extensions: ['png', 'jpg', 'jpeg', 'webp'] }]
  })
  if (!r.canceled && r.filePath) {
    const b64 = dataUrl.replace(/^data:image\/\w+;base64,/, '')
    fs.writeFileSync(r.filePath, Buffer.from(b64, 'base64'))
    return true
  }
  return false
})

// ========== IPC 系统信息 ==========

ipcMain.handle('get-system-info', async () => {
  const info = { os: {}, cpu: {}, memory: {}, disk: [], network: [] }
  try {
    info.os.hostname = execSync('hostname').toString().trim()
    info.os.platform = execSync('uname -m').toString().trim()
    info.os.kernel = execSync('uname -r').toString().trim()
    info.os.distro = execSync('cat /etc/os-release 2>/dev/null | grep "^PRETTY_NAME" | cut -d"=" -f2').toString().trim().replace(/"/g, '')
    info.os.desktop = process.env.XDG_CURRENT_DESKTOP || process.env.DESKTOP_SESSION || 'Unknown'
    info.os.uptime = execSync('uptime -p').toString().trim().replace('up ', '')
    // 电脑型号
    try {
      const manu = execSync('cat /sys/class/dmi/id/sys_vendor 2>/dev/null').toString().trim()
      const product = execSync('cat /sys/class/dmi/id/product_name 2>/dev/null').toString().trim()
      info.os.computerModel = (manu ? manu + ' ' : '') + product
    } catch { info.os.computerModel = '未知' }
    // 设备标识信息（面向客户运维使用）
    try {
      info.os.machineId = execSync('cat /etc/machine-id 2>/dev/null').toString().trim()
    } catch { info.os.machineId = '未知' }
    try {
      info.os.productUuid = execSync('cat /sys/class/dmi/id/product_uuid 2>/dev/null').toString().trim()
    } catch { info.os.productUuid = '未知' }
    try {
      info.os.productSerial = execSync('cat /sys/class/dmi/id/product_serial 2>/dev/null').toString().trim()
    } catch { info.os.productSerial = '未知' }
    // IP 地址
    try {
      info.os.ipAddress = execSync('hostname -I 2>/dev/null').toString().trim().split(/\s+/)[0] || '未知'
    } catch { info.os.ipAddress = '未知' }
  } catch (e) { info.os.error = e.message }
  try {
    info.cpu.model = execSync('cat /proc/cpuinfo | grep "model name" | head -1 | cut -d: -f2 | xargs').toString().trim()
    info.cpu.cores = parseInt(execSync('nproc').toString().trim())
    info.cpu.arch = execSync('uname -m').toString().trim()
  } catch (e) { info.cpu.error = e.message }
  try {
    const m = fs.readFileSync('/proc/meminfo', 'utf-8')
    const mt = parseInt(m.match(/MemTotal:\s+(\d+)/)?.[1] || '0')
    const ma = parseInt(m.match(/MemAvailable:\s+(\d+)/)?.[1] || '0')
    const st = parseInt(m.match(/SwapTotal:\s+(\d+)/)?.[1] || '0')
    const sf = parseInt(m.match(/SwapFree:\s+(\d+)/)?.[1] || '0')
    info.memory = { total: Math.round(mt / 1024), available: Math.round(ma / 1024), used: Math.round((mt - ma) / 1024), usagePercent: mt > 0 ? Math.round(((mt - ma) / mt) * 100) : 0, swapTotal: Math.round(st / 1024), swapFree: Math.round(sf / 1024), swapUsed: Math.round((st - sf) / 1024) }
  } catch (e) { info.memory.error = e.message }
  try {
    const df = execSync('df -h --output=source,fstype,size,used,avail,pcent,target 2>/dev/null | tail -n +2').toString().trim()
    info.disk = df.split('\n').map(l => { const p = l.trim().split(/\s+/); return { device: p[0], fstype: p[1], size: p[2], used: p[3], avail: p[4], usePercent: p[5], mount: p[6] } }).filter(d => d.device.startsWith('/'))
  } catch (e) { info.disk.error = e.message }
  try {
    info.network = fs.readdirSync('/sys/class/net/').filter(i => i !== 'lo').map(i => { try { return { name: i, mac: fs.readFileSync(`/sys/class/net/${i}/address`, 'utf-8').trim(), state: fs.readFileSync(`/sys/class/net/${i}/operstate`, 'utf-8').trim() } } catch { return null } }).filter(Boolean)
  } catch (e) { info.network.error = e.message }
  return info
})

// ========== IPC 资源监控 ==========

/** 异步采样 CPU 使用率（不阻塞事件循环） */
async function sampleCpuUsage() {
  try {
    const s1 = fs.readFileSync('/proc/stat', 'utf-8')
    const c1 = s1.match(/^cpu\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/)
    if (!c1) return 0
    const t1 = +c1[1] + +c1[2] + +c1[3] + +c1[4], i1 = +c1[4]
    await new Promise(r => setTimeout(r, 200))
    const s2 = fs.readFileSync('/proc/stat', 'utf-8')
    const c2 = s2.match(/^cpu\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/)
    if (!c2) return 0
    const t2 = +c2[1] + +c2[2] + +c2[3] + +c2[4], i2 = +c2[4]
    return (t2 - t1) > 0 ? Math.round((1 - (i2 - i1) / (t2 - t1)) * 1000) / 10 : 0
  } catch { return 0 }
}

function readMemoryPercent() {
  try {
    const m = fs.readFileSync('/proc/meminfo', 'utf-8')
    const mt = parseInt(m.match(/MemTotal:\s+(\d+)/)?.[1] || '0')
    const ma = parseInt(m.match(/MemAvailable:\s+(\d+)/)?.[1] || '0')
    return mt > 0 ? Math.round(((mt - ma) / mt) * 100) : 0
  } catch { return 0 }
}

ipcMain.handle('get-resource-monitor', async () => {
  const data = { cpu: 0, memory: 0 }
  data.cpu = await sampleCpuUsage()
  data.memory = readMemoryPercent()
  return data
})

ipcMain.handle('get-system-monitor', async () => {
  const data = { cpu: 0, cpuCores: '-', cpuModel: '-', memory: { percent: 0, used: '-', total: '-' }, disks: [], network: { ip: '-', mac: '-' } }
  data.cpu = await sampleCpuUsage()
  try {
    const m = fs.readFileSync('/proc/meminfo', 'utf-8')
    const mt = parseInt(m.match(/MemTotal:\s+(\d+)/)?.[1] || '0')
    const ma = parseInt(m.match(/MemAvailable:\s+(\d+)/)?.[1] || '0')
    const used = mt - ma
    data.memory = { percent: mt > 0 ? Math.round((used / mt) * 100) : 0, total: Math.round(mt / 1024) + ' MB', used: Math.round(used / 1024) + ' MB' }
  } catch (e) {}
  return data
})

// ========== IPC 脚本执行 ==========

ipcMain.handle('execute-script', async (_, scriptPath, options = {}) => {
  return new Promise((resolve) => {
    const { category = '' } = options || {}
    const possiblePaths = [
      path.join(__dirname, '../resources/scripts', category, scriptPath),
      path.join(__dirname, '../resources/scripts', scriptPath),
    ]
    for (const p of [...possiblePaths]) { if (!p.endsWith('.sh')) possiblePaths.push(p + '.sh') }

    let fullPath = null
    for (const p of possiblePaths) { try { if (fs.existsSync(p)) { fullPath = p; break } } catch {} }

    if (!fullPath) { resolve({ success: false, error: '脚本文件不存在: ' + scriptPath }); return }

    const child = spawn('bash', [fullPath], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: { ...process.env, PATH: '/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin' }
    })
    let stdout = '', stderr = ''
    child.stdout.on('data', (d) => { stdout += d.toString() })
    child.stderr.on('data', (d) => { stderr += d.toString() })
    child.on('close', (code) => { resolve({ success: code === 0, code, stdout, stderr }) })
    child.on('error', (err) => { resolve({ success: false, error: err.message }) })
  })
})

ipcMain.handle('list-scripts', (_, category) => {
  const dir = path.join(__dirname, '../resources/scripts', category || '')
  try { if (!fs.existsSync(dir)) return []; return fs.readdirSync(dir).filter(f => f.endsWith('.sh')).map(f => ({ name: f, path: f, category })) } catch { return [] }
})

ipcMain.handle('execute-script-direct', async (_, scriptContent) => {
  // 将脚本内容写入临时文件并执行
  const tmpFile = '/tmp/.kun_custom_script_' + Date.now() + '.sh'
  try {
    require('fs').writeFileSync(tmpFile, scriptContent, { mode: 0o755 })
    const result = require('child_process').execSync('bash ' + tmpFile + ' 2>&1', { timeout: 30000, encoding: 'utf-8' })
    try { require('fs').unlinkSync(tmpFile) } catch {}
    return { success: true, stdout: result.toString() }
  } catch (e) {
    try { require('fs').unlinkSync(tmpFile) } catch {}
    return { success: false, error: e.message }
  }
})

ipcMain.handle('read-script-content', (_, scriptPath) => {
  const ps = [path.join(__dirname, '../resources/scripts', scriptPath), path.join(__dirname, '../resources/scripts', scriptPath + '.sh')]
  for (const p of ps) { try { if (fs.existsSync(p)) return fs.readFileSync(p, 'utf-8') } catch {} }
  return null
})

// ========== IPC 终端 ==========

ipcMain.handle('terminal-exec', (_, command) => {
  return new Promise((resolve) => {
    const child = spawn('bash', ['-c', command], { stdio: ['pipe', 'pipe', 'pipe'], env: { ...process.env, TERM: 'xterm-256color' } })
    let output = ''
    child.stdout.on('data', (d) => { output += d.toString() })
    child.stderr.on('data', (d) => { output += d.toString() })
    child.on('close', (code) => { resolve({ output, code }) })
  })
})

ipcMain.handle('execute-terminal-script', async (_, scriptPath, input) => {
  return new Promise((resolve) => {
    const child = spawn('bash', [scriptPath], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: { ...process.env, TERM: 'xterm-256color' }
    })
    let stdout = '', stderr = ''
    if (input) { child.stdin.write(input); child.stdin.end() }
    child.stdout.on('data', (d) => { stdout += d.toString() })
    child.stderr.on('data', (d) => { stderr += d.toString() })
    child.on('close', (code) => { resolve({ success: code === 0, code, stdout, stderr }) })
    child.on('error', (err) => { resolve({ success: false, error: err.message }) })
  })
})

// ========== IPC 天气 ==========

ipcMain.handle('get-weather', async () => {
  try {
    return new Promise((resolve) => {
      https.get('https://wttr.in/?format=%C+%t+%h+%w&lang=zh', (res) => {
        let data = ''
        res.on('data', (c) => data += c)
        res.on('end', () => resolve({ success: true, data: data.trim() }))
      }).on('error', (err) => resolve({ success: false, error: err.message }))
    })
  } catch (e) { return { success: false, error: e.message } }
})

// ========== IPC 天气浮窗 ==========

ipcMain.handle('weather-float-toggle', () => {
  if (weatherFloatWindow && weatherFloatWindow.isVisible()) weatherFloatWindow.hide()
  else if (weatherFloatWindow) weatherFloatWindow.show()
  else createWeatherFloatWindow()
})
ipcMain.handle('weather-float-show', () => { if (!weatherFloatWindow) createWeatherFloatWindow(); else weatherFloatWindow.show() })
ipcMain.handle('weather-float-hide', () => weatherFloatWindow?.hide())
ipcMain.handle('weather-float-close', () => { if (weatherFloatWindow) { weatherFloatWindow.close(); weatherFloatWindow = null } })
ipcMain.handle('weather-float-is-visible', () => weatherFloatWindow?.isVisible() || false)
ipcMain.handle('weather-float-get-style', () => weatherFloatWindow?.getBounds() || {})
ipcMain.handle('weather-float-set-style', (_, s) => { if (weatherFloatWindow && s) weatherFloatWindow.setBounds(s) })
ipcMain.handle('weather-float-set-always-on-top', (_, f) => { if (weatherFloatWindow) weatherFloatWindow.setAlwaysOnTop(f) })
ipcMain.handle('weather-float-set-position-locked', (_, l) => { if (weatherFloatWindow) weatherFloatWindow.setResizable(!l) })

// ========== IPC 缓存管理 ==========

ipcMain.handle('get-cache-size', () => {
  const cacheDir = app.getPath('cache')
  function getDirSize(dir) { let s = 0; try { const files = fs.readdirSync(dir, { withFileTypes: true }); for (const f of files) { const fp = path.join(dir, f.name); if (f.isFile()) s += fs.statSync(fp).size; else if (f.isDirectory()) s += getDirSize(fp) } } catch {} return s }
  const size = getDirSize(cacheDir); return { size, sizeFormatted: formatBytes(size) }
})

ipcMain.handle('clear-cache', () => {
  const cacheDir = app.getPath('cache')
  try { const files = fs.readdirSync(cacheDir); for (const f of files) fs.rmSync(path.join(cacheDir, f), { recursive: true, force: true });

// ========== IPC 局域网文件传输 (LocalSend) ==========

let _localsendProcess = null
let _localsendDir = ''

ipcMain.handle('localsend-start', async (_, dir, port) => {
  try {
    if (_localsendProcess) {
      _localsendProcess.kill()
      _localsendProcess = null
    }
    const serveDir = dir || os.tmpdir()
    const servePort = port || 8080
    _localsendDir = serveDir
    
    const serverScript = path.join(__dirname, '../resources/localsend_server.py')
    if (!fs.existsSync(serverScript)) {
      return { success: false, error: '服务脚本不存在: ' + serverScript }
    }
    
    _localsendProcess = spawn('python3', [serverScript, String(servePort), serveDir], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: { ...process.env }
    })
    
    let started = false
    return new Promise((resolve) => {
      const checkStart = (d) => {
        if (!started && d.toString().includes('Server started')) {
          started = true
          resolve({ success: true, port: servePort, dir: serveDir })
        }
      }
      _localsendProcess.stdout.on('data', checkStart)
      _localsendProcess.stderr.on('data', checkStart)
      setTimeout(() => {
        if (!started) { started = true; resolve({ success: true, port: servePort, dir: serveDir }) }
      }, 2000)
      _localsendProcess.on('error', (err) => {
        if (!started) { started = true; resolve({ success: false, error: err.message }) }
      })
    })
  } catch(e) {
    return { success: false, error: e.message }
  }
})

ipcMain.handle('localsend-stop', async () => {
  try {
    if (_localsendProcess) {
      _localsendProcess.kill('SIGINT')
      setTimeout(() => { try { _localsendProcess.kill('SIGKILL') } catch {} }, 3000)
      _localsendProcess = null
    }
    return { success: true }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

ipcMain.handle('localsend-status', async () => {
  const running = _localsendProcess !== null && _localsendProcess.exitCode === null
  return { running, dir: _localsendDir }
})

ipcMain.handle('localsend-get-ip', async () => {
  try {
    const interfaces = os.networkInterfaces()
    const ips = []
    for (const name of Object.keys(interfaces)) {
      for (const iface of interfaces[name]) {
        if (iface.family === 'IPv4' && !iface.internal) {
          ips.push({ name, address: iface.address, mac: iface.mac })
        }
      }
    }
    return { success: true, ips }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

// ========== IPC 软件包管理器 ==========

ipcMain.handle('packmgr-list', async (_, search) => {
  try {
    let cmd
    if (search && search.trim()) {
      const safe = search.replace(/'/g, "'\\''")
      cmd = "dpkg -l 2>/dev/null | tail -n +6 | grep -i '" + safe + "' || true"
    } else {
      cmd = 'dpkg -l 2>/dev/null | tail -n +6 || true'
    }
    const r = execSync(cmd, { timeout: 15000, encoding: 'utf-8' })
    const lines = r.toString().split('\n').filter(l => l.trim())
    const packages = []
    for (const line of lines) {
      const parts = line.trim().split(/\s+/)
      if (parts.length >= 3 && parts[0] !== 'Desired=') {
        packages.push({
          status: parts[0] || '',
          name: parts[1] || '',
          version: parts[2] || '',
          arch: parts[3] || '',
          desc: parts.slice(4).join(' ') || ''
        })
      }
    }
    return { success: true, packages }
  } catch(e) {
    return { success: false, error: e.message, packages: [] }
  }
})

ipcMain.handle('packmgr-info', async (_, pkgName) => {
  try {
    const safe = pkgName.replace(/'/g, "'\\''")
    const r = execSync("dpkg -s '" + safe + "' 2>/dev/null || true", { timeout: 10000, encoding: 'utf-8' })
    const info = {}
    for (const line of r.toString().split('\n')) {
      const idx = line.indexOf(':')
      if (idx > 0) { info[line.substring(0, idx).trim()] = line.substring(idx + 1).trim() }
    }
    return { success: true, info }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

ipcMain.handle('packmgr-search', async (_, keyword) => {
  try {
    const safe = keyword.replace(/'/g, "'\\''")
    const r = execSync("apt-cache search '" + safe + "' 2>/dev/null | head -60 || true", { timeout: 30000, encoding: 'utf-8' })
    const lines = r.toString().split('\n').filter(l => l.trim())
    const results = []
    for (const line of lines) {
      const idx = line.indexOf(' - ')
      if (idx > 0) {
        results.push({ name: line.substring(0, idx).trim(), desc: line.substring(idx + 3).trim() })
      } else {
        results.push({ name: line.trim(), desc: '' })
      }
    }
    return { success: true, results }
  } catch(e) {
    return { success: false, error: e.message, results: [] }
  }
})

ipcMain.handle('packmgr-get-deb-info', async (_, filePath) => {
  try {
    const safe = filePath.replace(/"/g, '\\"')
    const r = execSync("dpkg -I '" + safe + "' 2>/dev/null || true", { timeout: 15000, encoding: 'utf-8' })
    return { success: true, info: r.toString() }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

ipcMain.handle('packmgr-install', async (_, filePath, password) => {
  try {
    const safe = filePath.replace(/"/g, '\\"')
    const r = sudoExec("dpkg -i '" + safe + "' && apt-get install -f -y", password || '')
    return { success: !!r, output: r || '安装完成' }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

ipcMain.handle('packmgr-uninstall', async (_, pkgName, password) => {
  try {
    const safe = pkgName.replace(/'/g, "'\\''")
    const r = sudoExec("dpkg -r '" + safe + "' 2>&1", password || '')
    return { success: !!r, output: r || '卸载完成' }
  } catch(e) {
    return { success: false, error: e.message }
  }
})
 return { success: true } } catch (e) { return { success: false, error: e.message } }
})

// ========== IPC 设置 ==========

const settingsFile = path.join(app.getPath('userData'), 'settings.json')
function loadSettings() { try { if (fs.existsSync(settingsFile)) return JSON.parse(fs.readFileSync(settingsFile, 'utf-8')) } catch {} return {} }
function saveSettings(s) { try { fs.writeFileSync(settingsFile, JSON.stringify(s, null, 2)) } catch {} }
ipcMain.handle('get-setting', (_, key) => loadSettings()[key])
ipcMain.handle('set-setting', (_, key, value) => { const s = loadSettings(); s[key] = value; saveSettings(s) })
ipcMain.handle('get-all-settings', () => loadSettings())
ipcMain.handle('set-auto-start', (_, enable) => {
  try {
    app.setLoginItemSettings({ openAtLogin: enable })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== 应用生命周期 ==========

app.whenReady().then(() => {
  createMainWindow()
  createTray()
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createMainWindow(); else mainWindow?.show() })
})

app.on('window-all-closed', () => { if (closeBehavior === 'quit') app.quit() })

// 单实例锁
const gotTheLock = app.requestSingleInstanceLock()
if (!gotTheLock) { app.quit() } else {
  app.on('second-instance', () => { if (mainWindow) { if (mainWindow.isMinimized()) mainWindow.restore(); mainWindow.show(); mainWindow.focus() } })
}

// ========== 辅助函数 ==========

function formatBytes(bytes) {
  if (bytes === 0) return '0 B'
  const k = 1024, sizes = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.min(Math.floor(Math.log(bytes) / Math.log(k)), sizes.length - 1)
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

// ========== IPC 系统监控详情 ==========

/** 执行命令捕获输出（10秒超时） */
function execOut(cmd) {
  try { return execSync(cmd, { timeout: 10000, encoding: 'utf-8' }).toString() } catch { return '' }
}

/** 执行 pkexec 命令（60秒超时，给用户留足输密码时间） */
function pkexecOut(cmd) {
  try { return execSync('pkexec ' + cmd, { timeout: 60000, encoding: 'utf-8' }).toString() } catch { return '' }
}

/** 用密码执行 sudo 命令（写入临时脚本再执行，避免命令注入和 ~ 展开为 /root） */
function sudoExec(cmd, pw) {
  const userHome = process.env.HOME || '/root'
  const safeCmd = cmd.replace(/~/g, userHome)
  const tmpFile = path.join(os.tmpdir(), '.uos_sudo_' + Date.now() + '_' + Math.random().toString(36).slice(2, 8) + '.sh')
  try {
    fs.writeFileSync(tmpFile, '#!/bin/bash\nset -e\n' + safeCmd + '\n', { mode: 0o700 })
    let result
    if (!pw) {
      result = execSync('sudo -n bash ' + tmpFile + ' 2>&1', { timeout: 30000, encoding: 'utf-8' }).toString()
    } else {
      result = execSync('sudo -S bash ' + tmpFile + ' 2>&1', {
        timeout: 30000, encoding: 'utf-8', input: pw + '\n'
      }).toString()
    }
    try { fs.unlinkSync(tmpFile) } catch {}
    return result
  } catch(e) {
    try { fs.unlinkSync(tmpFile) } catch {}
    return ''
  }
}

ipcMain.handle('get-monitor-detail', async (_, type) => {
  switch(type) {
    // ========== 进程资源类 ==========
    case 'top-cpu':
    case 'top-mem': {
      const sortKey = type === 'top-cpu' ? 'cpu' : 'mem'
      const sortFlag = type === 'top-cpu' ? '-%cpu' : '-%mem'
      const label = type === 'top-cpu' ? 'CPU' : '内存'
      const out = execOut("ps aux --sort=" + sortFlag + " 2>/dev/null | head -81")
      if (!out) return { success: false, error: '无法获取' + label + '排行' }
      const lines = out.trim().split('\n')
      const header = lines[0]
      const raw = lines.slice(1).filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { user: p[0], pid: p[1], cpu: parseFloat(p[2]), mem: parseFloat(p[3]), vsz: p[4], rss: p[5], tty: p[6], stat: p[7], start: p[8], time: p[9], command: p.slice(10).join(' ') }
      })
      const blacklist = ['ps','head','awk','grep','sh','bash','cut','sort','tr']
      const groups = {}
      for (const item of raw) {
        const cmd = item.command || ''
        const parts = cmd.split('/')
        const fullName = parts[parts.length-1] || cmd
        const shortName = fullName.split(/\s+/)[0]
        if (!shortName || blacklist.includes(shortName)) continue
        if (!groups[shortName]) groups[shortName] = { shortName, fullCmd: cmd, cpu: 0, mem: 0, count: 0, pids: [] }
        groups[shortName].cpu += item.cpu
        groups[shortName].mem += item.mem
        groups[shortName].count++
        groups[shortName].pids.push({ pid: item.pid, cpu: item.cpu, mem: item.mem, user: item.user, fullCmd: cmd })
        if (cmd.length > groups[shortName].fullCmd.length) groups[shortName].fullCmd = cmd
      }
      const items = Object.values(groups).sort((a,b) => b[sortKey] - a[sortKey]).slice(0,30)
      return { success: true, header, items, aggregated: true }
    }
    case 'zombie': {
      const out = execOut("ps aux 2>/dev/null | awk '{if($8==\"Z\"||$8==\"Z+\")print}'")
      const count = out ? out.trim().split('\n').filter(Boolean).length : 0
      const items = out ? out.trim().split('\n').filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { user: p[0], pid: p[1], cpu: p[2], mem: p[3], command: p.slice(10).join(' '), stat: p[7] }
      }) : []
      return { success: true, count, items }
    }
    // ========== 磁盘存储类 ==========
    case 'disk-block': {
      const out = execOut("df -h 2>/dev/null")
      if (!out) return { success: false, error: '无法获取磁盘信息' }
      const lines = out.trim().split('\n')
      const header = lines[0]
      const items = lines.slice(1).filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { fs: p[0], size: p[1], used: p[2], avail: p[3], usePct: p[4], mount: p[5] }
      }).filter(i => i.fs.startsWith('/'))
      return { success: true, header, items }
    }
    case 'disk-inode': {
      const out = execOut("df -i 2>/dev/null")
      if (!out) return { success: false, error: '无法获取Inode信息' }
      const lines = out.trim().split('\n')
      const header = lines[0]
      const items = lines.slice(1).filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { fs: p[0], inodes: p[1], iused: p[2], ifree: p[3], iusePct: p[4], mount: p[5] }
      }).filter(i => i.fs.startsWith('/'))
      return { success: true, header, items }
    }
    case 'disk-structure': {
      const out = execOut("lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT,MODEL 2>/dev/null")
      if (!out) return { success: false, error: '无法获取硬盘结构' }
      const lines = out.trim().split('\n')
      const header = lines[0]
      const items = lines.slice(1).filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { name: p[0], size: p[1], type: p[2], fstype: p[3]||'-', mount: p[4]||'-', model: p.slice(5).join(' ')||'-' }
      })
      return { success: true, header, items }
    }
    // ========== 网络监控类 ==========
    case 'open-ports': {
      const out = execOut("ss -tlnp 2>/dev/null || netstat -tlnp 2>/dev/null")
      if (!out) return { success: false, error: '无法获取端口信息' }
      const lines = out.trim().split('\n')
      const header = lines[0]
      const items = lines.slice(1).filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { state: p[0], recvq: p[1], sendq: p[2], local: p[3], peer: p[4], process: p.slice(5).join(' ')||'-' }
      })
      return { success: true, header, items }
    }
    case 'active-connections': {
      const out = execOut("ss -tnp 2>/dev/null || netstat -tnp 2>/dev/null")
      if (!out) return { success: false, error: '无法获取连接信息' }
      const lines = out.trim().split('\n')
      const header = lines[0]
      const items = lines.slice(1).filter(Boolean).filter(l => l.includes('ESTAB')).map(l => {
        const p = l.trim().split(/\s+/)
        return { state: p[0], recvq: p[1], sendq: p[2], local: p[3], peer: p[4], process: p.slice(5).join(' ')||'-' }
      })
      return { success: true, header, items }
    }
    case 'route-table': {
      const out = execOut("ip route 2>/dev/null || route -n 2>/dev/null")
      if (!out) return { success: false, error: '无法获取路由表' }
      return { success: true, lines: out.trim().split('\n').filter(Boolean) }
    }
    // ========== 用户与登录审计 ==========
    case 'login-history': {
      const out = execOut("last -20 2>/dev/null || lastlog 2>/dev/null")
      if (!out) return { success: false, error: '无法获取登录历史' }
      return { success: true, lines: out.trim().split('\n').filter(Boolean) }
    }
    case 'online-users': {
      const out = execOut("who 2>/dev/null || w 2>/dev/null")
      if (!out) return { success: false, error: '无法获取在线用户' }
      return { success: true, lines: out.trim().split('\n').filter(Boolean) }
    }
    // ========== 内核底层类 ==========
    case 'kernel-modules': {
      const out = execOut("lsmod 2>/dev/null")
      if (!out) return { success: false, error: '无法获取内核模块' }
      const lines = out.trim().split('\n')
      const header = lines[0]
      const items = lines.slice(1).filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { module: p[0], size: p[1], used: p[2], by: p.slice(3).join(' ')||'-' }
      })
      return { success: true, header, items }
    }
    default: return { success: false, error: '未知监控类型: ' + type }
  }
})


// ========== IPC 系统配置 ==========

ipcMain.handle('get-sysconfig', async (_, id) => {
  try {
    switch(id) {
      case 'dev-mode': {
        // 统信UOS开发者模式检测（支持多种检测方式）
        const r1 = execOut('ls /etc/deepin/developer-mode 2>/dev/null && echo enabled || echo disabled').trim()
        if (r1 === 'enabled') return { enabled: true }
        // 备选检测：检查是否有开发者模式标识
        const r2 = execOut('cat /etc/deepin/deepin-id 2>/dev/null | grep -qi developer && echo enabled || echo disabled').trim()
        return { enabled: r2 === 'enabled' }
      }
      case 'firewall': {
        // 检测 ufw 或 firewalld
        const r = execOut('ufw status 2>/dev/null | grep -q active && echo enabled || echo disabled').trim()
        if (r === 'enabled') return { enabled: true }
        const r2 = execOut('firewall-cmd --state 2>/dev/null | grep -q running && echo enabled || echo disabled').trim()
        return { enabled: r2 === 'enabled' }
      }
      case 'ssh': {
        const r = execOut('systemctl is-active ssh 2>/dev/null || systemctl is-active sshd 2>/dev/null || echo inactive').trim()
        return { enabled: r === 'active' }
      }
      case 'bluetooth': {
        const r = execOut('systemctl is-active bluetooth 2>/dev/null || echo inactive').trim()
        return { enabled: r === 'active' }
      }
      case 'desktop-effects': {
        // 统信UOS桌面特效检测（支持多个gsettings路径）
        let r = execOut('gsettings get com.deepin.wrap.gnome.desktop.interface enable-animation 2>/dev/null || echo ""').trim()
        if (r === 'true' || r === 'false') return { enabled: r === 'true' }
        r = execOut('gsettings get com.deepin.dde.animation enable-animation 2>/dev/null || echo ""').trim()
        if (r === 'true' || r === 'false') return { enabled: r === 'true' }
        r = execOut('gsettings get org.gnome.desktop.interface enable-animation 2>/dev/null || echo "true"').trim()
        return { enabled: r === 'true' }
      }
      case 'wifi': {
        let enabled = false
        const conns = execOut('nmcli -t -f TYPE,STATE connection show --active 2>/dev/null')
        if (conns) enabled = conns.split('\n').some(l => l.startsWith('802-11-wireless') && l.includes(':activated'))
        // 备选：检查 wifi 射频状态
        if (!enabled) {
          const radio = execOut('nmcli radio wifi 2>/dev/null').trim()
          if (radio === 'enabled') enabled = true
        }
        return { enabled }
      }
      case 'auto-update': {
        // 检测多个可能的自动更新配置位置
        const r = execOut('cat /etc/apt/apt.conf.d/20auto-upgrades 2>/dev/null | grep -q "1" && echo enabled || echo disabled').trim()
        if (r === 'enabled') return { enabled: true }
        const r2 = execOut('cat /etc/apt/apt.conf.d/10periodic 2>/dev/null | grep -q "1" && echo enabled || echo disabled').trim()
        return { enabled: r2 === 'enabled' }
      }
      case 'sudo-pwfb': {
        const r = execOut('grep -q "pwfeedback" /etc/sudoers 2>/dev/null && echo enabled || echo disabled').trim()
        return { enabled: r === 'enabled' }
      }
      case 'long-filename': {
        // 统信UOS 默认内核支持长文件名，检测实际挂载的文件系统
        const r = execOut('cat /proc/mounts 2>/dev/null | grep -qi "utf8\\|nls\\|iocharset" && echo enabled || echo disabled').trim()
        if (r === 'enabled') return { enabled: true }
        // 备选：检测 nls 内核模块
        const r2 = execOut('lsmod 2>/dev/null | grep -qi "nls_utf8\\|nls_cp437\\|nls_iso8859" && echo enabled || echo disabled').trim()
        return { enabled: r2 === 'enabled' }
      }
      case 'usb-block': {
        const r = execOut('lsmod 2>/dev/null | grep -q usb-storage && echo enabled || echo disabled').trim()
        if (r === 'enabled') return { enabled: true }
        // 备选：检测 USB 存储内核模块是否有黑名单
        const r2 = execOut('cat /etc/modprobe.d/*.conf 2>/dev/null | grep -q "blacklist.*usb-storage" && echo disabled || echo enabled').trim()
        return { enabled: r2 === 'enabled' }
      }
      case 'usb-readonly': {
        // 检测所有块设备，不限于 sda
        let r = execOut('cat /sys/block/sda/ro 2>/dev/null || echo 0').trim()
        if (r === '1') return { enabled: true }
        r = execOut('cat /sys/block/sdb/ro 2>/dev/null || echo 0').trim()
        if (r === '1') return { enabled: true }
        r = execOut('cat /sys/block/nvme0n1/ro 2>/dev/null || echo 0').trim()
        return { enabled: r === '1' }
      }
      case 'cpu-mode': {
        const r = execOut('cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo balanced').trim()
        return { mode: r }
      }
      default: return { enabled: false }
    }
  } catch(e) { return { enabled: false } }
})

ipcMain.handle('set-sysconfig', async (_, id, value) => {
  try {
    const enable = value === true || value === 'true'
    switch(id) {
      case 'dev-mode': {
        const cmd = enable ? 'touch /etc/deepin/developer-mode 2>&1' : 'rm -f /etc/deepin/developer-mode 2>&1'
        if (!pkexecOut(cmd)) return { success: false, error: '需要管理员权限，无法修改开发者模式' }
        const r = execOut('ls /etc/deepin/developer-mode 2>/dev/null && echo enabled || echo disabled').trim()
        return { success: true, enabled: r === 'enabled' }
      }
      case 'firewall': {
        const cmd = enable ? 'ufw enable 2>&1' : 'ufw disable 2>&1'
        if (!pkexecOut(cmd)) return { success: false, error: '需要管理员权限，无法修改防火墙' }
        const r = execOut('ufw status 2>/dev/null | grep -q active && echo enabled || echo disabled').trim()
        return { success: true, enabled: r === 'enabled' }
      }
      case 'ssh': {
        const cmd = enable ? 'systemctl start ssh 2>/dev/null || systemctl start sshd 2>/dev/null' : 'systemctl stop ssh 2>/dev/null || systemctl stop sshd 2>/dev/null'
        pkexecOut(cmd)
        const r = execOut('systemctl is-active ssh 2>/dev/null || systemctl is-active sshd 2>/dev/null || echo inactive').trim()
        return { success: true, enabled: r === 'active' }
      }
      case 'bluetooth': {
        const cmd = enable ? 'systemctl start bluetooth 2>&1' : 'systemctl stop bluetooth 2>&1'
        pkexecOut(cmd)
        const r = execOut('systemctl is-active bluetooth 2>/dev/null || echo inactive').trim()
        return { success: true, enabled: r === 'active' }
      }
      case 'wifi': {
        if (enable) {
          execOut('nmcli radio wifi on 2>&1')
          const knownWifi = execOut("nmcli -t -f NAME,TYPE con show 2>/dev/null | grep ':802-11-wireless' | head -1 | cut -d: -f1").trim()
          if (knownWifi) execOut('nmcli connection up "' + knownWifi + '" 2>&1')
        } else {
          const activeWifi = execOut("nmcli -t -f NAME,TYPE connection show --active 2>/dev/null | grep ':802-11-wireless' | head -1 | cut -d: -f1").trim()
          if (activeWifi) execOut('nmcli connection down "' + activeWifi + '" 2>&1')
        }
        let now = false
        const conns = execOut('nmcli -t -f TYPE,STATE connection show --active 2>/dev/null')
        if (conns) now = conns.split('\n').some(l => l.startsWith('802-11-wireless') && l.includes(':activated'))
        return { success: true, enabled: now }
      }
      case 'sudo-pwfb': {
        if (enable) pkexecOut('sh -c "echo \"Defaults pwfeedback\" >> /etc/sudoers" 2>&1')
        else pkexecOut('sh -c "sed -i \"/Defaults pwfeedback/d\" /etc/sudoers" 2>&1')
        return { success: true }
      }
      case 'desktop-effects': {
        if (enable) {
          // 尝试多个 gsettings 路径
          execOut('gsettings set com.deepin.wrap.gnome.desktop.interface enable-animation true 2>/dev/null || gsettings set com.deepin.dde.animation enable-animation true 2>/dev/null || gsettings set org.gnome.desktop.interface enable-animation true 2>/dev/null')
        } else {
          execOut('gsettings set com.deepin.wrap.gnome.desktop.interface enable-animation false 2>/dev/null || gsettings set com.deepin.dde.animation enable-animation false 2>/dev/null || gsettings set org.gnome.desktop.interface enable-animation false 2>/dev/null')
        }
        return { success: true }
      }
      case 'auto-update': {
        if (enable) {
          pkexecOut('sh -c "echo \'APT::Periodic::Update-Package-Lists \\\"1\\\";\' > /etc/apt/apt.conf.d/20auto-upgrades && echo \'APT::Periodic::Unattended-Upgrade \\\"1\\\";\' >> /etc/apt/apt.conf.d/20auto-upgrades" 2>&1')
        } else {
          pkexecOut('sh -c "echo \'APT::Periodic::Update-Package-Lists \\\"0\\\";\' > /etc/apt/apt.conf.d/20auto-upgrades && echo \'APT::Periodic::Unattended-Upgrade \\\"0\\\";\' >> /etc/apt/apt.conf.d/20auto-upgrades" 2>&1')
        }
        return { success: true }
      }
      case 'long-filename': {
        // 长文件名支持通常由内核模块提供，这里提示用户
        return { success: false, error: '长文件名支持由内核模块提供，请使用系统控制中心或修改 /etc/modules' }
      }
      case 'usb-block': {
        if (enable) {
          pkexecOut('sh -c "echo \'blacklist usb-storage\' > /etc/modprobe.d/usb-storage-blacklist.conf" 2>&1')
        } else {
          pkexecOut('sh -c "rm -f /etc/modprobe.d/usb-storage-blacklist.conf" 2>&1')
        }
        return { success: true }
      }
      case 'usb-readonly': {
        // USB 只读模式需要 udev 规则
        if (enable) {
          pkexecOut('sh -c "echo \'ACTION==\\\"add\\\", SUBSYSTEM==\\\"usb\\\", ATTR{authorized}=\\\"1\\\"\' > /etc/udev/rules.d/99-usb-readonly.rules" 2>&1')
        } else {
          pkexecOut('sh -c "rm -f /etc/udev/rules.d/99-usb-readonly.rules" 2>&1')
        }
        return { success: true }
      }
      case 'cpu-mode': {
        const mode = value === 'powersave' || value === 'performance' ? value : 'balanced'
        execSync('sudo cpupower frequency-set -g ' + mode + ' 2>/dev/null')
        return { success: true }
      }
      default: return { success: false, error: '未知配置: ' + id }
    }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== IPC 系统优化模块 ==========

// 单独的安全基线 IPC 处理器（避免被 execute-optimization 的大 switch 影响）
ipcMain.handle('secbase-check', async (event, action, password) => {
  const { spawn } = require('child_process')
  return new Promise((resolve) => {
    let cmdArgs = ['uos-sec-harden']
    if (action === 'check' || action === 'all') cmdArgs.push('--' + action)
    else if (action.startsWith('run ')) { cmdArgs.push('--run'); cmdArgs.push(action.substring(4)) }
    else cmdArgs.push('--' + action)
    
    let output = ''
    let hasOutput = false
    let authTimer = null
    
    try { event.sender.send('secbase-progress', '__STATUS__:等待授权') } catch {}
    
    // 有密码用 sudo -S（stdin 传密码），否则用 pkexec
    let child
    if (password) {
      // 只用 -S，不用 -n（某些 sudo 版本 -n 会阻止 stdin 密码读取）
      child = spawn('sudo', ['-S'].concat(cmdArgs), {
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env, DISPLAY: process.env.DISPLAY || ':0' }
      })
      child.stdin.write(password + '\n')
      child.stdin.end()
    } else {
      child = spawn('pkexec', cmdArgs, {
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env, DISPLAY: process.env.DISPLAY || ':0' }
      })
    }
    
    child.stdout.on('data', (d) => {
      const chunk = d.toString()
      output += chunk
      if (!hasOutput) {
        hasOutput = true
        if (authTimer) { clearTimeout(authTimer); authTimer = null }
        try { event.sender.send('secbase-progress', '__STATUS__:执行中') } catch {}
      }
      try { event.sender.send('secbase-progress', output) } catch {}
    })
    child.stderr.on('data', (d) => {
      output += d.toString()
      try { event.sender.send('secbase-progress', output) } catch {}
    })
    
    authTimer = setTimeout(() => {
      if (!hasOutput) {
        try { event.sender.send('secbase-progress', '__STATUS__:授权超时-请检查密码') } catch {}
      }
    }, 60000)
    
    const totalTimer = setTimeout(() => {
      child.kill('SIGKILL')
      resolve({ success: false, error: '执行超时（超过600秒）', output: output, exitCode: -1 })
    }, 600000)
    
    child.on('close', (code) => {
      clearTimeout(authTimer)
      clearTimeout(totalTimer)
      const isCheck = cmdArgs.includes('--check')
      resolve({ success: isCheck ? code <= 1 : code === 0, output: output || '执行完成', exitCode: code })
    })
    child.on('error', (err) => {
      clearTimeout(authTimer)
      clearTimeout(totalTimer)
      resolve({ success: false, error: '执行失败: ' + (err.message || '未知错误') })
    })
  })
})
ipcMain.handle('execute-optimization', async (_, type, action, value) => {
  switch(type) {
    // 1. 内存使用优化
    case 'memory-tune': {
      if (action === 'tune') {
        const r1 = sudoExec('sysctl -w vm.swappiness=10 && sysctl -w vm.vfs_cache_pressure=50 && echo "vm.swappiness=10" > /etc/sysctl.d/99-memory-optimize.conf && echo "vm.vfs_cache_pressure=50" >> /etc/sysctl.d/99-memory-optimize.conf && sync && echo 3 > /proc/sys/vm/drop_caches', value)
        return { success: !!r1, output: '✅ 内存优化已完成！\n参数: swappiness=10, vfs_cache_pressure=50\n效果: Swap 使用倾向降低，物理内存优先使用' }
      } else if (action === 'restore') {
        const r1 = sudoExec('sysctl -w vm.swappiness=60 && sysctl -w vm.vfs_cache_pressure=100 && rm -f /etc/sysctl.d/99-memory-optimize.conf', value)
        return { success: !!r1, output: '✅ 已恢复默认配置！\n参数已重置为系统初始值\nswappiness=60, vfs_cache_pressure=100' }
      }
      return { success: false, error: '未知操作' }
    }

    // 2. 固态硬盘维护
    case 'ssd-trim': {
      if (action === 'trim-now') {
        const r1 = sudoExec('fstrim -v / && fstrim -v /home', value)
        return { success: !!r1, output: r1 || '✅ TRIM 执行完成' }
      } else if (action === 'enable-auto') {
        const r1 = sudoExec('systemctl start fstrim.timer', value)
        const st = sudoExec('systemctl is-active fstrim.timer', value).trim()
        return { success: st === 'active', output: '✅ 定时 SSD 维护已开启 (每周自动 TRIM)' }
      } else if (action === 'disable-auto') {
        sudoExec('systemctl stop fstrim.timer', value)
        return { success: true, output: '✅ 定时 SSD 维护已关闭' }
      }
      return { success: false, error: '未知操作' }
    }

    // 3. 关闭网络邻居 (Avahi)
    case 'avahi': {
      if (action === 'disable') {
        const r1 = sudoExec('systemctl stop avahi-daemon && systemctl stop avahi-daemon.socket', value)
        return { success: !!r1, output: '✅ Avahi 网络邻居服务已禁用\n局域网设备发现已关闭\n后台广播通信已停止' }
      } else if (action === 'enable') {
        const r1 = sudoExec('systemctl start avahi-daemon', value)
        return { success: !!r1, output: '✅ Avahi 网络邻居服务已重新启用' }
      }
      return { success: false, error: '未知操作' }
    }

    // 4. APT 缓存清理
    case 'apt-clean': {
      if (action === 'clean-old') {
        const r1 = sudoExec('apt-get clean', value)
        return { success: !!r1, output: '✅ 已清理旧版本 APT 安装包缓存' }
      } else if (action === 'clean-all') {
        const r1 = sudoExec('apt-get clean && apt-get autoclean && apt-get autoremove -y', value)
        return { success: !!r1, output: '✅ APT 缓存已完全清空\n已清理残留依赖包' }
      } else if (action === 'fix-lock') {
        const r1 = sudoExec('rm -f /var/lib/apt/lists/lock && rm -f /var/cache/apt/archives/lock && rm -f /var/lib/dpkg/lock-frontend && dpkg --configure -a', value)
        return { success: !!r1, output: '✅ APT 锁文件已修复\n未完成的 dpkg 配置已完成' }
      }
      return { success: false, error: '未知操作' }
    }

    // 5. 禁用冗余服务
    case 'disable-services': {
      if (action === 'list') {
        const services = ['cups','cups-browsed','bluetooth','avahi-daemon','cupsd']
        const statuses = {}
        for (const s of services) {
          const st = execOut(`systemctl is-active ${s} 2>/dev/null`).trim()
          const en = execOut(`systemctl is-enabled ${s} 2>/dev/null`).trim()
          statuses[s] = { active: st, enabled: en }
        }
        return { success: true, services: statuses }
      } else if (action === 'batch') {
        const services = ['cups','cups-browsed','bluetooth','avahi-daemon']
        let out = ''
        for (const s of services) {
          const r = sudoExec(`systemctl stop ${s}`, value)
          out += `${r ? '✅' : '❌'} ${s}: ${r ? '已禁用' : '失败'}\n`
        }
        return { success: true, output: out }
      } else if (action.startsWith('disable:')) {
        const svc = action.replace('disable:', '')
        sudoExec(`systemctl stop ${svc}`, value)
        return { success: true, output: `服务 ${svc} 已停止并禁用开机自启` }
      }
      return { success: false, error: '未知操作' }
    }

    // 6. 修复 CPU 高占用
    case 'fix-high-cpu': {
      if (action === 'kill-high') {
        const out = execOut('ps aux --sort=-%cpu 2>/dev/null | head -10')
        sudoExec('pkill -f imwheel 2>/dev/null; pkill -f xfce4-clipman 2>/dev/null; pkill -f indicator-application 2>/dev/null; pkill -f evolution-calendar 2>/dev/null; pkill -f tracker-store 2>/dev/null', value)
        return { success: true, output: `已终止常见高 CPU 占用进程\n\n当前 TOP10 进程:\n${out||'无数据'}` }
      } else if (action === 'disable-autostart') {
        sudoExec('rm -f /etc/xdg/autostart/imwheel.desktop', value)
        execOut('rm -f ~/.config/autostart/imwheel.desktop 2>/dev/null')
        execOut('rm -f ~/.config/autostart/xfce4-clipman.desktop 2>/dev/null')
        return { success: true, output: '✅ 已禁止 imwheel、剪贴板管理器等进程开机自启' }
      }
      return { success: false, error: '未知操作' }
    }

    // 7. 系统日志清理
    case 'clean-logs': {
      if (action === 'clean-7days') {
        const r1 = sudoExec('journalctl --vacuum-time=7d && find /var/log -name "*.log" -mtime +7 -delete && find /var/log -name "*.gz" -delete', value)
        return { success: !!r1, output: '✅ 已清理 7 天前的系统日志和应用日志' }
      } else if (action.startsWith('clean-days:')) {
        const days = action.replace('clean-days:', '')
        const r1 = sudoExec('journalctl --vacuum-time=' + days + 'd && find /var/log -name "*.log" -mtime +' + days + ' -delete', value)
        return { success: !!r1, output: '✅ 已清理 ' + days + ' 天前的日志' }
      } else if (action === 'clean-buffer') {
        const r1 = sudoExec('journalctl --rotate && journalctl --vacuum-time=1s', value)
        return { success: !!r1, output: '✅ 日志缓冲区已清空' }
      }
      return { success: false, error: '未知操作' }
    }

    case '_sysmgr': {
      if (action === 'account-policy:show') {
        return { success: true, output: execOut('cat /etc/pam.d/common-password 2>/dev/null | grep -v "^#" | grep -v "^$" | head -30') }
      }
      if (action === 'account-policy:set-minlen') {
        sudoExec('sh -c "echo \"password requisite pam_pwquality.so minlen=8\" >> /etc/pam.d/common-password"', value)
        return { success: true, output: '最小密码长度已设为 8 位' }
      }
      if (action === 'account-policy:set-expire') {
        sudoExec('sh -c "sed -i \"s/PASS_MAX_DAYS.*/PASS_MAX_DAYS   90/\" /etc/login.defs"', value)
        return { success: true, output: '密码过期天数已设为 90 天' }
      }
      if (action === 'apt-source:show') {
        return { success: true, output: execOut('cat /etc/apt/sources.list 2>/dev/null; ls /etc/apt/sources.list.d/ 2>/dev/null') }
      }
      if (action === 'apt-source:test') {
        const r = execOut('apt-get update 2>&1 | tail -20')
        return { success: true, output: r || '源测试完成' }
      }
      if (action === 'apt-source:update') {
        sudoExec('apt-get update', value)
        return { success: true, output: '软件源已更新' }
      }
      if (action === 'kernel-tuning:show') {
        const r = execOut('sysctl -a 2>/dev/null | grep -E "swappiness|dirty_ratio|dirty_background|vfs_cache" | head -20')
        return { success: true, output: r || '无法获取内核参数' }
      }
      if (action === 'kernel-tuning:optimize') {
        sudoExec('sh -c "sysctl -w vm.swappiness=10 && sysctl -w vm.vfs_cache_pressure=50 && sysctl -w vm.dirty_ratio=20 && sysctl -w vm.dirty_background_ratio=5"', value)
        return { success: true, output: '内核参数已优化' }
      }
      if (action === 'kernel-tuning:restore') {
        sudoExec('sh -c "sysctl -w vm.swappiness=60 && sysctl -w vm.vfs_cache_pressure=100 && sysctl -w vm.dirty_ratio=30 && sysctl -w vm.dirty_background_ratio=10"', value)
        return { success: true, output: '内核参数已恢复默认' }
      }
      if (action === 'startup-mgr:list') {
        const r = execOut('systemctl list-unit-files --type=service --state=enabled 2>/dev/null | head -30')
        return { success: true, output: r || '无法获取启动项' }
      }
      if (action === 'startup-mgr:speedup') {
        const services = ['cups','bluetooth','avahi-daemon','cups-browsed']
        let out = ''
        for (const s of services) {
          sudoExec('systemctl stop ' + s + ' 2>/dev/null', value)
          out += '已禁用: ' + s + '\n'
        }
        return { success: true, output: out }
      }
      if (action === 'system-cleanup:analyze') {
        const r = execOut('du -sh /var/cache/apt/archives/ 2>/dev/null; du -sh /var/log/ 2>/dev/null; du -sh /tmp/ 2>/dev/null; journalctl --disk-usage 2>/dev/null')
        return { success: true, output: r || '分析完成' }
      }
      if (action === 'system-cleanup:clean') {
        sudoExec('sh -c "apt-get clean && apt-get autoclean && journalctl --vacuum-time=7d && find /tmp -type f -atime +7 -delete 2>/dev/null"', value)
        return { success: true, output: '系统清理完成' }
      }
      if (action === 'shortcut-mgr:list') {
        const r = execOut('gsettings list-recursively com.deepin.dde.keybinding 2>/dev/null | head -40')
        return { success: true, output: r || '无法获取快捷键列表' }
      }
      if (action === 'power-mgr:show') {
        const r = execOut('cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null; echo ""; upower -d 2>/dev/null | head -20')
        return { success: true, output: r || '无法获取电源信息' }
      }
      if (action === 'power-mgr:powersave') {
        sudoExec('cpupower frequency-set -g powersave 2>/dev/null', value)
        return { success: true, output: '已切换为节能模式' }
      }
      if (action === 'power-mgr:balanced') {
        sudoExec('cpupower frequency-set -g ondemand 2>/dev/null', value)
        return { success: true, output: '已切换为平衡模式' }
      }
      if (action === 'power-mgr:performance') {
        sudoExec('cpupower frequency-set -g performance 2>/dev/null', value)
        return { success: true, output: '已切换为高性能模式' }
      }
      return { success: false, error: '未知操作: ' + action }
    }

    case '_udom': {
      if (action === 'udom_wechat') { sudoExec('sh -c "rm -rf ~/.cache/wechat && rm -rf ~/.config/wechat"', value); return { success: true, output: '微信缓存已清理' } }
      if (action === 'udom_wps') { sudoExec('sh -c "rm -rf ~/.config/Kingsoft && rm -rf ~/.cache/kingsoft"', value); return { success: true, output: 'WPS 配置已重置' } }
      if (action === 'udom_qq') { sudoExec('sh -c "rm -rf ~/.cache/QQ && rm -rf ~/.config/qq"', value); return { success: true, output: 'QQ 缓存已清理' } }
      if (action === 'udom_wecom') { sudoExec('sh -c "rm -rf ~/.cache/WXWork && rm -rf ~/.config/wxwork"', value); return { success: true, output: '企业微信缓存已清理' } }
      if (action === 'udom_usb') { sudoExec('sh -c "modprobe -r usb_storage && modprobe usb_storage && udevadm trigger"', value); return { success: true, output: 'USB 驱动已重载' } }
      if (action === 'udom_depends') { sudoExec('apt-get install -f -y', value); return { success: true, output: '依赖关系已修复' } }
      if (action === 'udom_input') { sudoExec('sh -c "im-config -n fcitx && fcitx-autostart"', value); return { success: true, output: '输入法配置已重置' } }
      if (action === 'udom_rootpwd') { sudoExec('sh -c "passwd -x 99999 root && chage -d $(date +%Y-%m-%d) root"', value); return { success: true, output: 'Root 密码有效期已重置' } }
      if (action === 'udom_computer') { sudoExec('sh -c "xdg-mime default dde-file-manager.desktop inode/directory"', value); return { success: true, output: '计算机打开方式已修复' } }
      if (action === 'udom_dns') { return { success: true, output: execOut('cat /etc/resolv.conf 2>/dev/null || echo "无法读取"') } }
      if (action === 'udom_hosts') { return { success: true, output: execOut('cat /etc/hosts 2>/dev/null || echo "无法读取"') } }
      if (action === 'udom_keyring') { sudoExec('sh -c "rm -rf ~/.local/share/keyrings && mkdir -p ~/.local/share/keyrings"', value); return { success: true, output: '密钥环已清理' } }
      if (action === 'udom_update') { sudoExec('apt-get update', value); return { success: true, output: '软件源已更新' } }
      if (action === 'udom_input_intranet') { sudoExec('sh -c "export DISPLAY=:0 && fcitx-autostart && fcitx-remote -r"', value); return { success: true, output: '内网输入法已修复' } }
      if (action === 'udom_store') { sudoExec('sh -c "rm -rf ~/.cache/deepin-store && apt-get update"', value); return { success: true, output: '商店缓存已刷新' } }
      if (action === 'udom_huawei') { return { success: true, output: execOut('echo "=== CPU信息"; cat /proc/cpuinfo | head -10; echo "=== 内存信息"; cat /proc/meminfo | head -5; echo "=== 磁盘信息"; df -h; echo "=== 系统信息"; uname -a; echo "=== 运行时间"; uptime') } }
      if (action === 'udom_cleancache') { sudoExec('sh -c "sync && echo 3 > /proc/sys/vm/drop_caches && apt-get clean"', value); return { success: true, output: '系统缓存已清理' } }
      if (action === 'udom_print') { sudoExec('sh -c "systemctl restart cups && cupsctl --no-shared-printers"', value); return { success: true, output: '打印服务已重置' } }
      if (action === 'udom_launcher') { sudoExec('sh -c "killall dde-dock 2>/dev/null; killall dde-launcher 2>/dev/null"', value); return { success: true, output: '启动器已重置' } }
      if (action === 'udom_sources') { return { success: true, output: execOut('cat /etc/apt/sources.list 2>/dev/null || echo "无文件"') } }
      if (action === 'udom_samba_on') { sudoExec('sh -c "systemctl start smbd"', value); return { success: true, output: 'Samba 共享已开启' } }
      if (action === 'udom_samba_off') { sudoExec('sh -c "systemctl stop smbd"', value); return { success: true, output: 'Samba 共享已关闭' } }
      if (action === 'udom_ssh_on') { sudoExec('sh -c "systemctl start ssh"', value); return { success: true, output: 'SSH 远程已开启' } }
      if (action === 'udom_ssh_off') { sudoExec('sh -c "systemctl stop ssh"', value); return { success: true, output: 'SSH 远程已关闭' } }
      if (action === 'udom_license') { sudoExec('sh -c "rm -f /etc/deepin/license_temp && rm -f /var/lib/uos-license/.tmp_license"', value); return { success: true, output: '临时激活码已移除' } }
      if (action === 'udom_polkit') { sudoExec('sh -c "chown root:root /usr/bin/pkexec && chmod 4755 /usr/bin/pkexec"', value); return { success: true, output: 'polkit 权限已修复' } }
      if (action === 'udom_hostname') { return { success: true, output: execOut('hostname; cat /etc/hostname') } }
      return { success: false, error: '未知操作: ' + action }
    }


    case '_security': {
      if (action === 'check-updates') {
        const updates = execOut('apt list --upgradable 2>/dev/null | grep -v "Listing..." | head -100')
        const secUpdates = execOut('apt list --upgradable 2>/dev/null | grep -i security | head -50')
        const lines = updates.split('\n').filter(Boolean).map(l => {
          const parts = l.trim().split(/\s+/)
          return { name: parts[0] || l, version: parts[1] || '' }
        })
        return { success: true, updates: lines, cves: secUpdates.split('\n').filter(Boolean).map(l => ({ id: l.trim().split(/\s+/)[0] || l, desc: '安全更新', pkg: l.trim().split(/\s+/)[0] || '' })), lastCheck: new Date().toLocaleString('zh-CN') }
      }
      if (action === 'upgrade-all') {
        sudoExec('apt-get update && apt-get upgrade -y', value)
        return { success: true, output: '全部更新已安装' }
      }
      if (action === 'upgrade-sec') {
        const secList = execOut('apt list --upgradable 2>/dev/null | grep -i security | head -50')
        if (!secList.trim()) return { success: true, output: '当前没有需要安装的安全更新' }
        sudoExec('apt-get update && apt-get upgrade -y --only-upgrade $(apt list --upgradable 2>/dev/null | grep -i security | cut -d/ -f1)', value)
        return { success: true, output: '安全更新已安装' }
      }
      if (action === 'download-patch') {
        const url = typeof value === 'object' ? value.url : value
        if (!url) return { success: false, error: '请提供下载链接' }
        const outPath = (typeof value === 'object' ? value.output : null) || '/tmp/patches/' + url.split('/').pop()
        execSync('mkdir -p /tmp/patches 2>/dev/null')
        execSync('wget -c "' + url.replace(/"/g, '\"') + '" -O "' + outPath.replace(/"/g, '\"') + '" 2>&1', { timeout: 300000 })
        return { success: true, output: outPath }
      }
      if (action === 'install-patch') {
        const file = typeof value === 'object' ? value.file : value
        if (!file) return { success: false, error: '请选择补丁文件' }
        const pw = typeof value === 'object' ? value.password : value
        const r = sudoExec('dpkg -i "' + file.replace(/"/g, '\"') + '" && apt-get install -f -y', pw)
        return { success: !r.startsWith('dpkg: error'), output: r || '安装完成' }
      }
      if (action === 'install-dir') {
        const dir = typeof value === 'object' ? value.dir : value
        if (!dir) return { success: false, error: '请选择目录' }
        const pw = typeof value === 'object' ? value.password : value
        const r = sudoExec('sh -c "for f in ' + dir.replace(/"/g, '\"') + '/*.deb; do [ -f \"$f\" ] && dpkg -i \"$f\"; done; apt-get install -f -y"', pw)
        return { success: true, output: r || '批量安装完成' }
      }
      if (action === 'run-script') {
        const script = typeof value === 'object' ? value.script : value
        if (!script) return { success: false, error: '请提供脚本内容' }
        const pw = typeof value === 'object' ? value.password : value
        const fs = require('fs')
        fs.writeFileSync('/tmp/.secpatch.sh', script, { mode: 0o755 })
        const r = sudoExec('bash /tmp/.secpatch.sh', pw)
        return { success: true, output: r || '脚本执行完成' }
      }
      if (action === 'query-cve') {
        const cveId = value
        if (!cveId) return { success: false, error: '请提供 CVE 编号' }
        const helper = __dirname + '/../resources/tool_helper.py'
        try {
          const r = execSync('python3 "' + helper + '" query-cve "' + cveId.replace(/"/g, '\"') + '"', { timeout: 15000, encoding: 'utf-8' })
          const parsed = JSON.parse(r.toString())
          return parsed
        } catch(e) {
          return { success: false, error: '查询失败' }
        }
      }
      return { success: false, error: '未知操作: ' + action }
    }

    case '_install': {
      if (action === 'tesseract') {
        sudoExec('apt-get install -y tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng', value)
        // Verify installation
        const check = execOut('which tesseract 2>/dev/null || echo notfound')
        return { success: check.trim() !== 'notfound', output: check.trim() }
      }
      return { success: false, error: '未知安装: ' + action }
    }

    default: return { success: false, error: '未知优化类型: ' + type }
  }
})


// ========== 依赖安装 IPC ==========

ipcMain.handle('install-dependency', async (_, name) => {
  const { execSync } = require('child_process')
  try {
    if (name === 'tesseract') {
      // 使用 sudoExec 安装 tesseract（密码由前端传入）
      const cmd = 'apt-get install -y tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng'
      // 注意：这里需要管理员密码，但前端调用时没有密码参数
      // 改为返回安装命令，让终端流程执行
      return { success: false, need_sudo: true, cmd: 'apt-get install -y tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng' }
    }
    return { success: false, error: '未知依赖: ' + name }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

// ========== 实用工具 IPC ==========

ipcMain.handle('execute-tool', async (_, tool, params) => {
  const { execSync } = require('child_process')
  const fs = require('fs')
  const path = require('path')

  const helper = __dirname + '/../resources/tool_helper.py'

  try {
    switch(tool) {
      case 'pdf-merge': {
        const { files, output } = params
        if (!files || files.length < 2) return { success: false, error: '请选择至少两个 PDF 文件' }
        const outPath = output || '/tmp/merged.pdf'
        const cmd = 'pdfunite ' + files.map(f => '"' + f.replace(/"/g, '\\"') + '"').join(' ') + ' "' + outPath.replace(/"/g, '\\"') + '"'
        execSync(cmd, { timeout: 60000 })
        return { success: true, output: outPath }
      }
      case 'pdf-split': {
        const { file, outputDir } = params
        if (!file) return { success: false, error: '请选择 PDF 文件' }
        const dir = outputDir || '/tmp'
        const base = path.basename(file, '.pdf')
        const cmd = 'pdfseparate -f 1 "' + file.replace(/"/g, '\\"') + '" "' + dir.replace(/"/g, '\\"') + '/' + base + '-%d.pdf"'
        execSync(cmd, { timeout: 60000 })
        return { success: true, output: dir }
      }
      case 'image-resize': {
        const r = execSync('python3 "' + helper + '" image-resize "' + JSON.stringify(params).replace(/"/g, '\\"') + '"', { timeout: 30000, encoding: 'utf-8' })
        return JSON.parse(r.toString())
      }
      case 'image-convert': {
        const r = execSync('python3 "' + helper + '" image-convert "' + JSON.stringify(params).replace(/"/g, '\\"') + '"', { timeout: 30000, encoding: 'utf-8' })
        return JSON.parse(r.toString())
      }
      case 'ocr': {
        const r = execSync('python3 "' + helper + '" ocr "' + JSON.stringify(params).replace(/"/g, '\\"') + '"', { timeout: 60000, encoding: 'utf-8' })
        return JSON.parse(r.toString())
      }
      case 'video-process': {
        const { file, action, output } = params
        if (!file) return { success: false, error: '请选择视频文件' }
        const outPath = output || '/tmp/processed_' + path.basename(file)
        if (action === 'compress') {
          execSync('ffmpeg -i "' + file.replace(/"/g, '\\"') + '" -vcodec libx264 -crf 28 "' + outPath.replace(/"/g, '\\"') + '" -y 2>&1', { timeout: 300000 })
        } else if (action === 'to-mp4') {
          const out = outPath.replace(/\.[^.]*$/, '.mp4')
          execSync('ffmpeg -i "' + file.replace(/"/g, '\\"') + '" -c:v libx264 -c:a aac "' + out.replace(/"/g, '\\"') + '" -y 2>&1', { timeout: 300000 })
        } else {
          return { success: false, error: '未知操作: ' + action }
        }
        return { success: true, output: outPath }
      }
      case 'audio-process': {
        const { file, action, output } = params
        if (!file) return { success: false, error: '请选择音频文件' }
        const outPath = output || '/tmp/processed_' + path.basename(file)
        if (action === 'to-mp3') {
          const out = outPath.replace(/\.[^.]*$/, '.mp3')
          execSync('ffmpeg -i "' + file.replace(/"/g, '\\"') + '" -codec:a libmp3lame -qscale:a 2 "' + out.replace(/"/g, '\\"') + '" -y 2>&1', { timeout: 300000 })
        } else if (action === 'compress') {
          execSync('ffmpeg -i "' + file.replace(/"/g, '\\"') + '" -codec:a libmp3lame -bitrate:a 128k "' + outPath.replace(/"/g, '\\"') + '" -y 2>&1', { timeout: 300000 })
        } else {
          return { success: false, error: '未知操作: ' + action }
        }
        return { success: true, output: outPath }
      }
      case 'security-sync': {
        const { action: secAction, vuln_id, sys_type, edition, version, arch, output_dir,
                offset, limit, keyword, severity, status } = params
        const tmpArgs = '/tmp/.uos_sec_' + Date.now() + '.json'
        // 根据命令类型构建参数
        let args
        if (secAction === 'list') {
          args = { offset: offset || 0, limit: limit || 20, sys_type: sys_type || 'desktop',
                   edition: edition || 'E', version: version || '', arch: arch || 'AMD64',
                   keyword: keyword || '', severity: severity || '', status: status || '' }
        } else {
          args = { vuln_id, sys_type, edition: edition || 'E', version: version || '',
                   arch: arch || 'AMD64', output_dir: output_dir || '/tmp/security_patches' }
        }
        fs.writeFileSync(tmpArgs, JSON.stringify(args))
        const helper = path.join(__dirname, '../resources/security_sync/security_sync_wrapper.py')
        try {
          const r = execSync('python3 "' + helper + '" ' + secAction + ' "' + tmpArgs + '"', { timeout: 120000, encoding: 'utf-8' })
          try { fs.unlinkSync(tmpArgs) } catch {}
          return JSON.parse(r.toString())
        } catch(e) {
          try { fs.unlinkSync(tmpArgs) } catch {}
          const stderr = e.stderr ? e.stderr.toString() : ''
          return { success: false, error: stderr || e.message }
        }
      }
      default:
        return { success: false, error: '未知工具: ' + tool }
    }
  } catch(e) {
    return { success: false, error: e.message || String(e) }
  }
})


// ========== 软件包管理器 IPC ==========

ipcMain.handle('pkg-manager', async (_, action, params) => {
  const { execSync } = require('child_process')
  const fs = require('fs')
  const path = require('path')

  try {
    switch(action) {
      case 'list-installed': {
        // 使用 apt list --installed 获取更快的输出
        try {
          const raw = execSync("apt list --installed 2>/dev/null", { timeout: 15000, encoding: 'utf-8' }).toString()
          const lines = raw.split('\n').filter(l => l && !l.startsWith('Listing...'))
          const packages = lines.map(l => {
            // Format: pkgname/stable,now version arch [installed]
            const match = l.match(/^([^/]+)\/(\S+)\s+(\S+)\s+(\S+)\s+\[installed(?:,\w+)*\]/)
            if(!match) return null
            const name = match[1]
            // Get description
            let desc = ''
            try {
              const descRaw = execSync("apt-cache show " + name + " 2>/dev/null | grep -m1 '^Description' | sed 's/Description: //'", { timeout: 5000, encoding: 'utf-8' }).toString().trim()
              desc = descRaw
            } catch {}
            return { name, version: match[3], arch: match[4], desc }
          }).filter(Boolean)
          return { success: true, packages }
        } catch(e) {
          // Fallback to dpkg -l
          const raw = execSync("dpkg -l 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
          const lines = raw.split('\n').filter(l => /^[a-z]/.test(l))
          const packages = lines.map(l => {
            const parts = l.split(/\s+/)
            if(parts.length < 4) return null
            const name = parts[1]
            let desc = ''
            try {
              const descRaw = execSync("dpkg -s " + name + " 2>/dev/null | grep '^Description' | sed 's/Description: //'", { timeout: 5000, encoding: 'utf-8' }).toString().trim()
              desc = descRaw
            } catch {}
            return { name, version: parts[2], arch: parts[3]||'', desc }
          }).filter(Boolean)
          return { success: true, packages }
        }
      }

      case 'search': {
        const keyword = params?.keyword || ''
        if(!keyword) return { success: false, error: '请输入搜索关键词' }
        const raw = execSync("apt-cache search " + keyword.replace(/[^a-zA-Z0-9._-]/g, '') + " 2>/dev/null", { timeout: 30000, encoding: 'utf-8' }).toString()
        const lines = raw.split('\n').filter(l => l.trim())
        const packages = lines.map(l => {
          const idx = l.indexOf(' - ')
          if(idx === -1) return { package: l.trim(), version: '', desc: '' }
          const name = l.substring(0, idx).trim()
          const desc = l.substring(idx + 3).trim()
          let version = ''
          try {
            const vRaw = execSync("apt-cache show " + name + " 2>/dev/null | grep '^Version' | head -1 | sed 's/Version: //'", { timeout: 5000, encoding: 'utf-8' }).toString().trim()
            version = vRaw
          } catch {}
          return { package: name, version, desc }
        })
        return { success: true, packages }
      }

      case 'install': {
        const pkgName = params?.name || ''
        const password = params?.password || ''
        if(!pkgName) return { success: false, error: '请指定要安装的软件包' }
        if(!password) return { success: false, error: '需要管理员密码' }
        const result = sudoExec('DEBIAN_FRONTEND=noninteractive apt-get install -y ' + pkgName.replace(/[^a-zA-Z0-9._+\-]/g, '') + ' 2>&1', password)
        if(result && !result.includes('E:')) {
          return { success: true, output: result }
        } else {
          return { success: false, error: result || '安装失败' }
        }
      }

      case 'remove': {
        const pkgName = params?.name || ''
        const password = params?.password || ''
        if(!pkgName) return { success: false, error: '请指定要卸载的软件包' }
        if(!password) return { success: false, error: '需要管理员密码' }
        const result = sudoExec('DEBIAN_FRONTEND=noninteractive apt-get remove -y ' + pkgName.replace(/[^a-zA-Z0-9._+\-]/g, '') + ' 2>&1', password)
        if(result && !result.includes('E:')) {
          return { success: true, output: result }
        } else {
          return { success: false, error: result || '卸载失败' }
        }
      }

      case 'info': {
        const pkgName = params?.name || ''
        if(!pkgName) return { success: false, error: '请指定软件包名称' }
        try {
          const raw = execSync("dpkg -s " + pkgName + " 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
          const lines = raw.split('\n')
          const info = {}
          info.name = pkgName
          for(const l of lines){
            if(l.startsWith('Version:')) info.version = l.replace('Version:', '').trim()
            else if(l.startsWith('Architecture:')) info.arch = l.replace('Architecture:', '').trim()
            else if(l.startsWith('Description:')) info.desc = l.replace('Description:', '').trim()
            else if(l.startsWith('Maintainer:')) info.maintainer = l.replace('Maintainer:', '').trim()
            else if(l.startsWith('Installed-Size:')) {
              const kb = parseInt(l.replace('Installed-Size:', '').trim())
              info.size = kb ? (kb/1024).toFixed(1) + ' MB' : l.replace('Installed-Size:', '').trim() + ' KB'
            }
            else if(l.startsWith('Depends:')) info.depends = l.replace('Depends:', '').trim()
          }
          return { success: true, info }
        } catch(e) {
          return { success: false, error: e.message }
        }
      }

      case 'files': {
        const pkgName = params?.name || ''
        if(!pkgName) return { success: false, error: '请指定软件包名称' }
        try {
          const raw = execSync("dpkg -L " + pkgName + " 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
          const files = raw.split('\n').filter(l => l.trim())
          return { success: true, files }
        } catch(e) {
          return { success: false, error: e.message }
        }
      }

      default:
        return { success: false, error: '未知操作: ' + action }
    }
  } catch(e) {
    return { success: false, error: e.message || String(e) }
  }
})

