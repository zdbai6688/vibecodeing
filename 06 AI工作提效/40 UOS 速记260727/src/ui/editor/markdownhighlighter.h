#ifndef MARKDOWNHIGHLIGHTER_H
#define MARKDOWNHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    void updateThemeColors();

    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightRule> m_rules;

    QTextCharFormat m_h1Format;
    QTextCharFormat m_h2Format;
    QTextCharFormat m_h3Format;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_listFormat;
    QTextCharFormat m_quoteFormat;
    QTextCharFormat m_horizontalRuleFormat;
};

#endif // MARKDOWNHIGHLIGHTER_H
