// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "markdownhighlighter.h"
#include <DGuiApplicationHelper>

DGUI_USE_NAMESPACE

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    updateThemeColors();
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, [this]() {
        updateThemeColors();
        rehighlight();
    });
}

void MarkdownHighlighter::updateThemeColors()
{
    auto *helper = DGuiApplicationHelper::instance();
    bool dark = (helper->themeType() == DGuiApplicationHelper::DarkType);

    // 深色模式使用更柔和的颜色，确保对比度
    m_h1Format.setFontWeight(QFont::Bold);
    m_h1Format.setFontPointSize(18);
    m_h1Format.setForeground(dark ? QColor("#FF7B7B") : QColor(230, 69, 69));

    m_h2Format.setFontWeight(QFont::Bold);
    m_h2Format.setFontPointSize(16);
    m_h2Format.setForeground(dark ? QColor("#FFB84D") : QColor(230, 138, 0));

    m_h3Format.setFontWeight(QFont::Bold);
    m_h3Format.setFontPointSize(14);
    m_h3Format.setForeground(dark ? QColor("#73B9FF") : QColor(24, 144, 255));

    m_boldFormat.setFontWeight(QFont::Bold);

    m_italicFormat.setFontItalic(true);

    m_codeFormat.setForeground(dark ? QColor("#FF7B7B") : QColor(230, 69, 69));
    m_codeFormat.setFontFamilies({"monospace"});
    m_codeFormat.setBackground(QBrush(dark ? QColor(255, 255, 255, 20) : QColor(128, 128, 128, 30)));

    m_linkFormat.setForeground(dark ? QColor("#73B9FF") : QColor(24, 144, 255));
    m_linkFormat.setFontUnderline(true);

    m_listFormat.setForeground(dark ? QColor("#7AE87A") : QColor(82, 196, 26));

    m_quoteFormat.setForeground(dark ? QColor("#AAAAAA") : QColor(128, 128, 128));
    m_quoteFormat.setFontItalic(true);

    m_horizontalRuleFormat.setForeground(dark ? QColor("#666666") : QColor(180, 180, 180));
    m_horizontalRuleFormat.setFontStrikeOut(true);

    // 更新规则
    m_rules = {
        { QRegularExpression("^# [^#].+"), m_h1Format },
        { QRegularExpression("^## [^#].+"), m_h2Format },
        { QRegularExpression("^### [^#].+"), m_h3Format },
        { QRegularExpression("\\*\\*[^\\*]+\\*\\*"), m_boldFormat },
        { QRegularExpression("(?<!\\*)\\*[^\\*]+\\*(?!\\*)"), m_italicFormat },
        { QRegularExpression("`[^`]+`"), m_codeFormat },
        { QRegularExpression("\\[.+\\]\\(.*\\)"), m_linkFormat },
        { QRegularExpression("^[\\-\\*]\\s"), m_listFormat },
        { QRegularExpression("^>\\s"), m_quoteFormat },
        { QRegularExpression("^[-\\*]{3,}$"), m_horizontalRuleFormat },
    };
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    for (const auto &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // 多行代码块 ```...``` 处理
    static const QRegularExpression codeBlockRe("^```");
    setCurrentBlockState(0);

    if (previousBlockState() != 1) {
        if (text.contains(codeBlockRe)) {
            setCurrentBlockState(1);
            setFormat(0, text.length(), m_codeFormat);
        }
    } else {
        setFormat(0, text.length(), m_codeFormat);
        if (text.contains(codeBlockRe)) {
            setCurrentBlockState(0);
        } else {
            setCurrentBlockState(1);
        }
    }
}
