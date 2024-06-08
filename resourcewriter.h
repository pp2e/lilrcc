#ifndef RESOURCEWRITER_H
#define RESOURCEWRITER_H

#include <QIODevice>
#include <QHash>

class ResourceLibrary;
class ResourceTreeDir;
class ResourceTreeNode;
class ResourceTreeFile;
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
    void enumerateEntries(ResourceTreeDir *dir);
    void writeNames();
    void writeTree(ResourceTreeDir *dir);

    QIODevice *m_device;
    quint32 m_version;
    quint32 m_treeOffset;
    quint32 m_dataOffset;
    quint32 m_namesOffset;
    quint32 m_overallFlags;
    // This for storing offset
    QHash<QString, quint32> m_names;
    QHash<ResourceTreeFile*, quint32> m_files;
    // this for writing
    QStringList m_writeNames;
};

#endif // RESOURCEWRITER_H
