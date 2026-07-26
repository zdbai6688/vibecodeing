const path = require('path')
const fs = require('fs')
const { execSync, spawn } = require('child_process')

function formatBytes(bytes) {
  if (bytes === 0) return '0 B'
  const k = 1024, sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

function execOut(cmd) {
  try { return execSync(cmd, { timeout: 10000, encoding: 'utf-8' }).toString() } catch { return '' }
}

function sudoExec(cmd, pw) {
  if (!pw) { return execOut('sudo ' + cmd + ' 2>&1') }
  try {
    return execSync('sudo -S sh -c "' + cmd.replace(/"/g, '\\"') + ' 2>&1"', {
      timeout: 30000, encoding: 'utf-8', input: pw + '\n'
    }).toString()
  } catch(e) { return '' }
}

function parseMemInfo(meminfoText) {
  const mt = parseInt(meminfoText.match(/MemTotal:\s+(\d+)/)?.[1] || '0')
  const ma = parseInt(meminfoText.match(/MemAvailable:\s+(\d+)/)?.[1] || '0')
  const st = parseInt(meminfoText.match(/SwapTotal:\s+(\d+)/)?.[1] || '0')
  const sf = parseInt(meminfoText.match(/SwapFree:\s+(\d+)/)?.[1] || '0')
  return {
    total: Math.round(mt / 1024),
    available: Math.round(ma / 1024),
    used: Math.round((mt - ma) / 1024),
    usagePercent: mt > 0 ? Math.round(((mt - ma) / mt) * 100) : 0,
    swapTotal: Math.round(st / 1024),
    swapFree: Math.round(sf / 1024),
    swapUsed: Math.round((st - sf) / 1024)
  }
}

function parseProcStatForCpu(statText) {
  const c1 = statText.match(/^cpu\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/)
  if (!c1) return null
  const t1 = +c1[1] + +c1[2] + +c1[3] + +c1[4], i1 = +c1[4]
  return { total: t1, idle: i1 }
}

function calcCpuPercent(sample1, sample2) {
  if (!sample1 || !sample2) return 0
  const dt = sample2.total - sample1.total
  const di = sample2.idle - sample1.idle
  return dt > 0 ? Math.round((1 - di / dt) * 1000) / 10 : 0
}

function getDirSize(dir) {
  let s = 0
  try {
    const files = fs.readdirSync(dir, { withFileTypes: true })
    for (const f of files) {
      const fp = path.join(dir, f.name)
      if (f.isFile()) s += fs.statSync(fp).size
      else if (f.isDirectory()) s += getDirSize(fp)
    }
  } catch {}
  return s
}

function loadSettings(settingsFile) {
  try {
    if (fs.existsSync(settingsFile)) return JSON.parse(fs.readFileSync(settingsFile, 'utf-8'))
  } catch {}
  return {}
}

function saveSettings(settingsFile, settings) {
  try { fs.writeFileSync(settingsFile, JSON.stringify(settings, null, 2)) } catch {}
}

function aggregateProcesses(rawLines, blacklist, sortKey = 'cpu') {
  const groups = {}
  const lines = rawLines.filter(Boolean)
  for (const line of lines) {
    const p = line.trim().split(/\s+/)
    const item = {
      user: p[0], pid: p[1], cpu: parseFloat(p[2]), mem: parseFloat(p[3]),
      vsz: p[4], rss: p[5], tty: p[6], stat: p[7], start: p[8], time: p[9],
      command: p.slice(10).join(' ')
    }
    const cmd = item.command || ''
    const parts = cmd.split('/')
    const fullName = parts[parts.length - 1] || cmd
    const shortName = fullName.split(/\s+/)[0]
    if (!shortName || blacklist.includes(shortName)) continue
    if (!groups[shortName]) {
      groups[shortName] = { shortName, fullCmd: cmd, cpu: 0, mem: 0, count: 0, pids: [] }
    }
    groups[shortName].cpu += item.cpu
    groups[shortName].mem += item.mem
    groups[shortName].count++
    groups[shortName].pids.push({ pid: item.pid, cpu: item.cpu, mem: item.mem, user: item.user, fullCmd: cmd })
    if (cmd.length > groups[shortName].fullCmd.length) groups[shortName].fullCmd = cmd
  }
  return Object.values(groups).sort((a, b) => b[sortKey] - a[sortKey]).slice(0, 30)
}

module.exports = {
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
}