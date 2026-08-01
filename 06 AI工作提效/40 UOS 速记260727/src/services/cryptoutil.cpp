// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cryptoutil.h"

#include <QByteArray>
#include <QDebug>

// XOR 密钥 — 每个字节不同，编译期固定
const unsigned char CryptoUtil::XOR_KEY[KEY_LENGTH] = {
    0xA3, 0x5B, 0xC7, 0x1E, 0x9F, 0x42, 0xE8, 0x6D,
    0x37, 0x91, 0x4C, 0x8A, 0x2F, 0xD6, 0x78, 0xB1,
    0xE4, 0x0C, 0x5A, 0xF3, 0x29, 0x87, 0x4B, 0x1C,
    0x6A, 0x95, 0x3E, 0x70, 0xBD, 0x62, 0xF8, 0x0D
};

const QString CryptoUtil::ENC_PREFIX = QStringLiteral("$enc$");

QString CryptoUtil::encrypt(const QString &plainText)
{
    if (plainText.isEmpty()) {
        return QString();
    }

    QByteArray data = plainText.toUtf8();
    QByteArray result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); ++i) {
        result.append(static_cast<char>(data.at(i) ^ XOR_KEY[i % KEY_LENGTH]));
    }

    // Base64 编码并添加加密标识前缀
    QString encoded = QString::fromLatin1(result.toBase64());
    return ENC_PREFIX + encoded;
}

QString CryptoUtil::decrypt(const QString &cipherText)
{
    if (cipherText.isEmpty()) {
        return QString();
    }

    if (!isEncrypted(cipherText)) {
        return cipherText; // 未加密，按明文返回
    }

    // 去除前缀
    QString payload = cipherText.mid(ENC_PREFIX.length());

    QByteArray data = QByteArray::fromBase64(payload.toLatin1());
    QByteArray result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); ++i) {
        result.append(static_cast<char>(data.at(i) ^ XOR_KEY[i % KEY_LENGTH]));
    }

    return QString::fromUtf8(result);
}

bool CryptoUtil::isEncrypted(const QString &text)
{
    return text.startsWith(ENC_PREFIX);
}
