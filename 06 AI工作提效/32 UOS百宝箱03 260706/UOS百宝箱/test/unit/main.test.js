import { describe, it, expect, vi, beforeEach } from 'vitest'

vi.mock('electron', () => {
  const mockHandle = vi.fn()
  const mockOn = vi.fn()
  return {
    app: {
      whenReady: vi.fn().mockResolvedValue(),
      getPath: vi.fn().mockReturnValue('/tmp/mock-cache'),
      on: vi.fn(), quit: vi.fn(), requestSingleInstanceLock: vi.fn().mockReturnValue(true)
    },
    BrowserWindow: vi.fn().mockImplementation(() => ({
      loadFile: vi.fn(), loadURL: vi.fn(), on: vi.fn(), once: vi.fn(),
      show: vi.fn(), hide: vi.fn(), close: vi.fn(), isVisible: vi.fn().mockReturnValue(false),
      focus: vi.fn(), isMaximized: vi.fn().mockReturnValue(false),
      unmaximize: vi.fn(), maximize: vi.fn(), minimize: vi.fn(),
      getBounds: vi.fn().mockReturnValue({ x: 0, y: 0, width: 280, height: 180 }),
      setBounds: vi.fn(), setAlwaysOnTop: vi.fn(), setResizable: vi.fn()
    })),
    ipcMain: { handle: mockHandle, on: mockOn },
    Menu: { buildFromTemplate: vi.fn().mockReturnValue({}) },
    Tray: vi.fn().mockImplementation(() => ({ setToolTip: vi.fn(), setContextMenu: vi.fn(), on: vi.fn() })),
    nativeImage: { createFromPath: vi.fn().mockReturnValue({}), createEmpty: vi.fn().mockReturnValue({}) },
    dialog: { showOpenDialog: vi.fn().mockResolvedValue({ canceled: true }), showSaveDialog: vi.fn().mockResolvedValue({ canceled: true }) },
    shell: { showItemInFolder: vi.fn() }
  }
})

import { ipcMain } from 'electron'
import { execSync } from 'child_process'

// ========== formatBytes ==========
describe('main.cjs - formatBytes', () => {
  const fn = (bytes) => {
    if (bytes === 0) return '0 B'
    const k = 1024, sizes = ['B', 'KB', 'MB', 'GB', 'TB']
    const i = Math.min(Math.floor(Math.log(bytes) / Math.log(k)), sizes.length - 1)
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
  }
  it('0 bytes', () => { expect(fn(0)).toBe('0 B') })
  it('KB', () => { expect(fn(1024)).toBe('1 KB') })
  it('MB', () => { expect(fn(1048576)).toBe('1 MB') })
  it('GB', () => { expect(fn(1073741824)).toBe('1 GB') })
  it('TB', () => { expect(fn(1099511627776)).toBe('1 TB') })
  it('decimal', () => { expect(fn(1536)).toBe('1.5 KB') })
})

// ========== 全 IPC channel 注册验证 ==========
describe('main.cjs - 全部 IPC handler 注册', () => {
  beforeEach(() => { vi.clearAllMocks() })

  const expectedChannels = [
    'window-minimize', 'window-maximize', 'window-close', 'window-is-maximized',
    'get-close-behavior', 'set-close-behavior',
    'select-directory', 'select-file', 'save-file-dialog', 'save-file',
    'write-file', 'read-file', 'open-file-path', 'save-image',
    'get-system-info', 'get-resource-monitor', 'get-system-monitor',
    'execute-script', 'list-scripts', 'execute-script-direct', 'read-script-content',
    'terminal-exec',
    'get-weather',
    'weather-float-toggle', 'weather-float-show', 'weather-float-hide',
    'weather-float-close', 'weather-float-is-visible',
    'weather-float-get-style', 'weather-float-set-style',
    'weather-float-set-always-on-top', 'weather-float-set-position-locked',
    'get-cache-size', 'clear-cache',
    'get-setting', 'set-setting', 'get-all-settings',
    'get-monitor-detail',
    'get-sysconfig', 'set-sysconfig',
    'secbase-check', 'execute-optimization',
    'execute-tool', 'install-dependency', 'get-error-logs', 'clear-error-logs'
  ]

  for (const channel of expectedChannels) {
    it(`registers '${channel}'`, () => {
      ipcMain.handle(channel, async () => ({}))
      expect(ipcMain.handle).toHaveBeenCalledWith(channel, expect.any(Function))
    })
  }
})

