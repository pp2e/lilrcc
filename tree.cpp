#include "tree.h"

#include <zstd.h>
#include <QDebug>

// uses binary search for fast finding child node with specified hash
int binSearchNode(QList<ResourceTreeNode*> children, quint32 searchHash, bool &replace) {
    replace = false;
    if (children.isEmpty())
        return 0;

    int childrenCount = children.size();
    int firstChild = 0;

    ResourceTreeNode *node = children.at(firstChild+childrenCount/2);
    while (searchHash != node->nameHash()) {
        if (searchHash < node->nameHash()) {
            if (childrenCount == 1)
                return firstChild+childrenCount/2;

            childrenCount /= 2;
        } else {
            if (childrenCount == 1)
                return firstChild+childrenCount/2+1;

            firstChild += childrenCount/2;
            childrenCount -= childrenCount/2;
        }
        node = children.at(firstChild+childrenCount/2);
    }
    replace = true;
    return firstChild+childrenCount/2;
}

ResourceTreeNode::ResourceTreeNode(QString name, quint32 nameHash)
    : m_name(name)
    , m_nameHash(nameHash) {}

ResourceTreeNode::~ResourceTreeNode() {}

QString ResourceTreeNode::name() {
    return m_name;
}

quint32 ResourceTreeNode::nameHash() {
    return m_nameHash;
}

// Dir
ResourceTreeDir::ResourceTreeDir(QString name, quint32 nameHash)
    : ResourceTreeNode(name, nameHash) {}

ResourceTreeDir::~ResourceTreeDir() {
    qDeleteAll(m_children);
}

bool ResourceTreeDir::isDir() {
    return true;
}

bool ResourceTreeDir::appendChild(ResourceTreeNode *node) {
    m_children.append(node);
    return true;
}

bool ResourceTreeDir::insertChild(ResourceTreeNode *node) {
    bool replace = false;
    int pos = binSearchNode(m_children, node->nameHash(), replace);
    if (replace) {
        delete m_children[pos];
        m_children[pos] = node;
    } else {
        m_children.insert(pos, node);
    }
    return true;
}

bool ResourceTreeDir::removeChild(ResourceTreeNode *node) {
    return m_children.removeOne(node);
}

QList<ResourceTreeNode *> ResourceTreeDir::children() {
    return m_children;
}

// File
ResourceTreeFile::ResourceTreeFile(QString name, quint32 nameHash, quint32 dataSize)
    : ResourceTreeNode(name, nameHash)
    , m_dataSize(dataSize) {}

bool ResourceTreeFile::isDir() {
    return false;
}

quint32 ResourceTreeFile::dataSize() {
    return m_dataSize;
}

UncompressedResourceTreeFile::UncompressedResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset, quint32 dataSize)
    : ResourceTreeFile(name, nameHash, dataSize)
    , m_reader(reader)
    , m_dataOffset(dataOffset) {}

QByteArray UncompressedResourceTreeFile::read(QString &error) {
    return m_reader->readData(m_dataOffset);
}

Compression UncompressedResourceTreeFile::getCompression() {
    return NoCompression;
}

QByteArray UncompressedResourceTreeFile::getCompressed() {
    return m_reader->readData(m_dataOffset);
}

ZlibResourceTreeFile::ZlibResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset, quint32 dataSize)
    : ResourceTreeFile(name, nameHash, dataSize)
    , m_reader(reader)
    , m_dataOffset(dataOffset) {}

QByteArray ZlibResourceTreeFile::read(QString &error) {
    QByteArray rawData = m_reader->readData(m_dataOffset);
    return qUncompress(rawData);
}

Compression ZlibResourceTreeFile::getCompression() {
    return ZlibCompression;
}

QByteArray ZlibResourceTreeFile::getCompressed() {
    return m_reader->readData(m_dataOffset);
}

ZstdResourceTreeFile::ZstdResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset, quint32 dataSize)
    : ResourceTreeFile(name, nameHash, dataSize)
    , m_reader(reader)
    , m_dataOffset(dataOffset) {}

QByteArray ZstdResourceTreeFile::read(QString &error) {
    QByteArray rawData = m_reader->readData(m_dataOffset);
    size_t uncompressedSize = ZSTD_getFrameContentSize(rawData.data(), rawData.size());
    if (ZSTD_isError(uncompressedSize)) {
        error = "Cannot get unpacked size";
        return {};
    }
    QByteArray unpackedData;
    unpackedData.resize(uncompressedSize);
    size_t size = ZSTD_decompress(unpackedData.data(), uncompressedSize, rawData.data(), rawData.size());
    if (ZSTD_isError(size)) {
        error = "Cannot get unpack";
        return {};
    }
    return unpackedData;
}

Compression ZstdResourceTreeFile::getCompression() {
    return ZstdCompression;
}

QByteArray ZstdResourceTreeFile::getCompressed() {
    return m_reader->readData(m_dataOffset);
}

QByteArrayResourceTreeFile::QByteArrayResourceTreeFile(QString name, quint32 nameHash, QByteArray data)
    : ResourceTreeFile(name, nameHash, 4+data.size())
    , m_data(data) {}

QByteArray QByteArrayResourceTreeFile::read(QString &error) {
    return m_data;
}

Compression QByteArrayResourceTreeFile::getCompression() {
    return NoCompression;
}

QByteArray QByteArrayResourceTreeFile::getCompressed() {
    return m_data;
}

