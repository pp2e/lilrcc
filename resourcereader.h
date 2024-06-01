#ifndef LILRCCREADER_H
#define LILRCCREADER_H

#include <QString>
#include <QIODevice>

struct ReaderData {
    quint32 version;
    quint32 treeOffset;
    quint32 dataOffset;
    quint32 namesOffset;
    quint32 overallFlags;
    quint32 treeEntrySize;
};

class ResourceTreeDir;
class ResourceReader {
public:
    ResourceReader(QIODevice *device);

    void readTreeDirChildren(ResourceTreeDir *dirNode, int nodeNumber);
    QString readName(quint32 offset);
    quint32 readHash(quint32 offset);
    QByteArray readData(quint32 dataOffset);
    ReaderData data();

private:
    quint8 readNumber();
    quint16 readNumber2();
    quint32 readNumber4();
    quint64 readNumber8();

private:
    ReaderData m_readerData;
    QIODevice *m_device;
};

#endif // LILRCCREADER_H
