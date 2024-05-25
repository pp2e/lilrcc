#include "tree.h"

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

bool ResourceTreeDir::removeChild(ResourceTreeNode *node) {
    return m_children.removeOne(node);
}

QList<ResourceTreeNode *> ResourceTreeDir::children() {
    return m_children;
}

// File
ResourceTreeFile::ResourceTreeFile(QString name, quint32 nameHash)
    : ResourceTreeNode(name, nameHash) {}

bool ResourceTreeFile::isDir() {
    return false;
}

UncompressedResourceTreeFile::UncompressedResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset)
    : ResourceTreeFile(name, nameHash)
    , m_reader(reader)
    , m_dataOffset(dataOffset) {}

QByteArray UncompressedResourceTreeFile::read(QString &error) {
    return m_reader->readData(m_dataOffset);
}

ZlibResourceTreeFile::ZlibResourceTreeFile(QString name, quint32 nameHash, ResourceReader *reader, quint32 dataOffset)
    : ResourceTreeFile(name, nameHash)
    , m_reader(reader)
    , m_dataOffset(dataOffset) {}

QByteArray ZlibResourceTreeFile::read(QString &error) {
    QByteArray rawData = m_reader->readData(m_dataOffset);
    return qUncompress(rawData);
}

QByteArray UnimplementedResourceTreeFile::read(QString &error) {
    error = "Calling read on unimplemented file. Well, it is unimplemented";
    return {};
}
