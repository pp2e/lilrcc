#ifndef TREE_H
#define TREE_H

#include "resourcereader.h"

#include <QString>
#include <QList>

enum Flags {
    // must match qresource.cpp and rcc.h
    NoFlags = 0x00,
    Compressed = 0x01,
    Directory = 0x02,
    CompressedZstd = 0x04
};

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
enum Compression {
    NoCompression = 0,
    ZlibCompression = Flags::Compressed,
    ZstdCompression = Flags::CompressedZstd
};

class ResourceTreeFile : public ResourceTreeNode {
public:
    ResourceTreeFile(QString name, quint32 nameHash);
    bool isDir() override;
    virtual QByteArray read(QString &error)=0;
    virtual Compression getCompression()=0;
    virtual QByteArray getCompressed()=0;
};

// Uncompressed file from rcc
class UncompressedResourceTreeFile : public ResourceTreeFile {
public:
    UncompressedResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset);
    QByteArray read(QString &error);
    Compression getCompression();
    QByteArray getCompressed();
protected:
    ResourceReader *m_reader;
    quint32 m_dataOffset;
};

// Zlib compressed file from rcc
class ZlibResourceTreeFile : public ResourceTreeFile {
public:
    ZlibResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset);
    QByteArray read(QString &error);
    Compression getCompression();
    QByteArray getCompressed();
protected:
    ResourceReader *m_reader;
    quint32 m_dataOffset;
};

// Zstd compressed file from rcc
class ZstdResourceTreeFile : public ResourceTreeFile {
public:
    ZstdResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset);
    QByteArray read(QString &error);
    Compression getCompression();
    QByteArray getCompressed();
protected:
    ResourceReader *m_reader;
    quint32 m_dataOffset;
};

#endif // TREE_H
