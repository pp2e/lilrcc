#include "lilrcc.h"

#include <QDebug>
#include <QIODevice>

ResourceLibrary::ResourceLibrary(ResourceReader *reader)
    : m_root(":", 0)
{
    reader->readTreeDirChildren(&m_root, 0);
}

void ResourceLibrary::printTree(QTextStream &out) {
    out << m_root.name() << "\n";
    printDirTree(&m_root, out);
}

QList<QString> ResourceLibrary::ls(QString path, Lilrcc::Error &error) {
    QStringList pathSegments = parsePath(path);
    ResourceTreeNode *node = getNode(pathSegments, error);
    if (error != Lilrcc::NoError) return {};
    if (!node->isDir()) {
        error = Lilrcc::GotFileInsteadOfDir;
        return {};
    }
    QList<QString> entries;
    ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
    for (ResourceTreeNode* node : dir->children()) {
        entries << node->name() + (node->isDir() ? "/" : "");
    }
    return entries;
}

QByteArray ResourceLibrary::getFile(QString path, Lilrcc::Error &error) {
    QStringList pathSegments = parsePath(path);
    ResourceTreeNode *node = getNode(pathSegments, error);
    if (error != Lilrcc::NoError) return {};
    if (node->isDir()) {
        error = Lilrcc::GotFileInsteadOfDir;
        return {};
    }
    ResourceTreeFile *file = static_cast<ResourceTreeFile*>(node);
    QByteArray data = file->read(error);
    if (error != Lilrcc::NoError) return {};

    return data;
}

bool ResourceLibrary::rmFile(QString path, Lilrcc::Error &error) {
    QStringList pathSegments = parsePath(path);
    QString nodeName = pathSegments.takeLast();
    ResourceTreeNode *node = getNode(pathSegments, error);
    if (error != Lilrcc::NoError) return false;
    if (!node->isDir()) {
        error = Lilrcc::GotFileInsteadOfDir;
        return false;
    }
    ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
    ResourceTreeNode *child = binSearchNode(dir->children(), qt_hash(nodeName));
    delete child;
    dir->removeChild(child);
    return true;
}

bool ResourceLibrary::mvFile(QString source, QString dest, Lilrcc::Error &error) {
    QStringList sourceSegments = parsePath(source);
    QString sourceName = sourceSegments.takeLast();
    ResourceTreeNode *node = getNode(sourceSegments, error);
    if (error != Lilrcc::NoError) return false;
    if (!node->isDir()) {
        error = Lilrcc::GotFileInsteadOfDir;
        return false;
    }
    ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
    ResourceTreeNode *child = binSearchNode(dir->children(), qt_hash(sourceName));
    if (!child) {
        error = Lilrcc::EntryNotFound;
        return false;
    }
    dir->removeChild(child);

    QStringList destSegments = parsePath(dest);
    ResourceTreeNode *destNode = getNode(destSegments, error);
    if (error != Lilrcc::NoError) {
        delete child;
        return false;
    }
    if (!destNode->isDir()) {
        error = Lilrcc::GotFileInsteadOfDir;
        delete child;
        return false;
    }
    ResourceTreeDir *destDir = static_cast<ResourceTreeDir*>(destNode);
    destDir->insertChild(child);
    return true;
}

bool ResourceLibrary::addFile(QByteArray data, QString name, QString dest, Lilrcc::Error &error) {
    QByteArrayResourceTreeFile *file = new QByteArrayResourceTreeFile(name, qt_hash(name), data);

    QStringList destSegments = parsePath(dest);
    ResourceTreeNode *destNode = getNode(destSegments, error);
    if (error != Lilrcc::NoError) {
        delete file;
        return false;
    }
    if (!destNode->isDir()) {
        error = Lilrcc::GotFileInsteadOfDir;
        delete file;
        return false;
    }
    ResourceTreeDir *destDir = static_cast<ResourceTreeDir*>(destNode);
    destDir->insertChild(file);
    return true;
}

void ResourceLibrary::save(ResourceWriter *writer) {
    writer->write(&m_root, 3);
}

QString tab = "";
void ResourceLibrary::printDirTree(ResourceTreeDir *rootNode, QTextStream &out) {
    QList<ResourceTreeNode*> nodes = rootNode->children();
    for (int i = 0; i < nodes.size(); i++) {
        bool last = (i == nodes.size()-1);
        ResourceTreeNode *node = nodes.at(i);
        out << tab << (last ? "└── " : "├── ") << node->name();
        if (node->isDir()) {
            out << "\n";
            tab += last ? "    " : "│   ";
            printDirTree(static_cast<ResourceTreeDir*>(node), out);
            tab.chop(4);
        } else {
            if (dynamic_cast<ZlibResourceTreeFile*>(node))
                out << " -zlib";
            if (dynamic_cast<ZstdResourceTreeFile*>(node))
                out << " -zstd";
            out << "\n";
        }
    }
}

QStringList ResourceLibrary::parsePath(QString path) {
    // User provided rcc styled path, no problem
    if (path.startsWith(":/")) path.remove(0, 2);
    return path.split('/', Qt::SkipEmptyParts);
}

// uses binary search for fast finding child node with specified hash
ResourceTreeNode *ResourceLibrary::binSearchNode(QList<ResourceTreeNode*> children, quint32 searchHash) {
    if (children.isEmpty())
        return nullptr;

    int childrenCount = children.size();
    int firstChild = 0;

    ResourceTreeNode *node = children.at(firstChild+childrenCount/2);
    while (searchHash != node->nameHash()) {
        if (childrenCount == 1)
            return nullptr;

        if (searchHash < node->nameHash())
            childrenCount /= 2;
        else {
            firstChild += childrenCount/2;
            childrenCount -= childrenCount/2;
        }
        node = children.at(firstChild+childrenCount/2);
    }
    return node;
}

ResourceTreeNode *ResourceLibrary::getNode(QStringList path, Lilrcc::Error &error) {
    ResourceTreeNode *node = &m_root;
    for (QString &segment : path) {
        if (!node->isDir()) {
            error = Lilrcc::GotFileInsteadOfDir;
            return nullptr;
        }
        ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
        node = binSearchNode(dir->children(), qt_hash(segment));
        if (!node) {
            error = Lilrcc::EntryNotFound;
            return nullptr;
        }
    }
    return node;
}