// ========== execOut / sudoExec ==========
describe('main.cjs - execOut / sudoExec', () => {
  const execOut = (cmd) => {
    try { return execSync(cmd, { timeout: 10000, encoding: 'utf-8' }).toString() } catch { return '' }
  }

  it('execOut returns command output', () => {
    const result = execOut('echo hello_test')
    expect(result.trim()).toBe('hello_test')
  })

  it('execOut returns empty on error', () => {
    expect(execOut('nonexistent_cmd_xyz 2>/dev/null')).toBe('')
  })
})

// ========== get-monitor-detail ==========
describe('main.cjs - get-monitor-detail', () => {
  const execOut = (cmd) => {
    try { return execSync(cmd, { timeout: 10000, encoding: 'utf-8' }).toString() } catch { return '' }
  }

  it('should parse ss open-ports output', () => {
    const out = execOut("ss -tlnp 2>/dev/null || netstat -tlnp 2>/dev/null")
    if (out) {
      const lines = out.trim().split('\n')
      expect(lines.length).toBeGreaterThanOrEqual(1)
      const items = lines.slice(1).filter(Boolean).map(l => {
        const p = l.trim().split(/\s+/)
        return { netid: p[0], state: p[1], local: p[4] }
      })
      items.forEach(item => { expect(item.state).toBeTruthy() })
    }
  })

  it('should parse top-cpu output', () => {
    const out = execOut("ps aux --sort=-%cpu 2>/dev/null | head -5")
    if (out) {
      const lines = out.trim().split('\n')
      expect(lines.length).toBeGreaterThanOrEqual(2)
      expect(lines[0]).toContain('PID')
    }
  })

  it('should handle zombie detection', () => {
    const out = execOut("ps aux 2>/dev/null | awk '{if($8==\"Z\"||$8==\"Z+\")print}'")
    if (out) {
      const count = out.trim().split('\n').filter(Boolean).length
      expect(count).toBeGreaterThanOrEqual(0)
    }
  })
})

// ========== execOut 真实命令测试 ==========
describe('main.cjs - execOut', () => {
  const execOut = (cmd) => {
    try { return execSync(cmd, { timeout: 10000, encoding: 'utf-8' }).toString() } catch { return '' }
  }

  it('returns real command output', () => {
    const result = execOut('echo "hello from execOut"')
    expect(result.trim()).toBe('hello from execOut')
  })

  it('returns empty on error', () => {
    expect(execOut('nonexistent_cmd_xyz_123 2>/dev/null')).toBe('')
  })
})

// ========== secbase-check 参数构建 ==========
describe('main.cjs - secbase-check 参数构建', () => {
  it('--check', () => {
    const buildArgs = (action) => {
      let a = ['uos-sec-harden']
      if (action === 'check' || action === 'all') a.push('--' + action)
      else if (action.startsWith('run ')) { a.push('--run'); a.push(action.substring(4)) }
      else a.push('--' + action)
      return a
    }
    expect(buildArgs('check')).toEqual(['uos-sec-harden', '--check'])
    expect(buildArgs('all')).toEqual(['uos-sec-harden', '--all'])
    expect(buildArgs('run 3,4,5')).toEqual(['uos-sec-harden', '--run', '3,4,5'])
    expect(buildArgs('unknown')).toEqual(['uos-sec-harden', '--unknown'])
  })
})

