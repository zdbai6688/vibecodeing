# ASR 服务集成测试报告

## 百度语音测试结果

| 步骤 | 结果 |
|------|------|
| 获取 Access Token | ✅ 成功（`24.fb067695...`） |
| 短语音识别 API | ❌ 返回 404，服务未开通 |
| AASR API | ❌ 返回 Unsupported openapi method |

**结论：该 APP 未开通「短语音识别」服务。**

## 解决方案

请登录百度智能云控制台，开通服务：

1. 打开 https://console.bce.baidu.com/ai/#/ai/speech/overview
2. 找到「短语音识别标准版」→ 点击「立即开通」
3. 选择已创建的应用（或新建应用），绑定 API Key
4. 开通后即可使用

## 已实现的 ASR 代码

- `src/services/asrservice.h/.cpp` — ASR 引擎接口 + 百度/讯飞引擎
- `src/services/xfyun_asr.js` — 讯飞 WebSocket 客户端
- 设置页已支持配置百度/讯飞/阿里云 API Key
- 所有 Key 已保存到配置文件 `~/.config/UOS速记/settings.conf`