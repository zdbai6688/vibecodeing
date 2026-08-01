#!/usr/bin/env node
const crypto = require('crypto');
const fs = require('fs');
const http = require('http');

const [appid, apiKey, apiSecret, audioFile] = process.argv.slice(2);
if (!appid || !apiKey || !apiSecret || !audioFile) {
    console.log(JSON.stringify({ success: false, error: 'Usage: node xfyun_asr.js <appid> <apikey> <apisecret> <audio_file>' }));
    process.exit(1);
}

if (!fs.existsSync(audioFile)) {
    console.log(JSON.stringify({ success: false, error: '音频文件不存在: ' + audioFile }));
    process.exit(1);
}

// 讯飞语音听写 API 使用 WebSocket 协议
// 认证方式: wss://iat-api.xfyun.cn/v2/iat?authorization=<base64>&date=<rfc1123>&host=iat-api.xfyun.cn
const host = 'iat-api.xfyun.cn';
const path = '/v2/iat';
const date = new Date().toUTCString();

// 构建签名
const signatureOrigin = `host: ${host}\ndate: ${date}\nPOST ${path} HTTP/1.1`;
const signature = crypto.createHmac('sha256', apiSecret).update(signatureOrigin).digest('base64');
const authorizationOrigin = `api_key="${apiKey}", algorithm="hmac-sha256", headers="host date request-line", signature="${signature}"`;
const authorization = Buffer.from(authorizationOrigin).toString('base64');

const wsUrl = `wss://${host}${path}?authorization=${encodeURIComponent(authorization)}&date=${encodeURIComponent(date)}&host=${host}`;

// 读取音频文件并转为 base64
const audioData = fs.readFileSync(audioFile);
const base64Audio = audioData.toString('base64');

// 使用 Node.js 内置 WebSocket
const ws = new WebSocket(wsUrl);
let resultText = '';
let timeout = setTimeout(() => {
    ws.close();
    console.log(JSON.stringify({ success: false, error: '连接讯飞服务器超时，请检查网络和API Key配置' }));
    process.exit(0);
}, 20000);

ws.addEventListener('open', () => {
    clearTimeout(timeout);
    // 发送音频数据（一次性发送，status=2 表示结束）
    const frame = {
        common: { app_id: appid },
        business: { language: 'zh_cn', domain: 'iat', accent: 'mandarin', ptt: 1, vad_eos: 3000 },
        data: { status: 2, format: 'audio/L16;rate=16000', encoding: 'raw', audio: base64Audio }
    };
    ws.send(JSON.stringify(frame));

    // 超时
    timeout = setTimeout(() => {
        ws.close();
        console.log(JSON.stringify({ success: resultText.length > 0, text: resultText, error: resultText ? '' : '处理超时' }));
        process.exit(0);
    }, 15000);
});

ws.addEventListener('message', (event) => {
    try {
        const data = JSON.parse(event.data);
        if (data.code === 0) {
            if (data.data && data.data.result) {
                const r = data.data.result;
                if (r.ws) {
                    for (const w of r.ws) {
                        for (const cw of w.cw) resultText += cw.w;
                    }
                }
            }
            if (data.data && data.data.status === 2) {
                ws.close();
            }
        } else {
            ws.close();
            console.log(JSON.stringify({
                success: false,
                error: `讯飞API错误 (code=${data.code}): ${data.message || '未知错误'}`,
                tip: '请确认: 1) APPID已开通语音听写服务 2) API Key/Secret正确 3) 音频格式正确'
            }));
            process.exit(0);
        }
    } catch (e) {
        // 忽略解析错误
    }
});

ws.addEventListener('error', (e) => {
    clearTimeout(timeout);
    console.log(JSON.stringify({
        success: false,
        error: 'WebSocket连接失败: ' + (e.message || '请检查网络是否能访问 iat-api.xfyun.cn'),
        tip: '可能是网络环境限制或API Key无效'
    }));
    process.exit(0);
});

ws.addEventListener('close', (event) => {
    clearTimeout(timeout);
    if (resultText) {
        console.log(JSON.stringify({ success: true, text: resultText }));
    } else if (event.code !== 1000 && event.code !== 1005) {
        // 非正常关闭已经在 error 中处理了
    }
    process.exit(0);
});