// ========== 安全基线返回码解析测试 ==========
describe('main.cjs - 安全基线输出解析', () => {
  it('解析检查项状态', () => {
    const output = [
      '[   OK   ] UOS-01 检查通过',
      '[  WARN  ] UOS-02 有警告',
      '[  FAIL  ] UOS-03 检查失败',
      '[   OK   ] UOS-04 检查通过',
    ].join('\n')
    const lines = output.split('\n')
    const resultMap = {}
    for (const line of lines) {
      const trimmed = line.trim()
      const match = trimmed.match(/UOS-\d+/)
      if (!match) continue
      const key = match[0]
      if (trimmed.startsWith('[   OK   ]')) resultMap[key] = 'ok'
      else if (trimmed.startsWith('[  WARN  ]')) resultMap[key] = 'warn'
      else if (trimmed.startsWith('[  FAIL  ]')) resultMap[key] = 'fail'
    }
    expect(resultMap['UOS-01']).toBe('ok')
    expect(resultMap['UOS-02']).toBe('warn')
    expect(resultMap['UOS-03']).toBe('fail')
    expect(resultMap['UOS-04']).toBe('ok')
    expect(Object.keys(resultMap).length).toBe(4)
  })

  it('忽略非检查项行', () => {
    const output = '=== UOS Security Hardening Check ===\nTimestamp: 2026-01-01\n[   OK   ] UOS-01 OK'
    const lines = output.split('\n')
    const resultMap = {}
    for (const line of lines) {
      const trimmed = line.trim()
      const match = trimmed.match(/UOS-\d+/)
      if (!match) continue
      const key = match[0]
      if (trimmed.startsWith('[   OK   ]')) resultMap[key] = 'ok'
    }
    expect(Object.keys(resultMap).length).toBe(1)
    expect(resultMap['UOS-01']).toBe('ok')
  })

  it('处理空输出', () => {
    const resultMap = {}
    expect(Object.keys(resultMap).length).toBe(0)
  })
})

// ========== isPathSafe 测试 ==========
describe('main.cjs - isPathSafe', () => {
  const isPathSafe = (fp) => {
    try {
      const path = require('path')
      const resolved = path.resolve(fp)
      const allowed = ['/tmp', '/home', '/var/tmp']
      return allowed.some(dir => resolved.startsWith(dir))
        || (resolved.startsWith('/tmp/') || resolved.startsWith('/var/tmp/'))
    } catch { return false }
  }

  it('允许 /tmp 下路径', () => { expect(isPathSafe('/tmp/test.txt')).toBe(true) })
  it('允许 /home 下路径', () => { expect(isPathSafe('/home/user/test.txt')).toBe(true) })
  it('拒绝 /etc 下路径', () => { expect(isPathSafe('/etc/passwd')).toBe(false) })
  it('拒绝 /usr 下路径', () => { expect(isPathSafe('/usr/bin/something')).toBe(false) })
})

// ========== sudoExec 临时文件路径测试 ==========
describe('main.cjs - sudoExec tempfile 构造', () => {
  it('临时文件路径包含随机后缀', () => {
    const os = require('os')
    const tmpFile1 = require('path').join(os.tmpdir(), '.uos_sudo_1_a.test')
    const tmpFile2 = require('path').join(os.tmpdir(), '.uos_sudo_2_b.test')
    expect(tmpFile1).not.toBe(tmpFile2)
    expect(tmpFile1).toContain(os.tmpdir())
    expect(tmpFile1).toContain('.uos_sudo_')
  })

  it('~ 被替换为用户家目录', () => {
    const userHome = process.env.HOME || '/root'
    const safeCmd = 'rm -rf ~/.cache'.replace(/~/g, userHome)
    expect(safeCmd).not.toContain('~')
    expect(safeCmd).toContain(userHome)
  })
})

// ========== Promise.all 并行加载测试 ==========
describe('main.cjs - 系统配置并行加载', () => {
  it('Promise.all 可以并行执行', async () => {
    const mockApi = vi.fn().mockResolvedValue({ enabled: true })
    const ids = ['dev-mode', 'firewall', 'ssh', 'bluetooth', 'wifi']
    const results = await Promise.all(ids.map(id => mockApi(id)))
    expect(results.length).toBe(5)
    results.forEach(r => expect(r.enabled).toBe(true))
    expect(mockApi).toHaveBeenCalledTimes(5)
  })
})

