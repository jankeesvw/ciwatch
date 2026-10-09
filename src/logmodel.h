#pragma once

#include <QAbstractListModel>

#include "github.h"

// The log of one job, line by line. A running job's log is fetched again on
// every poll; when the new text only adds lines, they're appended so the view
// keeps its place instead of jumping back to the top.
class LogModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int firstError READ firstError NOTIFY firstErrorChanged)

public:
    enum Role { TimeRole = Qt::UserRole + 1, TextRole, KindRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setLines(const QList<GitHub::LogLine> &lines);
    void clear();
    QString text() const;
    int firstError() const;
    Q_INVOKABLE int nextError(int after) const;

signals:
    void firstErrorChanged();

private:
    QList<GitHub::LogLine> m_lines;
};
