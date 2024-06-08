#ifndef LILRCCREADER_H
#define LILRCCREADER_H

#include "error.h"

#include <QString>
#include <QIODevice>
#include <QTextStream>

class ResourceTreeDir;
class ResourceReader {
public:
    ResourceReader(QIODevice *device);

    Lilrcc::Error error();

    void readTreeDirChildren(ResourceTreeDir *dirNode, int nodeNumber);
    QString readName(quint32 offset);
    quint32 readHash(quint32 offset);
    QByteArray readData(quint32 dataOffset);

    void printHeader(QTextStream &out);
    void printEntries(QTextStream &out);
    void printNames(QTextStream &out);

private:
    quint8 readNumber();
    quint16 readNumber2();
    quint32 readNumber4();
    quint64 readNumber8();

private:
    Lilrcc::Error m_error;

    quint32 m_version;
    quint32 m_treeOffset;
    quint32 m_dataOffset;
    quint32 m_namesOffset;
    quint32 m_overallFlags;
    quint32 m_treeEntrySize;

    QIODevice *m_device;
};

#endif // LILRCCREADER_H
