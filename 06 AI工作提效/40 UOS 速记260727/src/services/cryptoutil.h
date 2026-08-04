// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CRYPTOUTIL_H
#define CRYPTOUTIL_H

#include <QString>
#include <QByteArray>

/**
 * @brief 简单的 API Key 加密/解密工具
 *
 * 使用 XOR + Base64 编码对敏感配置（如 API Key）进行轻量级加密存储，
 * 防止 API Key 以明文形式出现在 INI 配置文件中。
 *
 * 注意：这是基础级别的混淆保护，不是强加密。
 * 如需更高安全等级，建议使用系统密钥环（如 libsecret / KWallet）。
 */
class CryptoUtil
{
public:
    /// 对明文 API Key 进行加密，返回 Base64 编码的密文
    static QString encrypt(const QString &plainText);

    /// 解密 Base64 编码的密文，返回明文 API Key
    static QString decrypt(const QString &cipherText);

    /// 归一化解密：循环解开历史遗留的多层加密（重复保存导致），保证返回真正的明文
    static QString decryptDeep(const QString &text);

    /// 快速检查一段文本是否已加密（以特定前缀开头）
    static bool isEncrypted(const QString &text);

private:
    // 编译期固定的 XOR 密钥（每个字节不同，避免简单 XOR 可预测性）
    static const int KEY_LENGTH = 32;
    static const unsigned char XOR_KEY[KEY_LENGTH];

    // 加密标识前缀
    static const QString ENC_PREFIX;
};

#endif // CRYPTOUTIL_H
