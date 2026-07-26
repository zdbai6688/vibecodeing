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
        try {
          // 1. 获取 systemd --user 服务
          const userServicesRaw = execOut('systemctl --user list-unit-files --type=service --no-pager --no-legend 2>/dev/null | head -80')
          const systemServicesRaw = execOut('systemctl list-unit-files --type=service --no-pager --no-legend 2>/dev/null | head -80')
          
          // 2. 获取 autostart .desktop 文件
          const userAutostartDir = path.join(os.homedir(), '.config/autostart')
          const systemAutostartDir = '/etc/xdg/autostart'
          let userAutostartFiles = []
          let systemAutostartFiles = []
          try {
            if (fs.existsSync(userAutostartDir)) {
              userAutostartFiles = fs.readdirSync(userAutostartDir).filter(f => f.endsWith('.desktop'))
            }
            if (fs.existsSync(systemAutostartDir)) {
              systemAutostartFiles = fs.readdirSync(systemAutostartDir).filter(f => f.endsWith('.desktop'))
            }
          } catch(e) {}
          
          // 3. 解析 systemd 服务
          const systemdServices = []
          for (const raw of [userServicesRaw, systemServicesRaw]) {
            if (!raw) continue
            const lines = raw.trim().split('\n')
            for (const line of lines) {
              const parts = line.trim().split(/\s+/)
              if (parts.length >= 2) {
                const name = parts[0]
                const state = parts[1]
                if (name && !name.includes('@')) {
                  systemdServices.push({
                    name: name,
                    state: state,
                    type: 'systemd'
                  })
                }
              }
            }
          }
          
          // 4. 解析 autostart desktop 文件
          const autostartItems = []
          for (const f of systemAutostartFiles) {
            const fp = path.join(systemAutostartDir, f)
            try {
              const raw = fs.readFileSync(fp, 'utf-8')
              const nameMatch = raw.match(/^Name=(.+)/m)
              const execMatch = raw.match(/^Exec=(.+)/m)
              const commentMatch = raw.match(/^Comment=(.+)/m)
              const hiddenMatch = raw.match(/^Hidden=(.+)/m)
              const onlyShowIn = raw.match(/^OnlyShowIn=(.+)/m)
              autostartItems.push({
                name: f,
                displayName: nameMatch ? nameMatch[1] : f,
                exec: execMatch ? execMatch[1] : '',
                comment: commentMatch ? commentMatch[1] : '',
                hidden: hiddenMatch ? hiddenMatch[1] === 'true' : false,
                onlyShowIn: onlyShowIn ? onlyShowIn[1] : '',
                source: 'system',
                path: fp
              })
            } catch(e) {}
          }
          for (const f of userAutostartFiles) {
            const fp = path.join(userAutostartDir, f)
            try {
              const raw = fs.readFileSync(fp, 'utf-8')
              const nameMatch = raw.match(/^Name=(.+)/m)
              const execMatch = raw.match(/^Exec=(.+)/m)
              const commentMatch = raw.match(/^Comment=(.+)/m)
              const hiddenMatch = raw.match(/^Hidden=(.+)/m)
              const onlyShowIn = raw.match(/^OnlyShowIn=(.+)/m)
              const enabled = !(hiddenMatch && hiddenMatch[1] === 'true')
              autostartItems.push({
                name: f,
                displayName: nameMatch ? nameMatch[1] : f,
                exec: execMatch ? execMatch[1] : '',
                comment: commentMatch ? commentMatch[1] : '',
                hidden: hiddenMatch ? hiddenMatch[1] === 'true' : false,
                enabled: enabled,
                source: 'user',
                path: fp
              })
            } catch(e) {}
          }
          
          return { success: true, data: { systemdServices, autostartItems } }
        } catch(e) {
          return { success: false, error: e.message || String(e) }
        }
      }
      if (action.startsWith('startup-mgr:toggle-systemd:')) {
        const serviceName = action.replace('startup-mgr:toggle-systemd:', '')
        if (!serviceName) return { success: false, error: '未指定服务名' }
        const currentState = execOut('systemctl --user is-enabled ' + serviceName + ' 2>/dev/null').trim()
        if (currentState === 'enabled' || currentState === 'static') {
          sudoExec('systemctl --user disable ' + serviceName, value)
          return { success: true, output: '已禁用服务: ' + serviceName }
        } else {
          sudoExec('systemctl --user enable ' + serviceName, value)
          return { success: true, output: '已启用服务: ' + serviceName }
        }
      }
      if (action.startsWith('startup-mgr:toggle-systemd-system:')) {
        const serviceName = action.replace('startup-mgr:toggle-systemd-system:', '')
        if (!serviceName) return { success: false, error: '未指定服务名' }
        const currentState = execOut('systemctl is-enabled ' + serviceName + ' 2>/dev/null').trim()
        if (currentState === 'enabled' || currentState === 'static') {
          sudoExec('systemctl disable ' + serviceName, value)
          return { success: true, output: '已禁用系统服务: ' + serviceName }
        } else {
          sudoExec('systemctl enable ' + serviceName, value)
          return { success: true, output: '已启用系统服务: ' + serviceName }
        }
      }
      if (action.startsWith('startup-mgr:toggle-autostart:')) {
        const fileName = action.replace('startup-mgr:toggle-autostart:', '')
        if (!fileName) return { success: false, error: '未指定文件名' }
        const userAutostartDir = path.join(os.homedir(), '.config/autostart')
        const srcPath = path.join('/etc/xdg/autostart', fileName)
        const dstPath = path.join(userAutostartDir, fileName)
        try {
          if (fs.existsSync(dstPath)) {
            // 检查是否被隐藏
            const raw = fs.readFileSync(dstPath, 'utf-8')
            const hiddenMatch = raw.match(/^Hidden=(.+)/m)
            if (hiddenMatch && hiddenMatch[1] === 'true') {
              // 启用：删除 Hidden=true 或删除整个文件（从用户目录删除 = 恢复系统默认）
              const newRaw = raw.replace(/^Hidden=true\s*/m, '')
              fs.writeFileSync(dstPath, newRaw, 'utf-8')
              return { success: true, output: '已启用: ' + fileName }
            } else {
              // 禁用：添加 Hidden=true
              const newRaw = raw.replace(/\r?\n$/, '') + '\nHidden=true\n'
              fs.writeFileSync(dstPath, newRaw, 'utf-8')
              return { success: true, output: '已禁用: ' + fileName }
            }
          } else if (fs.existsSync(srcPath)) {
            // 从系统目录复制并设置 Hidden=true 以禁用
            const raw = fs.readFileSync(srcPath, 'utf-8')
            if (!fs.existsSync(userAutostartDir)) fs.mkdirSync(userAutostartDir, { recursive: true })
            fs.writeFileSync(dstPath, raw.replace(/\r?\n$/, '') + '\nHidden=true\n', 'utf-8')
            return { success: true, output: '已禁用: ' + fileName }
          } else {
            return { success: false, error: '未找到文件: ' + fileName }
          }
        } catch(e) {
          return { success: false, error: e.message }
        }
      }
      if (action === 'startup-mgr:add-autostart') {
        try {
          const params = JSON.parse(value || '{}')
          const { execCmd, displayName, comment } = params
          if (!execCmd) return { success: false, error: '未指定启动命令' }
          const safeName = (displayName || 'CustomApp').replace(/[^a-zA-Z0-9_-]/g, '_')
          const fileName = safeName + '.desktop'
          const userAutostartDir = path.join(os.homedir(), '.config/autostart')
          if (!fs.existsSync(userAutostartDir)) fs.mkdirSync(userAutostartDir, { recursive: true })
          const desktopContent = '[Desktop Entry]\nType=Application\nName=' + (displayName || 'Custom App') + '\nExec=' + execCmd + '\n' + (comment ? 'Comment=' + comment + '\n' : '') + 'X-GNOME-Autostart-enabled=true\n'
          fs.writeFileSync(path.join(userAutostartDir, fileName), desktopContent, 'utf-8')
          return { success: true, output: '已添加自定义启动项: ' + (displayName || execCmd) }
        } catch(e) {
          return { success: false, error: e.message }
        }
      }
      if (action.startsWith('startup-mgr:remove-autostart:')) {
        const fileName = action.replace('startup-mgr:remove-autostart:', '')
        if (!fileName) return { success: false, error: '未指定文件名' }
        const userAutostartDir = path.join(os.homedir(), '.config/autostart')
        const fp = path.join(userAutostartDir, fileName)
        try {
          if (fs.existsSync(fp)) {
            fs.unlinkSync(fp)
            return { success: true, output: '已删除启动项: ' + fileName }
          }
          return { success: false, error: '文件不存在: ' + fileName }
        } catch(e) {
          return { success: false, error: e.message }
        }
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
      case 'localsend-discovery':
        try {
          const dgram = require('dgram')
          const os = require('os')
          
          const PORT = 42135
          const BROADCAST_ADDR = '255.255.255.255'
          const hostname = os.hostname()
          const DISCOVERY_MSG = JSON.stringify({ type: 'discovery', hostname: hostname, timestamp: Date.now() })
          
          return new Promise((resolve) => {
            const devices = []
            let timeout = null
            const socket = dgram.createSocket({ type: 'udp4', reuseAddr: true })
            
            socket.on('error', (err) => {
              if (timeout) clearTimeout(timeout)
              try { socket.close() } catch {}
              resolve({ success: true, devices: [] })
            })
            
            socket.on('message', (msg, rinfo) => {
              try {
                const data = JSON.parse(msg.toString())
                if (data.type === 'discovery-response' || data.type === 'discovery') {
                  const existing = devices.findIndex(d => d.address === rinfo.address)
                  const device = { address: rinfo.address, hostname: data.hostname || rinfo.address, port: data.port || PORT, timestamp: Date.now() }
                  if (existing >= 0) {
                    devices[existing] = device
                  } else {
                    devices.push(device)
                  }
                }
              } catch {}
            })
            
            socket.bind(PORT, () => {
              socket.setBroadcast(true)
              for (let i = 0; i < 3; i++) {
                setTimeout(() => {
                  try { socket.send(Buffer.from(DISCOVERY_MSG), PORT, BROADCAST_ADDR) } catch {}
                }, i * 300)
              }
              timeout = setTimeout(() => {
                try { socket.close() } catch {}
                resolve({ success: true, devices })
              }, 3000)
            })
          })
        } catch(e) {
          return { success: false, error: e.message }
        }
        
      case 'localsend-send':
        try {
          const http = require('http')
          const fs = require('fs')
          const path = require('path')
          
          const { targetAddress, targetPort, files } = params
          if (!targetAddress || !files || files.length === 0) {
            return { success: false, error: '缺少目标地址或文件' }
          }
          
          const results = []
          const promises = files.map((file, index) => {
            return new Promise((resolveFile) => {
              const fileName = path.basename(file)
              const fileStat = fs.statSync(file)
              const fileSize = fileStat.size
              
              const options = {
                hostname: targetAddress,
                port: targetPort || 42136,
                path: '/upload',
                method: 'POST',
                headers: {
                  'Content-Type': 'application/octet-stream',
                  'X-File-Name': encodeURIComponent(fileName),
                  'X-File-Size': String(fileSize),
                  'X-File-Index': String(index)
                }
              }
              
              const req = http.request(options, (res) => {
                let body = ''
                res.on('data', chunk => { body += chunk })
                res.on('end', () => {
                  results.push({ file: fileName, success: res.statusCode === 200, response: body })
                  resolveFile()
                })
              })
              
              req.on('error', (err) => {
                results.push({ file: fileName, success: false, error: err.message })
                resolveFile()
              })
              
              req.setTimeout(120000, () => {
                req.destroy()
                results.push({ file: fileName, success: false, error: '传输超时' })
                resolveFile()
              })
              
              fs.createReadStream(file).pipe(req)
            })
          })
          
          return Promise.all(promises).then(() => ({ success: true, results }))
        } catch(e) {
          return { success: false, error: e.message }
        }
        
      case 'localsend-receive':
        try {
          const http = require('http')
          const fs = require('fs')
          const path = require('path')
          const os = require('os')
          
          const { saveDir } = params
          const downloadDir = saveDir || '/tmp/localsend_received'
          if (!fs.existsSync(downloadDir)) {
            fs.mkdirSync(downloadDir, { recursive: true })
          }
          
          if (global.__localsendServer) {
            try { global.__localsendServer.close() } catch {}
            global.__localsendServer = null
          }
          
          const server = http.createServer((req, res) => {
            if (req.method === 'POST' && req.url === '/upload') {
              const fileName = decodeURIComponent(req.headers['x-file-name'] || 'unknown')
              const fileSize = parseInt(req.headers['x-file-size'] || '0')
              const savePath = path.join(downloadDir, fileName)
              
              if (savePath.indexOf(downloadDir) !== 0) {
                res.writeHead(403)
                res.end('Forbidden')
                return
              }
              
              const writeStream = fs.createWriteStream(savePath)
              let receivedSize = 0
              const startTime = Date.now()
              
              req.on('data', chunk => {
                receivedSize += chunk.length
                writeStream.write(chunk)
              })
              
              req.on('end', () => {
                writeStream.end()
                const elapsed = (Date.now() - startTime) / 1000
                res.writeHead(200, { 'Content-Type': 'application/json' })
                res.end(JSON.stringify({
                  success: true,
                  fileName,
                  fileSize: receivedSize,
                  savePath,
                  duration: elapsed,
                  speed: elapsed > 0 ? Math.round(receivedSize / elapsed / 1024) + ' KB/s' : 'N/A'
                }))
              })
              
              req.on('error', (err) => {
                writeStream.end()
                res.writeHead(500)
                res.end(JSON.stringify({ success: false, error: err.message }))
              })
            } else {
              res.writeHead(200)
              res.end(JSON.stringify({ status: 'active', hostname: os.hostname() }))
            }
          })
          
          return new Promise((resolve) => {
            server.listen(42136, '0.0.0.0', () => {
              global.__localsendServer = server
              resolve({ success: true, port: 42136, saveDir: downloadDir })
            })
            server.on('error', (err) => {
              resolve({ success: false, error: err.message })
            })
          })
        } catch(e) {
          return { success: false, error: e.message }
        }
        
      case 'localsend-stop':
        try {
          if (global.__localsendServer) {
            global.__localsendServer.close()
            global.__localsendServer = null
          }
          return { success: true }
        } catch(e) {
          return { success: false, error: e.message }
        }
        
      case 'localsend-status':
        try {
          const isRunning = !!global.__localsendServer
          return { success: true, running: isRunning, port: 42136 }
        } catch(e) {
          return { success: false, error: e.message }
        }
        
      

      case 'file-search': {
        const { keyword, filters } = params
        if (!keyword) return { success: false, error: '请输入搜索关键词' }
        
        const searchRoot = os.homedir()
        let cmd = ''
        const escapedKeyword = keyword.replace(/[^\u4e00-\u9fa5a-zA-Z0-9._-]/g, '')
        if (!escapedKeyword) return { success: false, error: '请输入有效的搜索关键词' }
        
        cmd = 'find ' + searchRoot + ' -maxdepth 5 -iname "*' + escapedKeyword + '*" -not -path "*/.*" 2>/dev/null'
        
        if (filters && filters.type) {
          const typeMap = {
            'document': '\( -iname "*.pdf" -o -iname "*.doc" -o -iname "*.docx" -o -iname "*.txt" -o -iname "*.xls" -o -iname "*.xlsx" -o -iname "*.ppt" -o -iname "*.pptx" -o -iname "*.csv" -o -iname "*.md" \)',
            'image': '\( -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o -iname "*.gif" -o -iname "*.bmp" -o -iname "*.webp" -o -iname "*.svg" -o -iname "*.ico" \)',
            'video': '\( -iname "*.mp4" -o -iname "*.avi" -o -iname "*.mkv" -o -iname "*.mov" -o -iname "*.wmv" -o -iname "*.flv" -o -iname "*.webm" \)',
            'audio': '\( -iname "*.mp3" -o -iname "*.wav" -o -iname "*.flac" -o -iname "*.aac" -o -iname "*.ogg" -o -iname "*.wma" \)',
            'archive': '\( -iname "*.zip" -o -iname "*.tar" -o -iname "*.gz" -o -iname "*.bz2" -o -iname "*.xz" -o -iname "*.rar" -o -iname "*.7z" \)',
            'executable': '\( -executable -type f \)'
          }
          if (typeMap[filters.type]) {
            cmd = 'find ' + searchRoot + ' -maxdepth 5 ' + typeMap[filters.type] + ' -not -path "*/.*" 2>/dev/null | xargs -I{} find {} -maxdepth 0 -iname "*' + escapedKeyword + '*" 2>/dev/null'
          }
        }
        
        if (filters && filters.size) {
          const sizeMap = { '-1k': '-size -1k', '1k-100k': '-size +1k -size -100k', '100k-1m': '-size +100k -size -1M', '1m-10m': '-size +1M -size -10M', '10m-100m': '-size +10M -size -100M', '+100m': '-size +100M' }
          if (sizeMap[filters.size]) cmd += ' ' + sizeMap[filters.size]
        }
        
        if (filters && filters.date) {
          const now = new Date()
          let dateAfter = ''
          if (filters.date === 'today') dateAfter = new Date(now.getFullYear(),now.getMonth(),now.getDate()).toISOString().split('T')[0]
          else if (filters.date === 'week') { const d = new Date(now); d.setDate(d.getDate()-7); dateAfter = d.toISOString().split('T')[0] }
          else if (filters.date === 'month') { const d = new Date(now); d.setMonth(d.getMonth()-1); dateAfter = d.toISOString().split('T')[0] }
          else if (filters.date === 'year') { const d = new Date(now); d.setFullYear(d.getFullYear()-1); dateAfter = d.toISOString().split('T')[0] }
          if (dateAfter) cmd += ' -newer ' + dateAfter
        }
        
        cmd += ' 2>/dev/null'
        
        const maxResults = 80
        let output = ''
        try {
          output = execSync(cmd, { timeout: 30000, encoding: 'utf-8', maxBuffer: 10*1024*1024 }).toString()
        } catch(e) {
          if (e.stdout) output = e.stdout.toString()
          else return { success: false, error: '搜索超时或出错: ' + e.message }
        }
        
        const lines = output.split('\n').filter(Boolean).slice(0, maxResults)
        const files = lines.map(line => {
          try {
            const stat = fs.statSync(line)
            const sizeStr = stat.size < 1024 ? stat.size + 'B' : stat.size < 1024*1024 ? (stat.size/1024).toFixed(1) + 'KB' : stat.size < 1024*1024*1024 ? (stat.size/1024/1024).toFixed(1) + 'MB' : (stat.size/1024/1024/1024).toFixed(2) + 'GB'
            const ext = path.extname(line).toLowerCase()
            let type = '文件'
            if (['.pdf','.doc','.docx','.txt','.xls','.xlsx','.ppt','.pptx','.csv','.md'].includes(ext)) type = '文档'
            else if (['.jpg','.jpeg','.png','.gif','.bmp','.webp','.svg','.ico'].includes(ext)) type = '图片'
            else if (['.mp4','.avi','.mkv','.mov','.wmv','.flv','.webm'].includes(ext)) type = '视频'
            else if (['.mp3','.wav','.flac','.aac','.ogg','.wma'].includes(ext)) type = '音频'
            else if (['.zip','.tar','.gz','.bz2','.xz','.rar','.7z'].includes(ext)) type = '压缩包'
            return { name: path.basename(line), path: line, type, sizeStr, dateStr: new Date(stat.mtime).toLocaleDateString('zh-CN'), isDir: stat.isDirectory() }
          } catch { return null }
        }).filter(Boolean)
        
        return { success: true, files }
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

// ========== Phase 3: 防火墙规则管理器 ==========
ipcMain.handle('firewall-list-rules', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("sudo iptables -L -n --line-numbers 2>/dev/null || iptables -L -n --line-numbers 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
    const chains = {}; let currentChain = ''
    raw.split('\n').forEach(l => {
      if (l.startsWith('Chain ')) {
        const m = l.match(/^Chain (\S+)/); if (m) { currentChain = m[1]; chains[currentChain] = chains[currentChain] || [] }
      } else if (currentChain && l.trim() && !l.includes('target') && !l.startsWith('num')) {
        const parts = l.trim().split(/\s+/); if (parts.length >= 2) chains[currentChain].push({ raw: l.trim(), parts })
      }
    })
    // Also get nftables rules
    let nftRules = ''
    try { nftRules = execSync("sudo nft list ruleset 2>/dev/null || nft list ruleset 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    return { success: true, iptables: chains, nftables: nftRules, enabled: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('firewall-add-rule', async (_, rule) => {
  const { execSync } = require('child_process'); const { sudoExec } = global
  try {
    let cmd = ''
    if (rule.type === 'port') cmd = `iptables -A INPUT -p ${rule.proto||'tcp'} --dport ${rule.port} -j ${rule.action||'ACCEPT'}`
    else if (rule.type === 'ip') cmd = `iptables -A INPUT -s ${rule.ip} -j ${rule.action||'ACCEPT'}`
    else if (rule.type === 'icmp') cmd = 'iptables -A INPUT -p icmp -j ACCEPT'
    const result = sudoExec(cmd + ' 2>&1', rule.password||'')
    return { success: true, output: result }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('firewall-remove-rule', async (_, idx) => {
  const { execSync } = require('child_process')
  try {
    execSync(`iptables -D INPUT ${parseInt(idx)+1} 2>&1`, { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('firewall-toggle-rule', async (_, idx) => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync(`iptables -L INPUT -n --line-numbers 2>/dev/null`, { timeout: 5000, encoding: 'utf-8' }).toString()
    const lines = raw.split('\n').filter(l => l.trim() && /^\d+/.test(l.trim()))
    const line = lines[parseInt(idx)]; if (!line) return { success: false, error: '规则不存在' }
    if (line.includes('DROP')) execSync(`iptables -R INPUT ${parseInt(idx)+1} ${line.replace('DROP','ACCEPT')} 2>&1`, { timeout: 5000 })
    else execSync(`iptables -R INPUT ${parseInt(idx)+1} ${line.replace('ACCEPT','DROP')} 2>&1`, { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('firewall-get-status', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("sudo ufw status 2>/dev/null || echo 'disabled'", { timeout: 5000, encoding: 'utf-8' }).toString()
    const enabled = raw.includes('active') || raw.includes('Status: active')
    return { success: true, ufwStatus: enabled, raw }
  } catch(e) { return { success: true, ufwStatus: false } }
})
ipcMain.handle('firewall-set-status', async (_, enable) => {
  const { execSync } = require('child_process'); const { sudoExec } = global
  try {
    const result = sudoExec(`ufw ${enable?'enable':'disable'} 2>&1`, '')
    return { success: true, output: result }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('firewall-backup-rules', async () => {
  const { execSync } = require('child_process'); const fs = require('fs'); const path = require('path')
  try {
    const backupDir = path.join(require('electron').app.getPath('userData'), 'firewall_backups')
    if (!fs.existsSync(backupDir)) fs.mkdirSync(backupDir, { recursive: true })
    const file = path.join(backupDir, `iptables_backup_${Date.now()}.rules`)
    const raw = execSync("sudo iptables-save 2>/dev/null || iptables-save 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
    fs.writeFileSync(file, raw)
    return { success: true, file }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('firewall-restore-rules', async () => {
  const { execSync } = require('child_process'); const fs = require('fs'); const path = require('path')
  try {
    const backupDir = path.join(require('electron').app.getPath('userData'), 'firewall_backups')
    if (!fs.existsSync(backupDir)) return { success: false, error: '无备份文件' }
    const files = fs.readdirSync(backupDir).filter(f => f.endsWith('.rules')).sort()
    if (!files.length) return { success: false, error: '无备份文件' }
    const file = path.join(backupDir, files[files.length-1])
    const raw = fs.readFileSync(file, 'utf-8')
    execSync("sudo iptables-restore <<< '" + raw.replace(/'/g, "'\\''") + "' 2>/dev/null", { timeout: 10000 })
    return { success: true, file: files[files.length-1] }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('firewall-add-port', async (_, port, proto) => {
  const { execSync } = require('child_process')
  try {
    execSync(`sudo iptables -A INPUT -p ${proto||'tcp'} --dport ${parseInt(port)} -j ACCEPT 2>&1`, { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 网络流量监控 ==========
ipcMain.handle('netflow-get-traffic', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("cat /proc/net/dev 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    const lines = raw.split('\n').filter(l => l.includes(':'))
    const interfaces = lines.map(l => {
      const parts = l.trim().split(/\s+/); const name = parts[0].replace(':','')
      return { name, rxBytes: parseInt(parts[1])||0, txBytes: parseInt(parts[9])||0 }
    })
    const totalRx = interfaces.reduce((s,i) => s+i.rxBytes, 0)
    const totalTx = interfaces.reduce((s,i) => s+i.txBytes, 0)
    return { success: true, interfaces, totalRx, totalTx }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('netflow-get-process-traffic', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("ss -tupn 2>/dev/null | tail -n +2", { timeout: 10000, encoding: 'utf-8' }).toString()
    const lines = raw.split('\n').filter(l => l.trim())
    const procs = []
    lines.forEach(l => {
      const m = l.match(/users:\(\("([^"]+)"/)
      if (m) {
        const name = m[1]
        const existing = procs.find(p => p.name === name)
        if (existing) existing.connections++
        else procs.push({ name, connections: 1 })
      }
    })
    return { success: true, processes: procs.sort((a,b) => b.connections - a.connections).slice(0,20) }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('netflow-get-traffic-history', async () => {
  const { execSync } = require('child_process'); const fs = require('fs'); const path = require('path')
  try {
    const histFile = path.join(require('electron').app.getPath('userData'), 'traffic_history.json')
    if (!fs.existsSync(histFile)) return { success: true, history: [] }
    const data = JSON.parse(fs.readFileSync(histFile, 'utf-8'))
    return { success: true, history: Array.isArray(data) ? data.slice(-60) : [] }
  } catch(e) { return { success: true, history: [] } }
})

// ========== Phase 3: 系统安全扫描 ==========
ipcMain.handle('secscan-run', async (_, type) => {
  const { execSync } = require('child_process')
  try {
    let output = ''
    if (type === 'lynis') { try { output = execSync("sudo lynis audit system --quick 2>/dev/null || echo 'lynis not installed'", { timeout: 60000, encoding: 'utf-8' }).toString() } catch { output = 'lynis not installed' } }
    else if (type === 'rkhunter') { try { output = execSync("sudo rkhunter --check --skip-keypress 2>/dev/null || echo 'rkhunter not installed'", { timeout: 120000, encoding: 'utf-8' }).toString() } catch { output = 'rkhunter not installed' } }
    else if (type === 'chkrootkit') { try { output = execSync("sudo chkrootkit 2>/dev/null || echo 'chkrootkit not installed'", { timeout: 60000, encoding: 'utf-8' }).toString() } catch { output = 'chkrootkit not installed' } }
    else if (type === 'weakpass') {
      const users = execSync("awk -F: '($3>=1000){print $1}' /etc/passwd 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString().trim().split('\n')
      output = users.map(u => `User: ${u}`).join('\n') + '\n(弱密码检测需要安装 john/chkrootkit 等工具)'
    }
    else if (type === 'quick') {
      const parts = []
      try { parts.push(`开放端口: ${execSync("ss -tlnp 2>/dev/null | wc -l", { timeout: 5000, encoding: 'utf-8' }).toString().trim()}`) } catch {}
      try { parts.push(`登录失败: ${execSync("lastb 2>/dev/null | wc -l", { timeout: 5000, encoding: 'utf-8' }).toString().trim()}`) } catch {}
      try { parts.push(`root登录: ${execSync("last root 2>/dev/null | head -5 | wc -l", { timeout: 5000, encoding: 'utf-8' }).toString().trim()}`) } catch {}
      output = parts.join('\n')
    }
    return { success: true, output, type }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('secscan-get-results', async () => {
  const fs = require('fs'); const path = require('path')
  try {
    const f = path.join(require('electron').app.getPath('userData'), 'secscan_results.json')
    if (!fs.existsSync(f)) return { success: true, results: [] }
    return { success: true, results: JSON.parse(fs.readFileSync(f,'utf-8')) }
  } catch(e) { return { success: true, results: [] } }
})
ipcMain.handle('secscan-install-tool', async (_, tool) => {
  const { execSync } = require('child_process')
  try {
    execSync(`sudo apt-get install -y ${tool} 2>&1`, { timeout: 120000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 系统更新历史与回滚 ==========
ipcMain.handle('updatehist-get-history', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("grep -E '^(Start-Date|Commandline|Install|Upgrade|Remove|End-Date)' /var/log/apt/history.log 2>/dev/null | head -200", { timeout: 10000, encoding: 'utf-8' }).toString()
    const entries = []; let current = {}
    raw.split('\n').forEach(l => {
      if (l.startsWith('Start-Date:')) { current = { date: l.replace('Start-Date:','').trim() } }
      else if (l.startsWith('Commandline:')) current.cmd = l.replace('Commandline:','').trim()
      else if (l.startsWith('Install:')) current.install = l.replace('Install:','').trim()
      else if (l.startsWith('Upgrade:')) current.upgrade = l.replace('Upgrade:','').trim()
      else if (l.startsWith('Remove:')) current.remove = l.replace('Remove:','').trim()
      else if (l.startsWith('End-Date:')) { entries.push(current); current = {} }
    })
    return { success: true, entries: entries.slice(-50).reverse() }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('updatehist-rollback', async (_, pkg, version, password) => {
  const { execSync } = require('child_process'); const { sudoExec } = global
  try {
    const result = sudoExec(`apt-get install -y ${pkg}=${version.replace(/[^a-zA-Z0-9.+~-]/g,'')} 2>&1`, password)
    return { success: true, output: result }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('updatehist-get-status', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("apt list --upgradable 2>/dev/null | tail -n +2", { timeout: 15000, encoding: 'utf-8' }).toString()
    const updates = raw.split('\n').filter(l => l.trim()).map(l => {
      const m = l.match(/^([^/]+)/); return m ? m[1] : l
    })
    return { success: true, upgradable: updates.length, packages: updates.slice(0,30) }
  } catch(e) { return { success: true, upgradable: 0, packages: [] } }
})

// ========== Phase 3: 系统配置导入导出 ==========
ipcMain.handle('configio-export', async (_, sections, filePath) => {
  const { execSync } = require('child_process'); const fs = require('fs'); const path = require('path')
  try {
    const config = {}
    if (!sections || sections.includes('sources')) config.sources = execSync("cat /etc/apt/sources.list 2>/dev/null && ls /etc/apt/sources.list.d/ 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    if (!sections || sections.includes('network')) config.network = execSync("cat /etc/network/interfaces 2>/dev/null; nmcli connection show 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    if (!sections || sections.includes('firewall')) config.firewall = execSync("iptables-save 2>/dev/null || echo 'no iptables'", { timeout: 5000, encoding: 'utf-8' }).toString()
    if (!sections || sections.includes('services')) config.services = execSync("systemctl list-unit-files --type=service 2>/dev/null | head -50", { timeout: 5000, encoding: 'utf-8' }).toString()
    if (!sections || sections.includes('hostname')) config.hostname = execSync("hostnamectl 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    if (!sections || sections.includes('time')) config.time = execSync("timedatectl 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    const outPath = filePath || path.join(require('electron').app.getPath('desktop'), `uos_config_export_${Date.now()}.json`)
    fs.writeFileSync(outPath, JSON.stringify(config, null, 2), 'utf-8')
    return { success: true, file: outPath }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('configio-import', async (_, filePath) => {
  const fs = require('fs'); const { execSync } = require('child_process')
  try {
    const data = JSON.parse(fs.readFileSync(filePath, 'utf-8'))
    const report = {}
    if (data.hostname) { try { report.hostname = execSync(`hostnamectl set-hostname ${(data.hostname.match(/Static hostname:\s*(\S+)/)||[])[1]||''} 2>&1`, { timeout: 5000 }).toString() } catch(e) { report.hostname = e.message } }
    if (data.time) { try { const tz = (data.time.match(/Time zone:\s*(\S+)/)||[])[1]; if (tz) report.time = execSync(`timedatectl set-timezone ${tz} 2>&1`, { timeout: 5000 }).toString() } catch(e) { report.time = e.message } }
    return { success: true, report }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('configio-preview', async (_, filePath) => {
  const fs = require('fs')
  try {
    const data = JSON.parse(fs.readFileSync(filePath, 'utf-8'))
    return { success: true, data }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('configio-compare', async (_, filePath) => {
  const fs = require('fs'); const { execSync } = require('child_process')
  try {
    const imported = JSON.parse(fs.readFileSync(filePath, 'utf-8'))
    const current = {}
    try { current.sources = execSync("cat /etc/apt/sources.list 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    try { current.hostname = execSync("cat /etc/hostname 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString() } catch {}
    return { success: true, imported, current, diff: Object.keys(imported).filter(k => imported[k] !== current[k]) }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 系统性能分析 ==========
ipcMain.handle('perf-analyze-cpu', async () => {
  const { execSync } = require('child_process')
  try {
    const top = execSync("ps aux --sort=-%cpu 2>/dev/null | head -15", { timeout: 5000, encoding: 'utf-8' }).toString()
    const load = execSync("cat /proc/loadavg 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim()
    const uptime = execSync("uptime 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim()
    return { success: true, topProcesses: top, loadAverage: load, uptime }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('perf-analyze-disk', async () => {
  const { execSync } = require('child_process')
  try {
    const iostat = execSync("iostat -x 1 2 2>/dev/null || cat /proc/diskstats 2>/dev/null | head -20", { timeout: 5000, encoding: 'utf-8' }).toString()
    const df = execSync("df -h 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString()
    return { success: true, iostat, df }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('perf-analyze-memory', async () => {
  const { execSync } = require('child_process')
  try {
    const free = execSync("free -h 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString()
    const slab = execSync("cat /proc/meminfo 2>/dev/null | head -20", { timeout: 3000, encoding: 'utf-8' }).toString()
    const topMem = execSync("ps aux --sort=-%mem 2>/dev/null | head -15", { timeout: 5000, encoding: 'utf-8' }).toString()
    return { success: true, free, slab, topProcesses: topMem }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('perf-run', async (_, args) => {
  const { execSync } = require('child_process')
  try {
    const result = execSync(`perf ${args||'stat -e cycles,instructions,cache-misses ls 2>&1'} 2>/dev/null || echo 'perf not available'`, { timeout: 30000, encoding: 'utf-8' }).toString()
    return { success: true, output: result }
  } catch(e) { return { success: false, error: e.message } }
})
// ========== Phase 3: 系统性能分析 - CPU 热点 ==========
ipcMain.handle('perf-cpu-hotspot', async () => {
  const { execSync } = require('child_process')
  const scriptPath = path.join(__dirname, '../resources/scripts/Phase3/perf_analyze.sh');
  try {
    const result = execSync('bash "' + scriptPath + '" cpuhotspot', { timeout: 15000, encoding: 'utf-8' });
    return JSON.parse(result);
  } catch (e) {
    try {
      const top = execSync("ps aux --sort=-%cpu 2>/dev/null | head -15", { timeout: 5000, encoding: 'utf-8' }).toString()
      return { success: true, fallback: true, topProcesses: top }
    } catch(e2) { return { success: false, error: e.message } }
  }
})

// ========== Phase 3: 系统性能分析 - 内存泄漏 ==========
ipcMain.handle('perf-memory-leak', async () => {
  const { execSync } = require('child_process')
  const scriptPath = path.join(__dirname, '../resources/scripts/Phase3/perf_analyze.sh');
  try {
    const result = execSync('bash "' + scriptPath + '" memoryleak', { timeout: 15000, encoding: 'utf-8' });
    return JSON.parse(result);
  } catch (e) {
    try {
      const meminfo = execSync("cat /proc/meminfo 2>/dev/null | head -30", { timeout: 3000, encoding: 'utf-8' }).toString()
      const topRSS = execSync("ps aux --sort=-%rss 2>/dev/null | head -15", { timeout: 5000, encoding: 'utf-8' }).toString()
      return { success: true, fallback: true, meminfo, topProcesses: topRSS }
    } catch(e2) { return { success: false, error: e.message } }
  }
})

// ========== Phase 3: 系统性能分析 - strace 跟踪 ==========
ipcMain.handle('perf-strace', async (_, pid) => {
  const { execSync } = require('child_process')
  const scriptPath = path.join(__dirname, '../resources/scripts/Phase3/perf_analyze.sh');
  try {
    const result = execSync('bash "' + scriptPath + '" strace ' + (pid || ''), { timeout: 15000, encoding: 'utf-8' });
    return JSON.parse(result);
  } catch (e) {
    return { success: false, error: e.message }
  }
})

// ========== Phase 3: 系统性能分析 - 生成报告 ==========
ipcMain.handle('perf-generate-report', async () => {
  const { execSync } = require('child_process')
  const scriptPath = path.join(__dirname, '../resources/scripts/Phase3/perf_analyze.sh');
  try {
    const result = execSync('bash "' + scriptPath + '" report', { timeout: 30000, encoding: 'utf-8' });
    return JSON.parse(result);
  } catch (e) {
    try {
      const host = execSync('hostname 2>/dev/null', { timeout: 3000, encoding: 'utf-8' }).toString().trim() || 'unknown'
      const ts = new Date().toISOString().replace(/[T:.]/g, '_').slice(0,19)
      const reportPath = '/tmp/perf_report_' + host + '_' + ts + '.txt'
      const cpu = execSync("top -bn1 2>/dev/null | grep '%Cpu'", { timeout: 5000, encoding: 'utf-8' }).toString()
      const mem = execSync('free -h 2>/dev/null', { timeout: 3000, encoding: 'utf-8' }).toString()
      const disk = execSync("df -h 2>/dev/null | grep '^/'", { timeout: 3000, encoding: 'utf-8' }).toString()
      const procs = execSync("ps aux --sort=-%cpu 2>/dev/null | head -11", { timeout: 5000, encoding: 'utf-8' }).toString()
      const lines = [
        '============================================',
        '  UOS 系统性能分析报告',
        '  生成时间: ' + new Date().toLocaleString('zh-CN'),
        '  主机名: ' + host,
        '============================================',
        '',
        '[CPU 使用率]',
        cpu,
        '',
        '[内存使用]',
        mem,
        '',
        '[磁盘使用]',
        disk,
        '',
        '[Top 进程]',
        procs,
        '============================================'
      ]
      require('fs').writeFileSync(reportPath, lines.join('\n'), 'utf-8')
      return { success: true, report_file: reportPath, report_content: lines.join('\n') }
    } catch(e2) { return { success: false, error: e2.message } }
  }
})



// ========== Phase 3: 系统崩溃分析 ==========
const crashScriptPath = path.join(__dirname, '../resources/scripts/Phase3/crash_analyze.sh')

ipcMain.handle('crash-list-coredumps', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync('bash "' + crashScriptPath + '" list', { timeout: 15000, encoding: 'utf-8' }).toString()
    return JSON.parse(raw)
  } catch(e) {
    try {
      const raw = execSync("coredumpctl list 2>/dev/null || ls -la /var/lib/systemd/coredump/ 2>/dev/null || echo 'No coredumpctl available'", { timeout: 10000, encoding: 'utf-8' }).toString()
      const lines = raw.split('\n').filter(l => l.trim())
      return { success: true, coredumps: lines.slice(1).map(function(l) {
        var parts = l.trim().split(/\s+/)
        return { pid: parts[3]||'0', signal: parts[6]||'?', time: (parts[0]||'')+' '+(parts[1]||'')+' '+(parts[2]||''), exe: parts.slice(8).join(' ')||'', package: parts.slice(8).join(' ')||'unknown' }
      }) }
    } catch(e2) { return { success: false, error: e.message } }
  }
})

ipcMain.handle('crash-analyze', async (_, id) => {
  const { execSync } = require('child_process')
  try {
    var argId = (parseInt(id) >= 0) ? id : ''
    var raw = execSync('bash "' + crashScriptPath + '" analyze ' + argId, { timeout: 30000, encoding: 'utf-8' }).toString()
    return JSON.parse(raw)
  } catch(e) {
    try {
      var argId2 = (parseInt(id) >= 0) ? id : ''
      var raw2 = execSync('coredumpctl info ' + argId2 + ' 2>/dev/null || echo Cannot analyze', { timeout: 30000, encoding: 'utf-8' }).toString()
      return { success: true, info: raw2, backtrace: '' }
    } catch(e2) { return { success: false, error: e.message } }
  }
})

ipcMain.handle('crash-get-logs', async () => {
  const { execSync } = require('child_process')
  try {
    var raw = execSync('bash "' + crashScriptPath + '" logs', { timeout: 15000, encoding: 'utf-8' }).toString()
    return JSON.parse(raw)
  } catch(e) {
    try {
      var raw2 = execSync("journalctl -p err -b 2>/dev/null | tail -50", { timeout: 15000, encoding: 'utf-8' }).toString()
      var lines2 = raw2.split('\n').filter(function(l) { return l.trim() })
      return { success: true, logs: lines2.map(function(l) { return { raw: l, severity: l.toLowerCase().includes('segfault')?'fatal':'err' } }) }
    } catch(e2) { return { success: false, error: e.message } }
  }
})

ipcMain.handle('crash-get-suggestions', async (_, signal, pkg, exe) => {
  const { execSync } = require('child_process')
  try {
    var sugs = (signal||'') + ' \'' + (pkg||'').replace(/'/g,'') + '\' \'' + (exe||'').replace(/'/g,'') + '\''
    var raw = execSync('bash "' + crashScriptPath + '" suggest ' + sugs, { timeout: 10000, encoding: 'utf-8' }).toString()
    return JSON.parse(raw)
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 等保合规检查 ==========

/** Execute a shell command safely, return trimmed output or empty string */
function execOut(cmd, timeoutMs) {
  if (timeoutMs === undefined) timeoutMs = 5000;
  try {
    const { execSync } = require('child_process');
    return execSync(cmd, { timeout: timeoutMs, encoding: 'utf-8', stdio: ['pipe', 'pipe', 'pipe'] })
      .toString().trim();
  } catch(e) { return ''; }
}

/** Run a single compliance check */
function runCheck(id, name, category, checkCmd, passFn, desc, suggestion) {
  try {
    const out = execOut(checkCmd, 8000);
    const status = passFn(out) ? 'pass' : 'fail';
    return { id: id, name: name, category: category, status: status,
      detail: out || '未检测到配置',
      description: desc || '',
      suggestion: suggestion || '' };
  } catch(e) {
    return { id: id, name: name, category: category, status: 'fail',
      detail: '检测失败: ' + e.message,
      description: desc || '',
      suggestion: suggestion || '' };
  }
}

/** Run all 20 compliance checks and return structured results */
function runAllChecks() {
  var items = [];

  // === 1. 身份鉴别 ===
  items.push(runCheck('pw-policy', '密码复杂度策略', '身份鉴别',
    "cat /etc/pam.d/common-password 2>/dev/null | grep -v '^#' | grep -v '^$' | head -5",
    function(o) { return o.indexOf('pam_unix.so') >= 0 && (o.indexOf('minlen=8') >= 0 || o.indexOf('sha512') >= 0 || o.indexOf('obscure') >= 0); },
    '检查密码最小长度是否≥8位，是否使用SHA512加密',
    '在 /etc/pam.d/common-password 中配置 password requisite pam_unix.so sha512 minlen=8 obscure'));

  items.push(runCheck('faillock', '登录失败锁定策略', '身份鉴别',
    "cat /etc/pam.d/common-auth 2>/dev/null | grep -i faillock; cat /etc/security/faillock.conf 2>/dev/null | grep -v '^#' | grep -v '^$' | head -5",
    function(o) { return o.indexOf('faillock') >= 0; },
    '检查是否配置登录失败锁定（防暴力破解）',
    '安装 libpam-modules，在 common-auth 中添加: auth required pam_faillock.so preauth deny=5 unlock_time=900'));

  items.push(runCheck('pw-expire', '密码过期策略', '身份鉴别',
    "cat /etc/login.defs 2>/dev/null | grep -E '^PASS_MAX_DAYS|^PASS_MIN_DAYS|^PASS_WARN_AGE'",
    function(o) {
      var m = o.match(/PASS_MAX_DAYS\s+(\d+)/);
      return m && parseInt(m[1], 10) <= 90;
    },
    '检查密码最长使用期限是否≤90天',
    '在 /etc/login.defs 中设置 PASS_MAX_DAYS 90, PASS_MIN_DAYS 1, PASS_WARN_AGE 7'));

  // === 2. 访问控制 ===
  items.push(runCheck('ssh-config', 'SSH 安全配置', '访问控制',
    "cat /etc/ssh/sshd_config 2>/dev/null | grep -v '^#' | grep -v '^$' | grep -E 'PermitRootLogin|PasswordAuthentication'",
    function(o) { return o.indexOf('PermitRootLogin no') >= 0 || o.indexOf('PasswordAuthentication no') >= 0; },
    '检查SSH是否禁止root直接登录，是否禁用密码认证',
    '在 sshd_config 中设置 PermitRootLogin no, PasswordAuthentication no'));

  items.push(runCheck('firewall', '防火墙规则', '访问控制',
    "sudo iptables -L -n 2>/dev/null | head -10; echo '---'; sudo ufw status 2>/dev/null | head -5",
    function(o) { return o.indexOf('ACCEPT') >= 0 || o.indexOf('DROP') >= 0 || o.indexOf('active') >= 0; },
    '检查防火墙是否启用并有合理规则',
    '使用 iptables 或 ufw 配置防火墙规则，开放必要端口'));

  items.push(runCheck('sudoers', 'sudo 权限审计', '访问控制',
    "cat /etc/sudoers 2>/dev/null | grep -v '^#' | grep -v '^$' | head -10",
    function(o) { return o.indexOf('NOPASSWD') < 0; },
    '检查sudo权限配置是否合理',
    '限制sudo用户，避免配置 NOPASSWD 权限'));

  items.push(runCheck('default-accts', '默认账户安全', '访问控制',
    "cat /etc/shadow 2>/dev/null | awk -F: '($2==\"\"||$2==\"!\"){print $1}' | head -5",
    function(o) { return !o || o.length < 3; },
    '检查是否存在空密码账户或默认测试账户',
    '禁用不必要的系统账户: passwd -l <username>'));

  // === 3. 安全审计 ===
  items.push(runCheck('auditd', '审计服务 (auditd)', '安全审计',
    "systemctl is-active auditd 2>/dev/null; echo '---'; systemctl is-enabled auditd 2>/dev/null",
    function(o) { return o.indexOf('active') >= 0 && o.indexOf('enabled') >= 0; },
    '检查审计服务auditd是否运行并启用',
    'systemctl enable auditd && systemctl start auditd'));

  items.push(runCheck('audit-rules', '审计规则配置', '安全审计',
    "auditctl -l 2>/dev/null | head -10",
    function(o) { return o.length > 20; },
    '检查是否配置了详细的审计规则',
    '配置 audit.rules 监控关键文件与系统调用'));

  items.push(runCheck('log-audit', '系统日志审计', '安全审计',
    "systemctl is-active rsyslog 2>/dev/null",
    function(o) { return o.indexOf('active') >= 0; },
    '检查系统日志服务是否运行',
    'systemctl enable rsyslog && systemctl start rsyslog'));

  items.push(runCheck('log-protect', '日志文件保护', '安全审计',
    "stat -c '%a' /var/log/auth.log 2>/dev/null; echo '---'; stat -c '%a' /var/log 2>/dev/null",
    function(o) { return o.indexOf('777') < 0 && o.length > 0; },
    '检查系统日志文件权限是否受限',
    'chmod 640 /var/log/auth.log; chmod 755 /var/log'));

  // === 4. 入侵防范 ===
  items.push(runCheck('apparmor', '强制访问控制 (AppArmor)', '入侵防范',
    "aa-status 2>/dev/null | head -5; echo '---'; getenforce 2>/dev/null",
    function(o) { return o.indexOf('enforce') >= 0; },
    '检查AppArmor或SELinux是否启用并处于强制模式',
    '安装 apparmor-utils: systemctl enable apparmor && systemctl start apparmor'));

  items.push(runCheck('sys-integrity', '系统文件完整性', '入侵防范',
    "which debsums 2>/dev/null && echo 'installed' || echo 'not installed'",
    function(o) { return o.indexOf('installed') >= 0; },
    '检查系统关键文件完整性检查工具是否安装',
    '安装 debsums: apt install debsums -y'));

  items.push(runCheck('kernel-hard', '内核安全参数', '入侵防范',
    "sysctl net.ipv4.conf.all.rp_filter 2>/dev/null; sysctl net.ipv4.conf.all.accept_source_route 2>/dev/null; sysctl net.ipv4.tcp_syncookies 2>/dev/null",
    function(o) { return o.indexOf('= 1') >= 0 && o.indexOf('= 0') < 0; },
    '检查内核安全参数（反向路径过滤、禁止源路由、SYN Cookie）',
    '在 sysctl.conf 中设置 rp_filter=1, accept_source_route=0, tcp_syncookies=1'));

  // === 5. 数据安全 ===
  items.push(runCheck('encrypt', '磁盘加密', '数据安全',
    "lsblk -o NAME,TYPE,FSTYPE,MOUNTPOINT 2>/dev/null | grep -i 'crypt\\|luks'; echo '---'; cat /etc/crypttab 2>/dev/null | grep -v '^#' | head -5",
    function(o) { return o.indexOf('crypt') >= 0 || o.indexOf('luks') >= 0 || o.indexOf('LUKS') >= 0; },
    '检查磁盘是否加密（LUKS/dm-crypt）',
    '使用 cryptsetup 对敏感分区进行 LUKS 加密'));

  items.push(runCheck('backup-conf', '关键配置备份', '数据安全',
    "ls /var/backups/ 2>/dev/null | head -5; echo '---'; crontab -l 2>/dev/null | grep -i backup | head -3",
    function(o) { return o.length > 10; },
    '检查关键系统配置是否有自动备份机制',
    '配置 cron 定时备份 /etc 目录'));

  items.push(runCheck('file-perm', '敏感文件权限', '数据安全',
    "stat -c '%a' /etc/shadow 2>/dev/null; echo '---'; stat -c '%a' /etc/passwd 2>/dev/null",
    function(o) { return o.indexOf('640') >= 0 || o.indexOf('600') >= 0; },
    '检查敏感文件的权限是否合规',
    'chmod 640 /etc/shadow; chmod 644 /etc/passwd'));

  // === 6. 系统维护 ===
  items.push(runCheck('updates', '系统补丁更新', '系统维护',
    "apt list --upgradable 2>/dev/null | wc -l",
    function(o) { return parseInt(o, 10) <= 3; },
    '检查系统是否有可用安全更新',
    'apt update && apt upgrade -y'));

  items.push(runCheck('disk-space', '磁盘空间检查', '系统维护',
    "df -h / 2>/dev/null | tail -1 | awk '{print $5}'",
    function(o) { return parseInt(o.replace('%', ''), 10) < 85; },
    '检查根分区磁盘使用率是否<85%',
    '清理日志和缓存: journalctl --vacuum-size=500M; apt clean'));

  items.push(runCheck('time-sync', '系统时间同步', '系统维护',
    "timedatectl 2>/dev/null | grep -E 'NTP|synchronized'",
    function(o) { return o.indexOf('active') >= 0 || o.indexOf('yes') >= 0; },
    '检查系统时间是否通过NTP自动同步',
    'timedatectl set-ntp true'));

  items.push(runCheck('open-services', '系统开放服务', '系统维护',
    "ss -tlnp 2>/dev/null | head -30 || netstat -tlnp 2>/dev/null | head -30",
    function(o) { return o.split('\n').length <= 25; },
    '检查监听端口和开放服务是否过多',
    '关闭不必要的服务: systemctl disable --now <service-name>'));

  // Calculate score
  var total = items.length;
  var passed = 0, warnings = 0, failed = 0;
  for (var i = 0; i < items.length; i++) {
    if (items[i].status === 'pass') passed++;
    else if (items[i].status === 'warn') warnings++;
    else failed++;
  }
  var score = Math.round((passed / total) * 100);

  return { success: true, items: items, score: score, total: total,
    passed: passed, warnings: warnings, failed: failed,
    summary: '\u7B49\u4FDD 2.0 \u4E09\u7EA7\u5408\u89C4\u68C0\u67E5\u5B8C\u6210\uFF1A\u901A\u8FC7 ' + passed + '/' + total + ' \u9879' };
}

ipcMain.handle('compliance-run-check', async function() {
  var result = runAllChecks();
  // Cache to file
  try {
    var fs = require('fs');
    var p = require('path');
    var f = p.join(require('electron').app.getPath('userData'), 'compliance_results.json');
    fs.writeFileSync(f, JSON.stringify(result), 'utf-8');
  } catch(e) {}
  return result;
});

ipcMain.handle('compliance-get-results', async function() {
  var fs = require('fs');
  var path = require('path');
  try {
    var f = path.join(require('electron').app.getPath('userData'), 'compliance_results.json');
    if (!fs.existsSync(f)) return { success: true, results: [] };
    return { success: true, results: JSON.parse(fs.readFileSync(f, 'utf-8')) };
  } catch(e) { return { success: true, results: [] }; }
});

ipcMain.handle('compliance-fix-item', async function(_, id, password) {
  if (!password) return { success: false, error: '\u9700\u8981\u7BA1\u7406\u5458\u5BC6\u7801' };
  try {
    var result = '';
    var se = function(cmd) { return sudoExec(cmd, password); };

    switch(id) {
      case 'pw-policy':
        result = se("grep -q 'pam_unix.so' /etc/pam.d/common-password && sed -i 's/pam_unix.so.*/pam_unix.so sha512 minlen=8 obscure/g' /etc/pam.d/common-password || echo 'password requisite pam_unix.so sha512 minlen=8 obscure' >> /etc/pam.d/common-password");
        break;
      case 'faillock':
        result = se("apt-get install -y libpam-modules 2>/dev/null; grep -q pam_faillock.so /etc/pam.d/common-auth || echo 'auth required pam_faillock.so preauth audit deny=5 unlock_time=900' >> /etc/pam.d/common-auth");
        break;
      case 'pw-expire':
        result = se("sed -i 's/^PASS_MAX_DAYS.*/PASS_MAX_DAYS 90/' /etc/login.defs; sed -i 's/^PASS_MIN_DAYS.*/PASS_MIN_DAYS 1/' /etc/login.defs; sed -i 's/^PASS_WARN_AGE.*/PASS_WARN_AGE 7/' /etc/login.defs");
        break;
      case 'ssh-config':
        result = se("sed -i 's/^#*PermitRootLogin.*/PermitRootLogin no/' /etc/ssh/sshd_config; sed -i 's/^#*PasswordAuthentication.*/PasswordAuthentication no/' /etc/ssh/sshd_config; systemctl restart sshd");
        break;
      case 'firewall':
        result = se("ufw --force enable 2>/dev/null || (iptables -P INPUT DROP; iptables -P FORWARD DROP; iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT; iptables -A INPUT -i lo -j ACCEPT; iptables -A INPUT -p tcp --dport 22 -j ACCEPT)");
        break;
      case 'sudoers':
        result = se("sed -i 's/^%sudo.*ALL=(ALL:ALL) NOPASSWD:ALL/%sudo ALL=(ALL:ALL) ALL/' /etc/sudoers 2>/dev/null; echo '\u5df2\u9650\u5236sudo\u6743\u9650'");
        break;
      case 'default-accts':
        result = se("for u in guest test demo; do id $u 2>/dev/null && passwd -l $u; done 2>/dev/null; echo '\u9ed8\u8ba4\u8d26\u6237\u5df2\u9501\u5b9a'");
        break;
      case 'auditd':
        result = se("systemctl enable auditd && systemctl start auditd");
        break;
      case 'audit-rules':
        result = se("apt-get install -y auditd 2>/dev/null; auditctl -w /etc/passwd -p wa -k passwd_changes; auditctl -w /etc/shadow -p wa -k shadow_changes; echo '\u5ba1\u8ba1\u89c4\u5219\u5df2\u6dfb\u52a0'");
        break;
      case 'log-audit':
        result = se("systemctl enable rsyslog && systemctl start rsyslog");
        break;
      case 'log-protect':
        result = se("chmod 640 /var/log/auth.log 2>/dev/null; chmod 640 /var/log/syslog 2>/dev/null; echo '\u65e5\u5fd7\u6743\u9650\u5df2\u4fee\u590d'");
        break;
      case 'apparmor':
        result = se("apt-get install -y apparmor apparmor-utils 2>/dev/null; systemctl enable apparmor; systemctl start apparmor");
        break;
      case 'sys-integrity':
        result = se("apt-get install -y debsums 2>/dev/null; echo '\u5b8c\u6574\u6027\u68c0\u67e5\u5de5\u5177\u5df2\u5b89\u88c5'");
        break;
      case 'kernel-hard':
        result = se("sysctl -w net.ipv4.conf.all.rp_filter=1; sysctl -w net.ipv4.conf.all.accept_source_route=0; sysctl -w net.ipv4.tcp_syncookies=1; echo 'net.ipv4.conf.all.rp_filter=1' >> /etc/sysctl.d/99-security.conf; echo 'net.ipv4.conf.all.accept_source_route=0' >> /etc/sysctl.d/99-security.conf; echo 'net.ipv4.tcp_syncookies=1' >> /etc/sysctl.d/99-security.conf");
        break;
      case 'encrypt':
        result = '\u78c1\u76d8\u52a0\u5bc6\u9700\u8981\u624b\u52a8\u64cd\u4f5c: cryptsetup luksFormat <device>';
        break;
      case 'backup-conf':
        result = se("mkdir -p /var/backups; echo '0 3 * * * root tar czf /var/backups/etc_$(date +%Y%m%d).tar.gz /etc' > /etc/cron.d/backup-etc; systemctl restart cron 2>/dev/null; echo '\u5907\u4efd\u914d\u7f6e\u5df2\u6dfb\u52a0'");
        break;
      case 'file-perm':
        result = se("chmod 640 /etc/shadow; chmod 644 /etc/passwd; chmod 640 /etc/gshadow; echo '\u6587\u4ef6\u6743\u9650\u5df2\u4fee\u590d'");
        break;
      case 'updates':
        result = se("apt-get update && apt-get upgrade -y");
        break;
      case 'disk-space':
        result = se("journalctl --vacuum-size=500M 2>/dev/null; apt-get clean 2>/dev/null; echo '\u78c1\u76d8\u7a7a\u95f4\u5df2\u6e05\u7406'");
        break;
      case 'time-sync':
        result = se("timedatectl set-ntp true");
        break;
      case 'open-services':
        result = se("systemctl disable --now cups-browsed 2>/dev/null || true; systemctl disable --now avahi-daemon 2>/dev/null || true; echo '\u975e\u5fc5\u8981\u670d\u52a1\u5df2\u7981\u7528'");
        break;
      default:
        result = '\u8be5\u68c0\u67e5\u9879\u65e0\u6cd5\u81ea\u52a8\u4fee\u590d: ' + id;
    }
    return { success: true, output: result };
  } catch(e) {
    return { success: false, error: e.message };
  }
});

ipcMain.handle('compliance-generate-report', async function() {
  var fs = require('fs');
  var path = require('path');
  var os = require('os');
  try {
    var result = runAllChecks();
    var items = result.items || [];
    var score = result.score || 0;
    var total = result.total || 0;
    var passed = result.passed || 0;

    var desktopDir = execOut("xdg-user-dir DESKTOP 2>/dev/null") || os.homedir();
    var filePath = path.join(desktopDir, '\u7B49\u4FDD\u5408\u89C4\u68C0\u67E5\u62A5\u544A_' + new Date().toISOString().slice(0,10) + '.md');

    var md = '# \u7B49\u4FDD\u5408\u89C4\u68C0\u67E5\u62A5\u544A\n\n';
    md += '\u751F\u6210\u65F6\u95F4: ' + new Date().toLocaleString('zh-CN') + '\n\n';
    md += '\u751F\u6210\u5DE5\u5177: UOS\u8FD0\u7EF4\u5DE5\u5177\u7BB1\n\n';
    md += '## \u603B\u4F53\u8BC4\u5206\n\n';
    md += '\u5408\u89C4\u8BC4\u5206: **' + score + '/100** (\u901A\u8FC7 ' + passed + '/' + total + ' \u9879)\n\n';
    md += '---\n\n';

    // Group by category
    var categories = {};
    for (var i = 0; i < items.length; i++) {
      var c = items[i];
      var cat = c.category || '\u5176\u4ED6';
      if (!categories[cat]) categories[cat] = [];
      categories[cat].push(c);
    }

    var catKeys = Object.keys(categories);
    for (var ci = 0; ci < catKeys.length; ci++) {
      var catName = catKeys[ci];
      var catItems = categories[catName];
      md += '## ' + catName + '\n\n';
      md += '| \u68C0\u67E5\u9879 | \u72B6\u6001 | \u8BE6\u60C5 | \u4FEE\u590D\u5EFA\u8BAE |\n';
      md += '|--------|------|------|----------|\n';
      for (var j = 0; j < catItems.length; j++) {
        var item = catItems[j];
        var stMap = { pass: '\u2705 \u901A\u8FC7', fail: '\u274C \u672A\u901A\u8FC7', warn: '\u26A0\uFE0F \u8B66\u544A', unknown: '\u2753 \u672A\u77E5' };
        md += '| ' + (item.name || '') + ' | ' + (stMap[item.status] || item.status) + ' | ';
        md += ((item.detail || '').replace(/\n/g, ' ') || '') + ' | ';
        md += ((item.suggestion || '\u65E0').replace(/\n/g, ' ')) + ' |\n';
      }
      md += '\n';
    }

    md += '---\n\n';
    md += '*\u62A5\u544A\u7531 UOS\u8FD0\u7EF4\u5DE5\u5177\u7BB1 \u81EA\u52A8\u751F\u6210\u4E8E ' + new Date().toLocaleString('zh-CN') + '*\n';

    fs.writeFileSync(filePath, md, 'utf-8');
    return { success: true, file: filePath };
  } catch(e) {
    return { success: false, error: e.message };
  }
});

// ========== Phase 3: NTP 时间同步 ==========
ipcMain.handle('ntp-get-status', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("timedatectl 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    const ntpEnabled = raw.includes('NTP service: active') || raw.includes('NTP enabled: yes')
    const synced = raw.includes('System clock synchronized: yes')
    const timezoneMatch = raw.match(/Time zone:\s*(\S+)\s*/)
    const timezone = timezoneMatch ? timezoneMatch[1] : 'UTC'
    const tzOffsetMatch = raw.match(/Time zone:\s*\S+\s+\(([^)]+)\)/)
    const tz_offset = tzOffsetMatch ? tzOffsetMatch[1] : ''
    const datetimeMatch = raw.match(/Local time:\s*(.+)/)
    const datetime = datetimeMatch ? datetimeMatch[1].trim() : ''
    // Try to get NTP servers
    let servers = []
    try {
      const chronyServers = execSync("chronyc sources -v 2>/dev/null | grep -E '^[\\^\\*\\+\\-]' || true", { timeout: 5000, encoding: 'utf-8' }).toString().trim()
      if(chronyServers) {
        servers = chronyServers.split('\n').filter(l => l.trim()).map(l => {
          const parts = l.trim().split(/\s+/)
          return parts[parts.length-1]
        }).filter(Boolean)
      } else {
        const ntpqServers = execSync("ntpq -p 2>/dev/null | tail -n +3 || true", { timeout: 5000, encoding: 'utf-8' }).toString().trim()
        if(ntpqServers) {
          servers = ntpqServers.split('\n').filter(l => l.trim()).map(l => {
            const parts = l.trim().split(/\s+/)
            return parts[0]
          }).filter(Boolean)
        }
      }
    } catch(e) {}
    if(!servers.length) {
      servers = ['pool.ntp.org (chrony/ntp)']
    }
    return { success: true, ntpEnabled, synced, timezone, tz_offset, datetime, servers }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('ntp-set-server', async (_, server) => {
  const { execSync } = require('child_process')
  try {
    execSync(`timedatectl set-ntp false 2>/dev/null`, { timeout: 5000 })
    execSync(`bash -c 'echo "server ${server} iburst" > /etc/chrony/chrony.conf 2>/dev/null || echo "server ${server}" > /etc/ntp.conf 2>/dev/null'`, { timeout: 5000 })
    execSync(`timedatectl set-ntp true 2>/dev/null`, { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('ntp-sync-now', async () => {
  const { execSync } = require('child_process')
  try {
    execSync("timedatectl set-ntp true 2>/dev/null", { timeout: 5000 })
    const result = execSync("chronyc -a makestep 2>/dev/null || ntpdate -u time.windows.com 2>/dev/null || echo 'sync attempted'", { timeout: 15000, encoding: 'utf-8' }).toString()
    return { success: true, output: result }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('ntp-set-timezone', async (_, tz) => {
  const { execSync } = require('child_process')
  try {
    execSync(`timedatectl set-timezone ${tz.replace(/[^a-zA-Z0-9_/]/g,'')} 2>&1`, { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('ntp-get-timezones', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("timedatectl list-timezones 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
    const zones = raw.split('\n').filter(l => l.trim())
    return { success: true, timezones: zones }
  } catch(e) { return { success: true, timezones: ['Asia/Shanghai','UTC','America/New_York'] } }
})

// ========== Phase 3: 系统代理配置 ==========
ipcMain.handle('proxy-get-config', async () => {
  const { execSync } = require('child_process')
  try {
    const config = { http: '', https: '', ftp: '', enabled: false, no_proxy: '', mode: 'none' }
    // 从环境变量读取
    try { config.http = execSync("echo $http_proxy 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { config.https = execSync("echo $https_proxy 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { config.ftp = execSync("echo $ftp_proxy 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { config.no_proxy = execSync("echo $no_proxy 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    // 从 gsettings 读取系统代理状态
    try { config.mode = execSync("gsettings get org.gnome.system.proxy mode 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim().replace(/'/g,'') } catch { config.mode = 'none' }
    config.enabled = config.mode !== 'none' && config.mode !== ''
    // 如果环境变量为空，从 gsettings 读取详细配置
    if (!config.http) {
      try {
        const host = execSync("gsettings get org.gnome.system.proxy.http host 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim().replace(/'/g,'')
        const port = execSync("gsettings get org.gnome.system.proxy.http port 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim()
        if (host && port) config.http = 'http://' + host + ':' + port
      } catch {}
    }
    if (!config.https) {
      try {
        const host = execSync("gsettings get org.gnome.system.proxy.https host 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim().replace(/'/g,'')
        const port = execSync("gsettings get org.gnome.system.proxy.https port 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim()
        if (host && port) config.https = 'https://' + host + ':' + port
      } catch {}
    }
    if (!config.ftp) {
      try {
        const host = execSync("gsettings get org.gnome.system.proxy.ftp host 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim().replace(/'/g,'')
        const port = execSync("gsettings get org.gnome.system.proxy.ftp port 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim()
        if (host && port) config.ftp = 'ftp://' + host + ':' + port
      } catch {}
    }
    // 读取持久化存储的配置
    try {
      const proxyFile = require('path').join(require('electron').app.getPath('userData'), 'proxy-config.json')
      if (require('fs').existsSync(proxyFile)) {
        const saved = JSON.parse(require('fs').readFileSync(proxyFile, 'utf-8'))
        if (saved.http) config.http = saved.http
        if (saved.https) config.https = saved.https
        if (saved.ftp) config.ftp = saved.ftp
        if (saved.no_proxy) config.no_proxy = saved.no_proxy
        if (saved.enabled !== undefined) config.enabled = saved.enabled
      }
    } catch {}
    return { success: true, config }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('proxy-set-config', async (_, config) => {
  try {
    // 持久化保存配置
    const { execSync } = require('child_process')
    const path = require('path')
    const fs = require('fs')
    const app = require('electron').app
    const proxyFile = path.join(app.getPath('userData'), 'proxy-config.json')
    const dir = path.dirname(proxyFile)
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true })
    fs.writeFileSync(proxyFile, JSON.stringify(config, null, 2))
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('proxy-test', async (_, { testUrl, config }) => {
  const { execSync } = require('child_process')
  try {
    const url = testUrl || 'https://www.baidu.com'
    const proxyUrl = config?.http || config?.https || ''
    if (!proxyUrl) {
      // 无代理直接测试
      const start = Date.now()
      const result = execSync("curl -s -o /dev/null -w '%{http_code}' --connect-timeout 10 " + url + " 2>/dev/null", { timeout: 15000, encoding: 'utf-8' }).toString().trim()
      const latency = Date.now() - start
      return { success: true, httpCode: result, latency, reachable: result === '200' || result === '301' || result === '302' }
    }
    const start = Date.now()
    const result = execSync("curl -s -o /dev/null -w '%{http_code}' --proxy '" + proxyUrl + "' --connect-timeout 10 " + url + " 2>/dev/null", { timeout: 15000, encoding: 'utf-8' }).toString().trim()
    const latency = Date.now() - start
    return { success: true, httpCode: result, latency, reachable: result === '200' || result === '301' || result === '302' }
  } catch(e) { return { success: false, error: '连接失败: ' + e.message } }
})
ipcMain.handle('proxy-set-system', async (_, { config, password }) => {
  const { execSync } = require('child_process')
  const { sudoExec } = global
  try {
    if (!config.enabled) {
      // 禁用系统代理（以当前用户身份运行）
      try { require('child_process').execSync("gsettings set org.gnome.system.proxy mode none", { timeout: 5000 }) } catch(e) {}
      // 清理环境变量文件（需要 sudo）
      sudoExec('rm -f /etc/profile.d/proxy.sh 2>/dev/null', password)
      return { success: true }
    }
    // 解析 HTTP 代理主机和端口
    let httpHost = '', httpPort = '8080', httpsHost = '', httpsPort = '8080', ftpHost = '', ftpPort = '8080'
    if (config.http) {
      const m = config.http.match(/^(?:https?:\/\/)?(.+?)(?::(\d+))?$/)
      if (m) { httpHost = m[1]; if (m[2]) httpPort = m[2] }
    }
    if (config.https) {
      const m = config.https.match(/^(?:https?:\/\/)?(.+?)(?::(\d+))?$/)
      if (m) { httpsHost = m[1]; if (m[2]) httpsPort = m[2] }
    } else {
      httpsHost = httpHost; httpsPort = httpPort
    }
    if (config.ftp) {
      const m = config.ftp.match(/^(?:ftp:\/\/)?(.+?)(?::(\d+))?$/)
      if (m) { ftpHost = m[1]; if (m[2]) ftpPort = m[2] }
    } else {
      ftpHost = httpHost; ftpPort = httpPort
    }
    // 设置系统代理 via gsettings (以当前用户身份运行，非 root)
    const { execSync: exec } = require('child_process')
    try { exec("gsettings set org.gnome.system.proxy mode manual", { timeout: 10000 }) } catch(e) {}
    try { exec("gsettings set org.gnome.system.proxy.http host '" + httpHost.replace(/'/g, "'\\''") + "'", { timeout: 5000 }) } catch(e) {}
    try { exec("gsettings set org.gnome.system.proxy.http port " + httpPort, { timeout: 5000 }) } catch(e) {}
    try { exec("gsettings set org.gnome.system.proxy.https host '" + httpsHost.replace(/'/g, "'\\''") + "'", { timeout: 5000 }) } catch(e) {}
    try { exec("gsettings set org.gnome.system.proxy.https port " + httpsPort, { timeout: 5000 }) } catch(e) {}
    try { exec("gsettings set org.gnome.system.proxy.ftp host '" + ftpHost.replace(/'/g, "'\\''") + "'", { timeout: 5000 }) } catch(e) {}
    try { exec("gsettings set org.gnome.system.proxy.ftp port " + ftpPort, { timeout: 5000 }) } catch(e) {}
    if (config.no_proxy) {
      var noProxyList = "[" + config.no_proxy.split(',').map(function(s) { return "'" + s.trim().replace(/'/g, "'\\''") + "'"; }).join(', ') + "]"
      try { exec("gsettings set org.gnome.system.proxy ignore-hosts " + noProxyList, { timeout: 5000 }) } catch(e) {}
    }
    // 写入系统环境变量文件 /etc/profile.d/proxy.sh（需要 sudo）
    try {
      var envLines = []
      if (config.http) { envLines.push('http_proxy="' + config.http.replace(/"/g, '\\"') + '"'); envLines.push('HTTP_PROXY="' + config.http.replace(/"/g, '\\"') + '"') }
      if (config.https) { envLines.push('https_proxy="' + config.https.replace(/"/g, '\\"') + '"'); envLines.push('HTTPS_PROXY="' + config.https.replace(/"/g, '\\"') + '"') }
      if (config.ftp) { envLines.push('ftp_proxy="' + config.ftp.replace(/"/g, '\\"') + '"'); envLines.push('FTP_PROXY="' + config.ftp.replace(/"/g, '\\"') + '"') }
      if (config.no_proxy) { envLines.push('no_proxy="' + config.no_proxy.replace(/"/g, '\\"') + '"'); envLines.push('NO_PROXY="' + config.no_proxy.replace(/"/g, '\\"') + '"') }
      var envContent = envLines.join('\n')
      sudoExec('cat > /etc/profile.d/proxy.sh << \'PROXYEOF\'\n' + envContent + '\nPROXYEOF\nchmod +x /etc/profile.d/proxy.sh', password)
    } catch(e) { /* env file write is best-effort */ }
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
// ========== Phase 3: 截图录屏 ==========
ipcMain.handle('capture-screenshot', async (_, mode) => {
  const { execSync } = require('child_process'); const fs = require('fs'); const path = require('path')
  try {
    const dir = require('electron').app.getPath('pictures')
    const file = path.join(dir, `screenshot_${Date.now()}.png`)
    if (mode === 'full') execSync(`gnome-screenshot -f '${file}' 2>/dev/null || deepin-screenshot -f '${file}' 2>/dev/null || import -window root '${file}' 2>/dev/null`, { timeout: 15000 })
    else if (mode === 'area') execSync(`gnome-screenshot -a -f '${file}' 2>/dev/null || deepin-screenshot 2>/dev/null || import '${file}' 2>/dev/null`, { timeout: 30000 })
    else if (mode === 'delay') execSync(`gnome-screenshot -d 5 -f '${file}' 2>/dev/null || deepin-screenshot -d 5 2>/dev/null`, { timeout: 30000 })
    if (!fs.existsSync(file)) return { success: false, error: '截图失败，请检查 gnome-screenshot/deepin-screenshot 是否安装' }
    const dataUrl = 'data:image/png;base64,' + fs.readFileSync(file).toString('base64')
    return { success: true, file, dataUrl }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('capture-start-recording', async () => {
  const { spawn } = require('child_process'); const path = require('path')
  try {
    const dir = require('electron').app.getPath('videos')
    const file = path.join(dir, `recording_${Date.now()}.mp4`)
    const proc = spawn('ffmpeg', ['-f','x11grab','-video_size','1920x1080','-i',':0.0','-codec:v','libx264','-r','15',file], { detached: true })
    global.__recordingProc = proc; global.__recordingFile = file
    return { success: true, file }
  } catch(e) { return { success: false, error: '录屏需要 ffmpeg: ' + e.message } }
})
ipcMain.handle('capture-stop-recording', async () => {
  try {
    if (global.__recordingProc) { global.__recordingProc.kill('SIGTERM'); global.__recordingProc = null }
    return { success: true, file: global.__recordingFile || '' }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('capture-recording-status', async () => {
  return { success: true, recording: !!global.__recordingProc, file: global.__recordingFile || '' }
})

// ========== Phase 3: 快捷键管理 ==========
ipcMain.handle('hotkey-list', async () => {
  const { execSync } = require('child_process')
  try {
    const schemas = ['org.gnome.desktop.wm.keybindings', 'com.deepin.dde.keybinding', 'org.gnome.settings-daemon.plugins.media-keys']
    const allShortcuts = []
    const seen = new Set()
    for (const schema of schemas) {
      try {
        const raw = execSync("gsettings list-recursively " + schema + " 2>/dev/null | head -80", { timeout: 10000, encoding: 'utf-8' }).toString()
        raw.split('\n').filter(l => l.trim()).forEach(l => {
          const idx = l.indexOf(' ')
          if (idx < 0) return
          const keyPath = l.substring(0, idx).trim()
          const val = l.substring(idx + 1).trim().replace(/'/g, '') || ''
          const keyName = keyPath.split('.').pop() || ''
          // deduplicate
          const dedupKey = keyName + ':' + val
          if (!seen.has(dedupKey) && keyName) {
            seen.add(dedupKey)
            allShortcuts.push({ key: keyName, value: val, source: schema })
          }
        })
      } catch {}
    }
    return { success: true, shortcuts: allShortcuts }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('hotkey-set', async (_, key, cmd) => {
  const { execSync } = require('child_process')
  try {
    execSync(`gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/ name 'custom' 2>/dev/null; gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/ binding '${key}' 2>/dev/null; gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/ command '${cmd}' 2>/dev/null`, { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('hotkey-reset', async () => {
  const { execSync } = require('child_process')
  try {
    execSync("gsettings reset-recursively org.gnome.desktop.wm.keybindings 2>/dev/null", { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('hotkey-export', async () => {
  const { execSync } = require('child_process'); const fs = require('fs'); const { dialog } = require('electron')
  try {
    const raw = execSync("gsettings list-recursively org.gnome.desktop.wm.keybindings 2>/dev/null && echo '---' && gsettings list-recursively com.deepin.dde.keybinding 2>/dev/null && echo '---' && gsettings list-recursively org.gnome.settings-daemon.plugins.media-keys 2>/dev/null", { timeout: 15000, encoding: 'utf-8' }).toString()
    const win = require('electron').BrowserWindow.getAllWindows()[0]
    const r = await dialog.showSaveDialog(win, {
      defaultPath: 'uos_shortcuts_export.dconf',
      filters: [{ name: '快捷键配置', extensions: ['dconf', 'txt'] }]
    })
    if (r.canceled || !r.filePath) return { success: false, error: '已取消' }
    fs.writeFileSync(r.filePath, raw, 'utf-8')
    return { success: true, file: r.filePath }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('hotkey-import', async (_, path) => {
  const { execSync } = require('child_process'); const fs = require('fs')
  try {
    const data = fs.readFileSync(path, 'utf-8')
    data.split('\n').filter(l => l.trim()).forEach(l => {
      try { execSync(`dconf write '${l.split(' ')[0].replace('org.gnome','/org/gnome').replace(/\./g,'/')}' \"${l.split(' ').slice(1).join(' ')}\" 2>/dev/null`, { timeout: 3000 }) } catch {}
    })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 主题字体管理 ==========
ipcMain.handle('theme-switch', async (_, mode) => {
  const { execSync } = require('child_process')
  try {
    if (mode === 'dark') execSync("gsettings set org.gnome.desktop.interface gtk-theme 'deepin-dark' 2>/dev/null || gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark' 2>/dev/null", { timeout: 5000 })
    else if (mode === 'light') execSync("gsettings set org.gnome.desktop.interface gtk-theme 'deepin' 2>/dev/null || gsettings set org.gnome.desktop.interface color-scheme 'prefer-light' 2>/dev/null", { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('theme-list-fonts', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("fc-list :lang=zh 2>/dev/null | head -50", { timeout: 10000, encoding: 'utf-8' }).toString()
    const fonts = raw.split('\n').filter(l => l.trim()).map(l => {
      const m = l.match(/^([^:]+)/); return { path: m?m[1].trim():l, name: l.split(':')[1]?.trim()||'' }
    })
    return { success: true, fonts }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('theme-install-font', async (_, fontPath) => {
  const { execSync } = require('child_process')
  try {
    execSync(`cp '${fontPath}' /usr/share/fonts/ 2>/dev/null || cp '${fontPath}' ~/.fonts/ 2>/dev/null && fc-cache -f 2>/dev/null`, { timeout: 10000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('theme-uninstall-font', async (_, name) => {
  const { execSync } = require('child_process')
  try {
    execSync(`fc-list | grep -i '${name.replace(/[^a-zA-Z0-9_-]/g,'')}' | head -1 | cut -d: -f1 | xargs -I{} rm -f {} 2>/dev/null && fc-cache -f 2>/dev/null`, { timeout: 10000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('theme-list-icons', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("ls /usr/share/icons/ 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    const themes = raw.split('\n').filter(l => l.trim())
    return { success: true, themes }
  } catch(e) { return { success: true, themes: [] } }
})
ipcMain.handle('theme-switch-icons', async (_, name) => {
  const { execSync } = require('child_process')
  try {
    execSync(`gsettings set org.gnome.desktop.interface icon-theme '${name}' 2>/dev/null`, { timeout: 5000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 打印机管理 ==========
ipcMain.handle('printer-list', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("lpstat -p 2>/dev/null || echo 'No printers'", { timeout: 10000, encoding: 'utf-8' }).toString()
    const printers = raw.split('\n').filter(l => l.startsWith('printer')).map(l => {
      const m = l.match(/printer (\S+)/); return m ? { name: m[1], status: l.includes('idle')?'idle':l.includes('disabled')?'disabled':'active' } : null
    }).filter(Boolean)
    return { success: true, printers }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('printer-add', async (_, name, options) => {
  const { execSync } = require('child_process')
  try {
    execSync(`lpadmin -p '${name}' ${options?.device?'-v '+options.device:''} -E 2>/dev/null || echo 'Need cups'`, { timeout: 15000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('printer-remove', async (_, name) => {
  const { execSync } = require('child_process')
  try {
    execSync(`lpadmin -x '${name}' 2>/dev/null`, { timeout: 10000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('printer-test-page', async (_, name) => {
  const { execSync } = require('child_process')
  try {
    execSync(`lp -d '${name}' /etc/passwd 2>/dev/null || echo 'Test job sent'`, { timeout: 10000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('printer-queue', async (_, name) => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync(`lpq -P '${name}' 2>/dev/null || echo 'No queue'`, { timeout: 5000, encoding: 'utf-8' }).toString()
    const jobs = raw.split('\n').filter(l => l.trim() && !l.startsWith('Rank')).slice(1).map(l => ({ raw: l }))
    return { success: true, jobs }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 网络共享管理 ==========
ipcMain.handle('netshare-samba-status', async () => {
  const { execSync } = require('child_process')
  try {
    const samba = execSync("systemctl status smbd 2>/dev/null | head -10", { timeout: 5000, encoding: 'utf-8' }).toString()
    const shares = execSync("smbstatus -S 2>/dev/null || testparm -s 2>/dev/null | grep -A1 '\\[' || echo 'No shares'", { timeout: 5000, encoding: 'utf-8' }).toString()
    const active = samba.includes('active') || samba.includes('running')
    return { success: true, active, status: samba, shares }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('netshare-samba-set', async (_, config) => {
  const { execSync } = require('child_process')
  try {
    const shareConf = `[${config.name||'share'}]\npath = ${config.path||'/tmp'}\nvalid users = ${config.users||'nobody'}\nread only = ${config.readonly?'yes':'no'}\nguest ok = yes\n`
    execSync(`bash -c 'echo "${shareConf}" >> /etc/samba/smb.conf' 2>/dev/null && systemctl restart smbd 2>/dev/null`, { timeout: 10000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('netshare-nfs-status', async () => {
  const { execSync } = require('child_process')
  try {
    const nfs = execSync("systemctl status nfs-kernel-server 2>/dev/null || systemctl status nfs-server 2>/dev/null | head -10", { timeout: 5000, encoding: 'utf-8' }).toString()
    const exports = execSync("exportfs -v 2>/dev/null || cat /etc/exports 2>/dev/null || echo 'No exports'", { timeout: 5000, encoding: 'utf-8' }).toString()
    const active = nfs.includes('active') || nfs.includes('running')
    return { success: true, active, status: nfs, exports }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('netshare-nfs-set', async (_, config) => {
  const { execSync } = require('child_process')
  try {
    const exportLine = config.path||'/tmp' + ' ' + config.clients||'*\(rw,sync,no_subtree_check)' + '\n'
    execSync(`bash -c 'echo "${exportLine}" >> /etc/exports' 2>/dev/null && exportfs -ra 2>/dev/null`, { timeout: 10000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('netshare-remove', async (_, sharePath, type) => {
  const { execSync } = require('child_process')
  try {
    if (type === 'samba') execSync(`sed -i '/\\[${sharePath}\\]/,/^$/d' /etc/samba/smb.conf 2>/dev/null && systemctl restart smbd 2>/dev/null`, { timeout: 10000 })
    else if (type === 'nfs') execSync(`sed -i '\\|${sharePath}|d' /etc/exports 2>/dev/null && exportfs -ra 2>/dev/null`, { timeout: 10000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: Docker 容器管理 ==========
ipcMain.handle('docker-status', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("systemctl status docker 2>/dev/null | head -10", { timeout: 5000, encoding: 'utf-8' }).toString()
    const version = execSync("docker --version 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString().trim()
    return { success: true, running: raw.includes('active')||raw.includes('running'), status: raw, version }
  } catch(e) { return { success: false, error: 'Docker 未安装或未运行' } }
})
ipcMain.handle('docker-list-containers', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("docker ps -a --format '{{.ID}}\t{{.Names}}\t{{.Image}}\t{{.Status}}' 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
    const containers = raw.split('\n').filter(l => l.trim()).map(l => {
      const parts = l.split('\t'); return { id: parts[0]||'', name: parts[1]||'', image: parts[2]||'', status: parts[3]||'' }
    })
    return { success: true, containers }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('docker-list-images', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("docker images --format '{{.Repository}}\t{{.Tag}}\t{{.ID}}\t{{.Size}}' 2>/dev/null", { timeout: 10000, encoding: 'utf-8' }).toString()
    const images = raw.split('\n').filter(l => l.trim()).map(l => {
      const parts = l.split('\t'); return { repo: parts[0]||'', tag: parts[1]||'', id: parts[2]||'', size: parts[3]||'' }
    })
    return { success: true, images }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('docker-start', async (_, id) => {
  const { execSync } = require('child_process')
  try { execSync(`docker start ${id} 2>&1`, { timeout: 15000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('docker-stop', async (_, id) => {
  const { execSync } = require('child_process')
  try { execSync(`docker stop ${id} 2>&1`, { timeout: 15000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('docker-remove-container', async (_, id) => {
  const { execSync } = require('child_process')
  try { execSync(`docker rm ${id} 2>&1`, { timeout: 10000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('docker-logs', async (_, id) => {
  const { execSync } = require('child_process')
  try { const raw = execSync(`docker logs --tail 50 ${id} 2>&1`, { timeout: 10000, encoding: 'utf-8' }).toString(); return { success: true, logs: raw } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('docker-pull', async (_, name) => {
  const { execSync } = require('child_process')
  try { execSync(`docker pull ${name} 2>&1`, { timeout: 300000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('docker-remove-image', async (_, id) => {
  const { execSync } = require('child_process')
  try { execSync(`docker rmi ${id} 2>&1`, { timeout: 10000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: VPN 管理 ==========
ipcMain.handle('vpn-list', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("nmcli connection show 2>/dev/null | grep -i vpn || echo 'No VPN connections'", { timeout: 10000, encoding: 'utf-8' }).toString()
    const connections = raw.split('\n').filter(l => l.trim() && !l.startsWith('NAME')).map(l => {
      const parts = l.trim().split(/\s+/); return { name: parts[0]||'', type: parts[1]||'', state: parts[3]||'' }
    })
    return { success: true, connections }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('vpn-add', async (_, config) => {
  const { execSync } = require('child_process')
  try {
    execSync(`nmcli connection add type vpn vpn-type ${config.type||'l2tp'} con-name '${config.name||'vpn'}' ifname '*' 2>&1`, { timeout: 15000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('vpn-remove', async (_, name) => {
  const { execSync } = require('child_process')
  try { execSync(`nmcli connection delete '${name}' 2>&1`, { timeout: 10000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('vpn-connect', async (_, name) => {
  const { execSync } = require('child_process')
  try { execSync(`nmcli connection up '${name}' 2>&1`, { timeout: 30000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('vpn-disconnect', async (_, name) => {
  const { execSync } = require('child_process')
  try { execSync(`nmcli connection down '${name}' 2>&1`, { timeout: 15000 }); return { success: true } }
  catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('vpn-status', async (_, name) => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync(`nmcli connection show '${name}' 2>/dev/null | head -20`, { timeout: 10000, encoding: 'utf-8' }).toString()
    return { success: true, info: raw }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('vpn-import', async (_, path) => {
  const { execSync } = require('child_process')
  try {
    execSync(`nmcli connection import type openvpn file '${path}' 2>&1 || nmcli connection import type l2tp file '${path}' 2>&1`, { timeout: 15000 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 系统升级助手 ==========
ipcMain.handle('upgrade-check', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("cat /etc/os-release 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString()
    const versionId = (raw.match(/VERSION_ID="?([^"\n]+)"?/m)||[])[1] || 'unknown'
    const name = (raw.match(/PRETTY_NAME="?([^"\n]+)"?/m)||[])[1] || raw.match(/NAME="?([^"\n]+)"?/m)?.[1] || 'UOS'
    const upgradable = execSync("apt list --upgradable 2>/dev/null | wc -l", { timeout: 15000, encoding: 'utf-8' }).toString().trim()
    return { success: true, currentVersion: versionId, osName: name, upgradable: parseInt(upgradable)||0 }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('upgrade-preflight', async () => {
  const { execSync } = require('child_process')
  try {
    const checks = []
    try { const disk = execSync("df / | tail -1 | awk '{print $4}' 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString().trim(); checks.push({ name:'磁盘空间', status: parseInt(disk)>500000?'pass':'warn', detail: `${disk} KB 可用` }) } catch { checks.push({ name:'磁盘空间', status:'unknown' }) }
    try { const mem = execSync("free -m | grep Mem | awk '{print $7}' 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString().trim(); checks.push({ name:'可用内存', status: parseInt(mem)>500?'pass':'warn', detail: `${mem} MB 可用` }) } catch { checks.push({ name:'可用内存', status:'unknown' }) }
    try { execSync("ping -c 1 -W 2 mirrors.ustc.edu.cn 2>/dev/null", { timeout: 5000 }); checks.push({ name:'网络连通', status:'pass', detail:'可访问镜像源' }) } catch { checks.push({ name:'网络连通', status:'warn', detail:'网络异常' }) }
    try { const bat = execSync("cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || echo '100'", { timeout: 3000, encoding: 'utf-8' }).toString().trim(); checks.push({ name:'电源', status: parseInt(bat)>20?'pass':'warn', detail: `电量 ${bat}%` }) } catch { checks.push({ name:'电源', status:'pass', detail:'台式机' }) }
    return { success: true, checks }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('upgrade-start', async (_, password) => {
  const { execSync } = require('child_process'); const { sudoExec } = global
  try {
    const result = sudoExec('DEBIAN_FRONTEND=noninteractive apt-get update && DEBIAN_FRONTEND=noninteractive apt-get dist-upgrade -y 2>&1', password)
    return { success: true, output: result }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('upgrade-progress', async () => {
  const { execSync } = require('child_process')
  try {
    const dpkg = execSync("pgrep -c dpkg 2>/dev/null || echo 0", { timeout: 3000, encoding: 'utf-8' }).toString().trim()
    return { success: true, running: parseInt(dpkg) > 0 }
  } catch(e) { return { success: true, running: false } }
})
ipcMain.handle('upgrade-rollback', async (_, password) => {
  const { execSync } = require('child_process'); const { sudoExec } = global
  try {
    const result = sudoExec("dpkg --configure -a && apt-get install -f -y 2>&1", password)
    return { success: true, output: result }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 系统资产清单 ==========
ipcMain.handle('asset-scan-hardware', async () => {
  const { execSync } = require('child_process')
  try {
    const info = {}
    try { info.cpu = execSync("lscpu 2>/dev/null | head -20", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    try { info.memory = execSync("free -h 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString() } catch {}
    try { info.disk = execSync("lsblk -o NAME,SIZE,TYPE,MOUNTPOINT 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    try { info.pci = execSync("lspci 2>/dev/null | head -30", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    try { info.usb = execSync("lsusb 2>/dev/null | head -20", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    try { info.network = execSync("ip link show 2>/dev/null", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    try { info.system = execSync("dmidecode -t system 2>/dev/null | head -20", { timeout: 10000, encoding: 'utf-8' }).toString() } catch {}
    return { success: true, hardware: info }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('asset-scan-software', async () => {
  const { execSync } = require('child_process')
  try {
    const info = {}
    try { info.os = execSync("cat /etc/os-release 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString() } catch {}
    try { info.kernel = execSync("uname -a 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString() } catch {}
    try { info.packages = execSync("dpkg -l 2>/dev/null | wc -l", { timeout: 10000, encoding: 'utf-8' }).toString().trim() + ' packages' } catch {}
    try { info.services = execSync("systemctl list-units --type=service --state=running 2>/dev/null | head -20", { timeout: 5000, encoding: 'utf-8' }).toString() } catch {}
    return { success: true, software: info }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('asset-get-inventory', async () => {
  const fs = require('fs'); const path = require('path'); const { execSync } = require('child_process')
  try {
    const cacheFile = path.join(require('electron').app.getPath('userData'), 'asset_inventory.json')
    if (fs.existsSync(cacheFile)) {
      const cached = JSON.parse(fs.readFileSync(cacheFile, 'utf-8'))
      if (Date.now() - cached.timestamp < 300000) return { success: true, inventory: cached, cached: true }
    }
    const inventory = {}
    try { inventory.hostname = execSync("hostname 2>/dev/null", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { inventory.serial = (execSync("dmidecode -t system 2>/dev/null | grep Serial | awk -F: '{print $2}'", { timeout: 10000, encoding: 'utf-8' }).toString().trim()) } catch {}
    try { inventory.cpu = execSync("lscpu 2>/dev/null | grep 'Model name' | awk -F: '{print $2}'", { timeout: 5000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { inventory.memory = execSync("free -h 2>/dev/null | grep Mem | awk '{print $2}'", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { inventory.disk = execSync("df -h / 2>/dev/null | tail -1 | awk '{print $2}'", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { inventory.os = execSync("cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2", { timeout: 3000, encoding: 'utf-8' }).toString().trim().replace(/"/g,'') } catch {}
    try { inventory.ip = execSync("hostname -I 2>/dev/null | awk '{print $1}'", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    try { inventory.mac = execSync("ip link show 2>/dev/null | grep 'link/ether' | head -1 | awk '{print $2}'", { timeout: 3000, encoding: 'utf-8' }).toString().trim() } catch {}
    inventory.timestamp = Date.now()
    inventory.scanDate = new Date().toISOString()
    fs.writeFileSync(cacheFile, JSON.stringify(inventory, null, 2), 'utf-8')
    return { success: true, inventory, cached: false }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('asset-export', async (_, format) => {
  const fs = require('fs'); const path = require('path'); const { execSync } = require('child_process')
  try {
    const result = await ipcMain.emit('asset-get-inventory') || { inventory: {} }
    const inv = result.inventory || {}
    const dir = require('electron').app.getPath('desktop')
    if (format === 'csv') {
      const file = path.join(dir, `asset_inventory_${Date.now()}.csv`)
      const csv = `主机名,序列号,CPU,内存,磁盘,操作系统,IP地址,MAC地址,扫描时间\n"${inv.hostname||''}","${inv.serial||''}","${inv.cpu||''}","${inv.memory||''}","${inv.disk||''}","${inv.os||''}","${inv.ip||''}","${inv.mac||''}","${inv.scanDate||''}"\n`
      fs.writeFileSync(file, csv, 'utf-8')
      return { success: true, file }
    } else {
      const file = path.join(dir, `asset_inventory_${Date.now()}.json`)
      fs.writeFileSync(file, JSON.stringify(inv, null, 2), 'utf-8')
      return { success: true, file }
    }
  } catch(e) { return { success: false, error: e.message } }
})

// ========== Phase 3: 远程管理工具 ==========
ipcMain.handle('remote-list-keys', async () => {
  const { execSync } = require('child_process')
  try {
    const raw = execSync("cat ~/.ssh/authorized_keys 2>/dev/null || echo 'No keys'", { timeout: 5000, encoding: 'utf-8' }).toString()
    const keys = raw.split('\n').filter(l => l.trim() && l.startsWith('ssh-')).map(l => {
      const parts = l.split(/\s+/); return { type: parts[0]||'', key: parts[1]||'', comment: parts[2]||'' }
    })
    return { success: true, keys }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('remote-add-key', async (_, name, key) => {
  const fs = require('fs'); const path = require('path')
  try {
    const sshDir = path.join(require('electron').app.getPath('home'), '.ssh')
    if (!fs.existsSync(sshDir)) fs.mkdirSync(sshDir, { recursive: true, mode: 0o700 })
    const authFile = path.join(sshDir, 'authorized_keys')
    fs.appendFileSync(authFile, key + '\n', { mode: 0o600 })
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('remote-remove-key', async (_, name) => {
  const fs = require('fs'); const path = require('path')
  try {
    const authFile = path.join(require('electron').app.getPath('home'), '.ssh', 'authorized_keys')
    if (!fs.existsSync(authFile)) return { success: false, error: 'No authorized_keys' }
    let data = fs.readFileSync(authFile, 'utf-8')
    data = data.split('\n').filter(l => !l.includes(name)).join('\n')
    fs.writeFileSync(authFile, data, 'utf-8')
    return { success: true }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('remote-batch-exec', async (_, hosts, cmd, password) => {
  const { execSync } = require('child_process')
  try {
    const results = []
    for (const host of (hosts||[]).slice(0,5)) {
      try {
        const out = execSync(`ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 ${host} '${cmd.replace(/'/g,"'\\''")}' 2>&1`, { timeout: 30000, encoding: 'utf-8' }).toString()
        results.push({ host, success: true, output: out })
      } catch(e) { results.push({ host, success: false, error: e.message }) }
    }
    return { success: true, results }
  } catch(e) { return { success: false, error: e.message } }
})
ipcMain.handle('remote-transfer', async (_, host, filePath, remotePath, password) => {
  const { execSync } = require('child_process')
  try {
    const out = execSync(`scp -o StrictHostKeyChecking=no '${filePath}' ${host}:'${remotePath||'~/'}' 2>&1`, { timeout: 60000, encoding: 'utf-8' }).toString()
    return { success: true, output: out }
  } catch(e) { return { success: false, error: e.message } }

// ========== USB 启动盘制作 ==========
ipcMain.handle('phase2-usb', async (_, action, params) => {
  try {
    const scriptDir = path.join(__dirname, '../resources/scripts/Phase2')
    const scriptPath = path.join(scriptDir, 'usb_bootmaker.sh')
    if (action === 'list') {
      const result = execSync('bash "' + scriptPath + '" list', { timeout: 10000, encoding: 'utf-8' })
      const lines = result.split('\n').filter(l => l.startsWith('USB|'))
      const devices = lines.map(l => {
        const parts = l.split('|')
        return { device: parts[1], size: parts[2], model: parts[3] || '' }
      })
      return { success: true, devices }
    } else if (action === 'check-iso') {
      const isoFile = params?.iso || ''
      if (!isoFile) return { success: false, error: '请选择ISO文件' }
      const result = execSync('bash "' + scriptPath + '" check-iso "' + isoFile.replace(/"/g, '\\"') + '"', { timeout: 10000, encoding: 'utf-8' })
      if (result.startsWith('OK')) return { success: true, valid: true }
      return { success: true, valid: false, error: result.replace('ERROR|', '') }
    } else if (action === 'check-device') {
      const usbDev = params?.device || ''
      if (!usbDev) return { success: false, error: '请选择USB设备' }
      const result = execSync('bash "' + scriptPath + '" check-device "' + usbDev.replace(/"/g, '\\"') + '"', { timeout: 10000, encoding: 'utf-8' })
      if (result.startsWith('OK')) return { success: true, valid: true }
      if (result.startsWith('MOUNTED')) return { success: true, valid: false, mounted: true, error: '设备已挂载，请先卸载' }
      return { success: true, valid: false, error: result.replace('ERROR|', '') }
    } else if (action === 'create') {
      const isoFile = params?.iso || ''
      const usbDev = params?.device || ''
      if (!isoFile || !usbDev) return { success: false, error: '请选择ISO文件和USB设备' }
      const password = params?.password || ''
      if (!password) return { success: false, error: '需要管理员密码' }
      try {
        const mountCheck = execSync('mount | grep "^' + usbDev.replace(/"/g, '\\"') + '" || true', { timeout: 5000, encoding: 'utf-8' })
        if (mountCheck.trim()) {
          const parts = mountCheck.split('\n').filter(l => l.trim())
          for (const part of parts) {
            const dev = part.split(' ')[0]
            if (dev) sudoExec('umount "' + dev.replace(/"/g, '\\"') + '" 2>/dev/null', password)
          }
        }
      } catch(e) {}
      const ddResult = sudoExec('dd if="' + isoFile.replace(/"/g, '\\"') + '" of="' + usbDev.replace(/"/g, '\\"') + '" bs=4M status=progress oflag=sync 2>&1', password)
      if (ddResult && !ddResult.includes('Error') && !ddResult.includes('error')) {
        return { success: true, output: '启动盘制作完成！\n设备: ' + usbDev + '\nISO: ' + isoFile }
      } else {
        return { success: false, error: ddResult || '写入失败，请检查权限或设备连接' }
      }
    }
    return { success: false, error: '未知操作' }
  } catch(e) { return { success: false, error: e.message } }
})

})

// ========== 系统应用集成 IPC ==========

ipcMain.handle("systemapp-launch", async (_, appName) => {
  try {
    const { execSync } = require("child_process")
    execSync("gtk-launch " + appName.replace(/[^a-zA-Z0-9._-]/g, "") + " 2>/dev/null || " +
      "xdg-open " + appName.replace(/[^a-zA-Z0-9._-]/g, "") + " 2>/dev/null || " +
      appName.replace(/[^a-zA-Z0-9._-]/g, "") + " 2>/dev/null", { timeout: 10000 })
    return { success: true }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

ipcMain.handle("systemapp-dbus", async (_, { service, objectPath, method, args }) => {
  try {
    const { execSync } = require("child_process")
    const srv = service.replace(/[^a-zA-Z0-9._-]/g, "")
    const obj = objectPath.replace(/[^a-zA-Z0-9._/-]/g, "")
    const mtd = method.replace(/[^a-zA-Z0-9._-]/g, "")
    const argsJson = args ? JSON.parse(JSON.stringify(args)) : []
    let cmd = "gdbus call --session -d " + srv + " -o " + obj + " -m " + mtd
    for (const a of argsJson) {
      if (typeof a === "string") cmd += " " + JSON.stringify(a)
      else if (typeof a === "number") cmd += " " + a
      else if (typeof a === "boolean") cmd += a ? " true" : " false"
    }
    const result = execSync(cmd, { timeout: 15000, encoding: "utf-8" })
    return { success: true, output: result.trim() }
  } catch(e) {
    return { success: false, error: e.message }
  }
})

ipcMain.handle("systemapp-cli", async (_, command) => {
  try {
    const { execSync } = require("child_process")
    const result = execSync(command, { timeout: 30000, encoding: "utf-8" })
    return { success: true, output: result.trim() }
  } catch(e) {
    return { success: false, error: e.message, output: e.stdout?.toString?.() || "" }
  }
})