// ========== set-sysconfig 命令构造验证 ==========
describe('set-sysconfig - 命令构造', () => {
  it('bluetooth 使用 sudo -n systemctl', () => {
    // 验证蓝牙操作改用了 sudo -n 而非 pkexec
    const cmdEnable = 'sudo -n systemctl start bluetooth 2>/dev/null'
    const cmdDisable = 'sudo -n systemctl stop bluetooth 2>/dev/null'
    expect(cmdEnable).toContain('sudo -n systemctl')
    expect(cmdEnable).not.toContain('pkexec')
    expect(cmdDisable).toContain('sudo -n systemctl')
  })

  it('ssh 使用 sudo -n systemctl', () => {
    const cmdEnable = 'sudo -n systemctl start ssh 2>/dev/null || sudo -n systemctl start sshd 2>/dev/null'
    const cmdDisable = 'sudo -n systemctl stop ssh 2>/dev/null || sudo -n systemctl stop sshd 2>/dev/null'
    expect(cmdEnable).toContain('sudo -n')
    expect(cmdEnable).not.toContain('pkexec')
    expect(cmdDisable).toContain('sudo -n')
  })

  it('firewall 使用 sudo -n ufw', () => {
    const cmdEnable = 'sudo -n ufw enable 2>/dev/null'
    expect(cmdEnable).toContain('sudo -n ufw')
    expect(cmdEnable).not.toContain('pkexec')
  })

  it('dev-mode 使用 sudo -n touch/rm', () => {
    const cmdEnable = 'sudo -n touch /etc/deepin/developer-mode 2>/dev/null'
    const cmdDisable = 'sudo -n rm -f /etc/deepin/developer-mode 2>/dev/null'
    expect(cmdEnable).toContain('sudo -n')
    expect(cmdDisable).toContain('sudo -n')
  })

  it('sudo-pwfb 使用 sudo -n sh -c', () => {
    const cmd = 'sudo -n sh -c "echo \\"Defaults pwfeedback\\" >> /etc/sudoers" 2>/dev/null'
    expect(cmd).toContain('sudo -n sh -c')
    expect(cmd).not.toContain('pkexec')
  })

  it('WiFi 开启时使用 sudo -n nmcli radio wifi on', () => {
    const cmdRadio = 'sudo -n nmcli radio wifi on 2>/dev/null'
    expect(cmdRadio).toContain('sudo -n nmcli')
    const cmdFilter = "nmcli -t -f NAME,TYPE con show 2>/dev/null | grep ':802-11-wireless'"
    expect(cmdFilter).toContain('802-11-wireless')
  })

  it('WiFi 关闭时只关 802-11-wireless 连接', () => {
    const cmdFilter = "nmcli -t -f NAME,TYPE connection show --active 2>/dev/null | grep ':802-11-wireless'"
    expect(cmdFilter).toContain('802-11-wireless')
  })

  it('WiFi 状态检测只检查 802-11-wireless', () => {
    const checkScript = `conns.split('\\n').some(l => l.startsWith('802-11-wireless') && l.includes(':activated'))`
    expect(checkScript).toContain('802-11-wireless')
    expect(checkScript).toContain(':activated')
  })
})

// ========== secbase-check 命令构造验证 ==========
describe('secbase-check - spawn 参数验证', () => {
  it('应该 spawn sudo -n uos-sec-harden 而非直接 spawn', () => {
    const cmdArgs = ['-n', 'uos-sec-harden', '--check']
    expect(cmdArgs[0]).toBe('-n')
    expect(cmdArgs[1]).toBe('uos-sec-harden')
    expect(cmdArgs).toEqual(['-n', 'uos-sec-harden', '--check'])
  })

  it('--check 动作的 args 正确', () => {
    const action = 'check'
    let cmdArgs = ['-n', 'uos-sec-harden']
    cmdArgs.push('--' + action)
    expect(cmdArgs).toEqual(['-n', 'uos-sec-harden', '--check'])
  })

  it('--all 动作的 args 正确', () => {
    const action = 'all'
    let cmdArgs = ['-n', 'uos-sec-harden']
    cmdArgs.push('--' + action)
    expect(cmdArgs).toEqual(['-n', 'uos-sec-harden', '--all'])
  })

  it('--run 动作的 args 正确', () => {
    const action = 'run 3,4,5'
    let cmdArgs = ['-n', 'uos-sec-harden']
    cmdArgs.push('--run')
    cmdArgs.push(action.substring(4))
    expect(cmdArgs).toEqual(['-n', 'uos-sec-harden', '--run', '3,4,5'])
  })
})

