#include "EncodingDetector.h"

#include <QStringDecoder>
#include <QTextCodec>

namespace reader::utils
{

namespace
{

// 有 BOM 时按 BOM 判定，最可靠；无 BOM 返回空串
QString encodingFromBom(const QByteArray &raw)
{
    if (raw.startsWith("\xEF\xBB\xBF")) {
        return QStringLiteral("UTF-8");
    }
    if (raw.startsWith("\xFF\xFE")) {
        return QStringLiteral("UTF-16LE");
    }
    if (raw.startsWith("\xFE\xFF")) {
        return QStringLiteral("UTF-16BE");
    }
    return QString();
}

// 无 BOM 的 UTF-16 启发式：文本的高位字节通常为 0x00，且规律地落在奇数或偶数下标上。
// 局限：以中文为主的 UTF-16 文本几乎不含 0x00，本启发式识别不了，会落到后面的回退分支。
QString guessUtf16WithoutBom(const QByteArray &raw)
{
    const int sampleSize = qMin(raw.size(), 512);
    if (sampleSize < 16) {
        return QString();
    }

    int nulAtEven = 0;
    int nulAtOdd = 0;
    for (int i = 0; i < sampleSize; ++i) {
        if (raw.at(i) != '\0') {
            continue;
        }
        if (i % 2 == 0) {
            ++nulAtEven;
        } else {
            ++nulAtOdd;
        }
    }

    const int threshold = sampleSize / 8;
    if (nulAtOdd > threshold && nulAtEven == 0) {
        return QStringLiteral("UTF-16LE");
    }
    if (nulAtEven > threshold && nulAtOdd == 0) {
        return QStringLiteral("UTF-16BE");
    }
    return QString();
}

// 用 Qt 内置解码器严格解码：只要出现非法字节就判失败（用于"是不是 UTF-8"的判定）
bool decodeStrict(QStringDecoder::Encoding encoding, const QByteArray &raw, QString *out)
{
    QStringDecoder decoder(encoding);
    const QString text = decoder.decode(raw);
    if (decoder.hasError()) {
        return false;
    }
    *out = text;
    return true;
}

// 用传统编码（GBK）严格解码：只要有非法字符就判失败
bool decodeCodecStrict(const QByteArray &raw, const char *codecName, QString *out)
{
    QTextCodec *codec = QTextCodec::codecForName(codecName);
    if (codec == nullptr) {
        return false;
    }

    QTextCodec::ConverterState state;
    const QString text = codec->toUnicode(raw.constData(), static_cast<int>(raw.size()), &state);
    if (state.invalidChars != 0) {
        return false;
    }
    *out = text;
    return true;
}

} // namespace

EncodingDetector::Result EncodingDetector::decode(const QByteArray &raw)
{
    Result result;

    // 1) BOM 最可靠，直接采用；同时把 BOM 本身从正文里剔除
    const QString bomEncoding = encodingFromBom(raw);
    if (!bomEncoding.isEmpty()) {
        result.encoding = bomEncoding;
        if (bomEncoding == QLatin1String("UTF-8")) {
            decodeStrict(QStringDecoder::Utf8, raw.mid(3), &result.text);
        } else if (bomEncoding == QLatin1String("UTF-16LE")) {
            decodeStrict(QStringDecoder::Utf16LE, raw.mid(2), &result.text);
        } else {
            decodeStrict(QStringDecoder::Utf16BE, raw.mid(2), &result.text);
        }
        return result;
    }

    // 2) 无 BOM：先排查漏写 BOM 的 UTF-16（ASCII 为主的文本能靠 0x00 规律识别）
    const QString guessedUtf16 = guessUtf16WithoutBom(raw);
    if (!guessedUtf16.isEmpty()) {
        const bool decoded = (guessedUtf16 == QLatin1String("UTF-16LE"))
                                 ? decodeStrict(QStringDecoder::Utf16LE, raw, &result.text)
                                 : decodeStrict(QStringDecoder::Utf16BE, raw, &result.text);
        if (decoded) {
            result.encoding = guessedUtf16;
            return result;
        }
    }

    // 3) 严格 UTF-8：出现非法字节即否定（GBK 字节几乎必然触发非法序列）
    if (decodeStrict(QStringDecoder::Utf8, raw, &result.text)) {
        result.encoding = QStringLiteral("UTF-8");
        return result;
    }

    // 4) 严格 GBK：中文 TXT 里最常见的非 UTF-8 编码
    if (decodeCodecStrict(raw, "GBK", &result.text)) {
        result.encoding = QStringLiteral("GBK");
        return result;
    }

    // 5) 都认不出来：按 PRD 第 5 节回退 UTF-8（非法字节替换为 U+FFFD）并标记不可靠，交 UI 提示
    QStringDecoder fallbackDecoder(QStringDecoder::Utf8);
    result.text = fallbackDecoder.decode(raw);
    result.encoding = QStringLiteral("UTF-8");
    result.reliable = false;
    return result;
}

} // namespace reader::utils
