#include "logmodel.h"

int LogModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant LogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};
    const auto &line = m_lines.at(index.row());
    switch (role) {
    case TimeRole: return line.time;
    case TextRole: return line.text;
    case KindRole: return line.kind;
    }
    return {};
}

QHash<int, QByteArray> LogModel::roleNames() const {
    return {{TimeRole, "time"}, {TextRole, "text"}, {KindRole, "kind"}};
}

void LogModel::setLines(const QList<GitHub::LogLine> &lines) {
    const int old = m_lines.size();
    const bool grows = old > 0 && lines.size() > old && lines.at(old - 1).text == m_lines.last().text &&
                       lines.at(old - 1).time == m_lines.last().time;
    if (grows) {
        beginInsertRows({}, old, lines.size() - 1);
        m_lines = lines;
        endInsertRows();
    } else if (lines.size() != old || old == 0 || lines.last().text != m_lines.last().text) {
        beginResetModel();
        m_lines = lines;
        endResetModel();
    }
    emit firstErrorChanged();
}

void LogModel::clear() {
    if (m_lines.isEmpty())
        return;
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit firstErrorChanged();
}

QString LogModel::text() const {
    QStringList out;
    for (const auto &line : m_lines)
        out << line.text;
    return out.join('\n');
}

int LogModel::firstError() const {
    return nextError(-1);
}

int LogModel::nextError(int after) const {
    for (int i = after + 1; i < m_lines.size(); ++i)
        if (m_lines.at(i).kind == GitHub::Error)
            return i;
    return -1;
}
