#include "neteaseprovider.h"

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>

using namespace Qt::Literals::StringLiterals;

namespace NeteaseProvider {

QString LrcToString(QString irc_text) {
    QRegularExpression re(R"(\[\s*\d{1,2}:\d{2}(?:\.\d{1,3})?\])"_L1);

    QStringList lines = irc_text.split(QRegularExpression("\r\n|\r|\n"_L1), Qt::KeepEmptyParts);
    QStringList out;
    out.reserve(lines.size());

    for (QString line : lines) {
        line.remove(re);
        line = line.trimmed();
        out.append(line);
    }

    return out.join('\n'_L1);
}

}