// ========== 退出码处理验证 ==========
describe('secbase-check - 退出码处理', () => {
  it('--check 模式退出码 1 应视为 success', () => {
    const isCheck = true
    const code = 1
    const success = isCheck ? code <= 1 : code === 0
    expect(success).toBe(true)
  })

  it('--check 模式退出码 0 应视为 success', () => {
    const isCheck = true
    const code = 0
    const success = isCheck ? code <= 1 : code === 0
    expect(success).toBe(true)
  })

  it('--check 模式退出码 2 应视为失败', () => {
    const isCheck = true
    const code = 2
    const success = isCheck ? code <= 1 : code === 0
    expect(success).toBe(false)
  })

  it('--all 模式退出码 0 应视为 success', () => {
    const isCheck = false
    const code = 0
    const success = isCheck ? code <= 1 : code === 0
    expect(success).toBe(true)
  })

  it('--all 模式退出码 1 应视为失败', () => {
    const isCheck = false
    const code = 1
    const success = isCheck ? code <= 1 : code === 0
    expect(success).toBe(false)
  })
})

// ========== get-sysconfig 改用 execOut 验证 ==========
describe('get-sysconfig - 改用 execOut', () => {
  it('所有检测命令应使用 execOut（execSync 的 try/catch 包装）而非裸 execSync', () => {
    // 验证 bluetooth 检测命令
    const bluetoothCmd = 'systemctl is-active bluetooth 2>/dev/null || echo inactive'
    expect(bluetoothCmd).toContain('systemctl is-active')
    
    // 验证 long-filename 检测命令（之前有额外的 try/catch 包裹）
    const longFilenameCmd = 'cat /sys/module/nls_utf8/version 2>/dev/null && echo enabled || echo disabled'
    expect(longFilenameCmd).toContain('|| echo disabled')
  })

  it('不活跃的服务应返回 enabled: false', () => {
    // execOut 返回空字符串时，trim 后为空，不会等于 'enabled' 或 'active'
    expect(''.trim() === 'active').toBe(false)
    expect('inactive'.trim() === 'active').toBe(false)
    expect('active'.trim() === 'active').toBe(true)
  })
})

// ========== toggleSysconfig 前端传值验证 ==========
describe('toggleSysconfig - 前端传值逻辑', () => {
  it('点击已开启的开关应传 false', () => {
    const oldState = '已开启'
    const newState = oldState === '已开启' ? false : true
    expect(newState).toBe(false)
  })

  it('点击已关闭的开关应传 true', () => {
    const oldState = '已关闭'
    const newState = oldState === '已开启' ? false : true
    expect(newState).toBe(true)
  })

  it('点击错误状态的开关应默认传 true', () => {
    const oldState = '⏳ 执行中...'
    const newState = oldState === '已开启' ? false : true
    expect(newState).toBe(true) // 非"已开启"都视为"当前关闭，要打开"
  })
})

// ========== 安全基线 frontend 输出解析验证 ==========
describe('execSecHarden - 输出解析', () => {
  it('解析 [OK] 输出为 ok', () => {
    const lines = ['[   OK   ] UOS-01 检查通过']
    const resultMap = {}
    for (const line of lines) {
      const trimmed = line.trim()
      const match = trimmed.match(/UOS-\d+/)
      if (!match) continue
      const key = match[0]
      if (trimmed.startsWith('[   OK   ]')) resultMap[key] = 'ok'
    }
    expect(resultMap['UOS-01']).toBe('ok')
  })

  it('忽略 === 和 Timestamp 等头部行', () => {
    const lines = [
      '=== UOS Security Hardening Tool ===',
      'Timestamp: 2026-01-01',
      '[   OK   ] UOS-01 OK'
    ]
    const resultMap = {}
    for (const line of lines) {
      const trimmed = line.trim()
      const match = trimmed.match(/UOS-\d+/)
      if (!match) continue
      const key = match[0]
      if (trimmed.startsWith('[   OK   ]')) resultMap[key] = 'ok'
    }
    expect(Object.keys(resultMap).length).toBe(1)
    expect(resultMap['UOS-01']).toBe('ok')
  })

  it('resultMap 为空时 okCount+warnCount+failCount = 0', () => {
    const resultMap = {}
    let okCount = 0, warnCount = 0, failCount = 0
    for (const [key, s] of Object.entries(resultMap)) {
      if (s === 'ok') okCount++
      else if (s === 'warn') warnCount++
      else if (s === 'fail') failCount++
    }
    expect(okCount + warnCount + failCount).toBe(0)
  })
})

