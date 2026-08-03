#!/usr/bin/env node
const crypto = require('crypto');
const fs = require('fs');
const WebSocket = require('ws');

const args = process.argv.slice(2);
let isTestMode = false;

if (args[0] === '--test') {
    isTestMode = true;
    args.shift();
}

const [appid, apiKey, apiSecret, audioFile] = args;

if (!appid || !apiKey || !apiSecret || (!isTestMode && !audioFile)) {
    console.log(JSON.stringify({ success: false, error: 'Usage: node xfyun_asr.js [--test] <appid> <apikey> <apisecret> [audio_file]' }));
    process.exit(1);
}

if (!isTestMode && !fs.existsSync(audioFile)) {
    console.log(JSON.stringify({ success: false, error: '音频文件不存在: ' + audioFile }));
    process.exit(1);
}

// 讯飞语音听写 API 使用 WebSocket 协议
const host = 'iat-api.xfyun.cn';
const path = '/v2/iat';
const date = new Date().toUTCString();

// 构建签名
const signatureOrigin = `host: ${host}\ndate: ${date}\nPOST ${path} HTTP/1.1`;
const signature = crypto.createHmac('sha256', apiSecret).update(signatureOrigin).digest('base64');
const authorizationOrigin = `api_key="${apiKey}", algorithm="hmac-sha256", headers="host date request-line", signature="${signature}"`;
const authorization = Buffer.from(authorizationOrigin).toString('base64');

const wsUrl = `wss://${host}${path}?authorization=${encodeURIComponent(authorization)}&date=${encodeURIComponent(date)}&host=${host}`;

let timeout = setTimeout(() => {
    console.log(JSON.stringify({
        success: false,
        message: '连接讯飞服务器超时，请检查网络和 API Key 配置',
        error: 'timeout'
    }));
    process.exit(0);
}, 15000);

const ws = new WebSocket(wsUrl);

function sendClose() {
    try { ws.close(); } catch(e) {}
}

ws.addEventListener('open', () => {
    clearTimeout(timeout);

    if (isTestMode) {
        // 测试模式：发送静音帧验证服务开通状态
        const frame = {
            common: { app_id: appid },
            business: { language: 'zh_cn', domain: 'iat', accent: 'mandarin', ptt: 0, vad_eos: 1000 },
            data: { status: 2, format: 'audio/L16;rate=16000', encoding: 'raw', audio: '' }
        };
        ws.send(JSON.stringify(frame));

        timeout = setTimeout(() => {
            sendClose();
            console.log(JSON.stringify({ success: true, message: '讯飞语音服务连接成功，APPID 已开通语音听写服务' }));
            process.exit(0);
        }, 5000);
    } else {
        // 转写模式
        const audioData = fs.readFileSync(audioFile);
        const base64Audio = audioData.toString('base64');

        const frame = {
            common: { app_id: appid },
            business: { language: 'zh_cn', domain: 'iat', accent: 'mandarin', ptt: 1, vad_eos: 3000 },
            data: { status: 2, format: 'audio/L16;rate=16000', encoding: 'raw', audio: base64Audio }
        };
        ws.send(JSON.stringify(frame));

        timeout = setTimeout(() => {
            sendClose();
            console.log(JSON.stringify({
                success: resultText.length > 0,
                text: resultText,
                error: resultText ? '' : '处理超时'
            }));
            process.exit(0);
        }, 15000);
    }
});

let resultText = '';

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
                if (isTestMode) {
                    clearTimeout(timeout);
                    sendClose();
                    console.log(JSON.stringify({ success: true, message: '讯飞语音服务连接成功，APPID 已开通语音听写服务' }));
                } else {
                    clearTimeout(timeout);
                    sendClose();
                    if (resultText) {
                        console.log(JSON.stringify({ success: true, text: resultText }));
                    }
                }
                process.exit(0);
            }
        } else {
            clearTimeout(timeout);
            sendClose();
            console.log(JSON.stringify({
                success: false,
                message: `讯飞API错误 (code=${data.code}): ${data.message || '未知错误'}。请确认 APPID 已开通语音听写服务`,
                error: data.message || `code=${data.code}`
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
        message: 'WebSocket 连接失败，请检查网络是否能访问 iat-api.xfyun.cn，以及 API Key/Secret 是否正确',
        error: e.message || 'connection_error'
    }));
    process.exit(0);
});

ws.addEventListener('close', (event) => {
    clearTimeout(timeout);
    if (resultText && !isTestMode) {
        console.log(JSON.stringify({ success: true, text: resultText }));
    } else if (!isTestMode && event.code !== 1000 && event.code !== 1005) {
        // 非正常关闭
    }
    if (isTestMode && !resultText) {
        console.log(JSON.stringify({ success: true, message: '讯飞语音服务连接成功' }));
    }
    process.exit(0);
});
