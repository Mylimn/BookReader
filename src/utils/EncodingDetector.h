#pragma once

#include <QByteArray>
#include <QString>

namespace reader::utils
{

// TXT 文本编码识别与解码（PRD 第 5 节）
//
// 识别顺序：BOM → 无 BOM 的 UTF-16 启发式 → 严格 UTF-8 → 严格 GBK；
// 全部失败时回退 UTF-8 并把 reliable 置 false，由 UI 层提示用户。
class EncodingDetector
{
public:
    struct Result
    {
        QString text;         // 解码后的正文
        QString encoding;     // 实际采用的编码名：UTF-8 / UTF-16LE / UTF-16BE / GBK
        bool reliable = true; // false 表示无法识别、已回退 UTF-8，正文可能有乱码
    };

    static Result decode(const QByteArray &raw);
};

} // namespace reader::utils