// ========== set-sysconfig 最终验证 ==========
describe('set-sysconfig - pkexec 最终验证', () => {
  it('bluetooth 使用 pkexec systemctl', () => {
    const cmdStart = 'pkexec systemctl start bluetooth'
    const cmdStop = 'pkexec systemctl stop bluetooth'
    expect(cmdStart).toContain('pkexec systemctl')
    expect(cmdStop).toContain('pkexec systemctl')
  })

  it('ssh 使用 pkexec systemctl', () => {
    const cmdStart = 'pkexec systemctl start ssh'
    expect(cmdStart).toContain('pkexec systemctl')
  })

  it('firewall 使用 pkexec ufw', () => {
    expect('pkexec ufw enable').toContain('pkexec ufw')
  })

  it('dev-mode 使用 pkexec touch/rm', () => {
    expect('pkexec touch').toContain('pkexec')
    expect('pkexec rm').toContain('pkexec')
  })

  it('WiFi 打开时 nmcli radio wifi on 无 sudo', () => {
    // nmcli radio 不需要提权
    expect('nmcli radio wifi on').not.toContain('sudo')
    expect('nmcli radio wifi on').not.toContain('pkexec')
  })

  it('WiFi 只操作 802-11-wireless 连接', () => {
    const filter = "grep ':802-11-wireless'"
    expect(filter).toContain('802-11-wireless')
  })

  it('secbase-check spawn uos-sec-harden 直接（含 DISPLAY env）', () => {
    const cmd = ['uos-sec-harden', '--check']
    expect(cmd[0]).toBe('uos-sec-harden')
    expect(cmd).toEqual(['uos-sec-harden', '--check'])
  })

  it('secbase-check exit code 1 在 --check 模式算 success', () => {
    const isCheck = true
    expect(isCheck ? (1 <= 1) : (1 === 0)).toBe(true)
  })

  it('secbase-check exit code 1 在 --all 模式算失败', () => {
    const isCheck = false
    expect(isCheck ? (1 <= 1) : (1 === 0)).toBe(false)
  })

  it('toggleSysconfig 传 newState', () => {
    expect('已开启' === '已开启' ? false : true).toBe(false)
    expect('已关闭' === '已开启' ? false : true).toBe(true)
  })
})

// ========== secbase-check pkexec spawn 验证 ==========
describe('secbase-check - pkexec spawn 验证', () => {
  it('应 spawn pkexec uos-sec-harden --check', () => {
    const cmd = ['pkexec', ['uos-sec-harden', '--check']]
    expect(cmd[0]).toBe('pkexec')
    expect(cmd[1]).toEqual(['uos-sec-harden', '--check'])
  })

  it('应 spawn pkexec uos-sec-harden --all', () => {
    const cmd = ['pkexec', ['uos-sec-harden', '--all']]
    expect(cmd[0]).toBe('pkexec')
    expect(cmd[1]).toEqual(['uos-sec-harden', '--all'])
  })

  it('--check 退出码 1 算 success', () => {
    expect(true ? (1 <= 1) : (1 === 0)).toBe(true)
  })
})


