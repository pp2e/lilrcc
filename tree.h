#ifndef TREE_H
#define TREE_H

#include "resourcereader.h"

#include <QString>
#include <QList>

// Abstract node
class ResourceTreeNode {
public:
    ResourceTreeNode(QString name, quint32 nameHash);
    virtual ~ResourceTreeNode();
    QString name();
    quint32 nameHash();
    virtual bool isDir()=0;

protected:
    QString m_name;
    quint32 m_nameHash;
};

// Directory
class ResourceTreeDir : public ResourceTreeNode {
public:
    using ResourceTreeNode::ResourceTreeNode;
    ~ResourceTreeDir();

    bool isDir() override;
    bool appendChild(ResourceTreeNode *node);
    bool removeChild(ResourceTreeNode *node);
    QList<ResourceTreeNode*> children();
private:
    QList<ResourceTreeNode*> m_children;
};

// Abstract file
class ResourceTreeFile : public ResourceTreeNode {
public:
    ResourceTreeFile(QString name, quint32 nameHash);
    bool isDir() override;
    virtual QByteArray read(QString &error)=0;
};

// Uncompressed file from rcc
class UncompressedResourceTreeFile : public ResourceTreeFile {
public:
    UncompressedResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset);
    QByteArray read(QString &error);
protected:
    ResourceReader *m_reader;
    quint32 m_dataOffset;
};

// Zlib compressed file from rcc
class ZlibResourceTreeFile : public ResourceTreeFile {
public:
    ZlibResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset);
    QByteArray read(QString &error);
protected:
    ResourceReader *m_reader;
    quint32 m_dataOffset;
};

// Unimplemented file, always fails read
class UnimplementedResourceTreeFile : public ResourceTreeFile {
public:
    using ResourceTreeFile::ResourceTreeFile;
    QByteArray read(QString &error);
};

#endif // TREE_H
