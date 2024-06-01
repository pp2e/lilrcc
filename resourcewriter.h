#ifndef RESOURCEWRITER_H
#define RESOURCEWRITER_H

#include <QIODevice>
#include <QHash>

struct ReaderData;
class ResourceLibrary;
class ResourceTreeDir;
class ResourceTreeNode;
class ResourceWriter {
public:
    ResourceWriter(QIODevice *device);

    void write(ResourceTreeDir *dir, quint32 version);

private:
    void writeNumber(quint8 number);
    void writeNumber2(quint16 number);
    void writeNumber4(quint32 number);
    void writeNumber8(quint64 number);

    void writeHeader();
    quint32 writeData(ResourceTreeDir *dir);
    quint32 writeName(QString name);
    quint32 writeNames(ResourceTreeDir *dir);
    void writeDataTree(ResourceTreeDir *dir);
    void rewriteHeader();

    QIODevice *m_device;
    quint32 m_version;
    quint32 m_treeOffset;
    quint32 m_dataOffset;
    quint32 m_namesOffset;
    quint32 m_overallFlags;
    QHash<QString, quint32> m_names;
};

#endif // RESOURCEWRITER_H