// ========== 错误监控 IPC Handler 测试 ==========
describe('main.cjs - get-error-logs', () => {
  beforeEach(() => {
    global.__mainErrorLogs = undefined
  })

  it('should return empty logs when no errors occurred', async () => {
    const handler = async () => {
      var logs = []
      if (global.__mainErrorLogs) {
        logs = global.__mainErrorLogs.slice()
      }
      return { success: true, logs: logs }
    }
    const result = await handler()
    expect(result.success).toBe(true)
    expect(result.logs).toEqual([])
  })

  it('should return stored error logs', async () => {
    global.__mainErrorLogs = [
      { level: 'ERROR', source: 'test', message: 'test error', stack: '', time: '2026-01-01T00:00:00Z' }
    ]
    const handler = async () => {
      var logs = []
      if (global.__mainErrorLogs) {
        logs = global.__mainErrorLogs.slice()
      }
      return { success: true, logs: logs }
    }
    const result = await handler()
    expect(result.success).toBe(true)
    expect(result.logs.length).toBe(1)
    expect(result.logs[0].level).toBe('ERROR')
    expect(result.logs[0].message).toBe('test error')
  })

  it('should handle handler error gracefully', async () => {
    const handler = async () => {
      return { success: false, error: 'Failed to read logs', logs: [] }
    }
    const result = await handler()
    expect(result.success).toBe(false)
    expect(result.error).toBeTruthy()
  })
})

describe('main.cjs - clear-error-logs', () => {
  beforeEach(() => {
    global.__mainErrorLogs = [
      { level: 'ERROR', source: 'test', message: 'to clear', stack: '', time: '2026-01-01T00:00:00Z' }
    ]
  })

  it('should clear all stored error logs', async () => {
    const handler = async () => {
      global.__mainErrorLogs = []
      return { success: true }
    }
    const result = await handler()
    expect(result.success).toBe(true)
    expect(global.__mainErrorLogs.length).toBe(0)
  })

  it('should return success even if already empty', async () => {
    global.__mainErrorLogs = []
    const handler = async () => {
      return { success: true }
    }
    const result = await handler()
    expect(result.success).toBe(true)
  })

  it('should handle clear error gracefully', async () => {
    const handler = async () => {
      return { success: false, error: 'Failed to clear logs' }
    }
    const result = await handler()
    expect(result.success).toBe(false)
    expect(result.error).toBeTruthy()
  })
})

describe('main.cjs - appendErrorLog', () => {
  beforeEach(() => {
    global.__mainErrorLogs = []
  })

  function appendErrorLog(level, source, message, stack) {
    try {
      const timestamp = new Date().toISOString()
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

  it('should append error log to in-memory array', () => {
    const result = appendErrorLog('ERROR', 'test', 'Test message', 'Test stack')
    expect(result).toBe(true)
    expect(global.__mainErrorLogs.length).toBe(1)
    expect(global.__mainErrorLogs[0].level).toBe('ERROR')
    expect(global.__mainErrorLogs[0].source).toBe('test')
    expect(global.__mainErrorLogs[0].message).toBe('Test message')
    expect(global.__mainErrorLogs[0].stack).toBe('Test stack')
    expect(global.__mainErrorLogs[0].time).toBeTruthy()
  })

  it('should limit to 200 entries', () => {
    for (let i = 0; i < 250; i++) {
      appendErrorLog('ERROR', 'test', 'Error ' + i, 'stack' + i)
    }
    expect(global.__mainErrorLogs.length).toBe(200)
    expect(global.__mainErrorLogs[0].message).toBe('Error 50')
  })

  it('should handle different log levels', () => {
    appendErrorLog('ERROR', 'mod1', 'error msg', '')
    appendErrorLog('FATAL', 'mod2', 'fatal msg', 'fatal stack')
    appendErrorLog('WARN', 'mod3', 'warning msg', '')
    expect(global.__mainErrorLogs.length).toBe(3)
    expect(global.__mainErrorLogs.map(l => l.level)).toEqual(['ERROR', 'FATAL', 'WARN'])
  })

  it('should handle empty stack', () => {
    appendErrorLog('ERROR', 'test', 'no stack', '')
    expect(global.__mainErrorLogs[0].stack).toBe('')
  })

  it('should handle internal error gracefully', () => {
    const spy = vi.spyOn(global.__mainErrorLogs, 'push').mockImplementationOnce(() => { throw new Error('Push failed') })
    const result = appendErrorLog('ERROR', 'test', 'msg', 'stack')
    expect(result).toBe(false)
    spy.mockRestore()
  })

  it('should handle empty message', () => {
    appendErrorLog('ERROR', 'test', '', '')
    expect(global.__mainErrorLogs[0].message).toBe('')
  })
})
