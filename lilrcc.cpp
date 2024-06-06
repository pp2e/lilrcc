#include "lilrcc.h"

#include <QDebug>
#include <QIODevice>

ResourceLibrary::ResourceLibrary(ResourceReader *reader)
    : m_root(":", 0)
{
    reader->readTreeDirChildren(&m_root, 0);
    m_readerData = reader->data();
}

void ResourceLibrary::printTree(QTextStream &out) {
    out << m_root.name() << "\n";
    printDirTree(&m_root, out);
}

bool ResourceLibrary::ls(QString path, QString &error) {
    QStringList pathSegments = parsePath(path);
    ResourceTreeNode *node = getNode(pathSegments, error);
    if (!error.isEmpty()) return false;
    if (!node->isDir()) {
        error = "Tree entry is file";
        return false;
    }
    ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
    for (ResourceTreeNode* node : dir->children()) {
        qDebug() << node->name() + (node->isDir() ? "/" : "");
    }
    return true;
}

QByteArray ResourceLibrary::getFile(QString path, QTextStream &out, QString &error) {
    QStringList pathSegments = parsePath(path);
    ResourceTreeNode *node = getNode(pathSegments, error);
    if (!error.isEmpty()) return {};
    if (node->isDir()) {
        qWarning() << "File is not file (directory)";
        return {};
    }
    ResourceTreeFile *file = static_cast<ResourceTreeFile*>(node);
    QByteArray data = file->read(error);
    if (!error.isEmpty()) return {};

    return data;
}

bool ResourceLibrary::rmFile(QString path, QString &error) {
    QStringList pathSegments = parsePath(path);
    QString nodeName = pathSegments.takeLast();
    ResourceTreeNode *node = getNode(pathSegments, error);
    if (!error.isEmpty()) return false;
    if (!node->isDir()) {
        error = "Got file instead of dir";
        return false;
    }
    ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
    ResourceTreeNode *child = binSearchNode(dir->children(), qt_hash(nodeName));
    delete child;
    dir->removeChild(child);
    return true;
}

bool ResourceLibrary::mvFile(QString source, QString dest, QString &error) {
    QStringList sourceSegments = parsePath(source);
    QString sourceName = sourceSegments.takeLast();
    ResourceTreeNode *node = getNode(sourceSegments, error);
    if (!error.isEmpty()) {
        error = "source: " + error;
        return false;
    }
    if (!node->isDir()) {
        error = "Got file instead of dir";
        return false;
    }
    ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
    ResourceTreeNode *child = binSearchNode(dir->children(), qt_hash(sourceName));
    if (!child) {
        error = "Source entry not found";
        return false;
    }
    dir->removeChild(child);

    QStringList destSegments = parsePath(dest);
    ResourceTreeNode *destNode = getNode(destSegments, error);
    if (!error.isEmpty()) {
        error = "target: " + error;
        delete child;
        return false;
    }
    if (!destNode->isDir()) {
        error = "Got file instead of dir";
        delete child;
        return false;
    }
    ResourceTreeDir *destDir = static_cast<ResourceTreeDir*>(destNode);
    destDir->insertChild(child);
    return true;
}

bool ResourceLibrary::addFile(QByteArray data, QString name, QString dest, QString &error) {
    QByteArrayResourceTreeFile *file = new QByteArrayResourceTreeFile(name, qt_hash(name), data);

    QStringList destSegments = parsePath(dest);
    ResourceTreeNode *destNode = getNode(destSegments, error);
    if (!error.isEmpty()) {
        error = "target: " + error;
        delete file;
        return false;
    }
    if (!destNode->isDir()) {
        error = "Got file instead of dir";
        delete file;
        return false;
    }
    ResourceTreeDir *destDir = static_cast<ResourceTreeDir*>(destNode);
    destDir->insertChild(file);
    return true;
}

void ResourceLibrary::save(ResourceWriter *writer) {
    writer->write(&m_root, m_readerData.version);
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

ResourceTreeNode *ResourceLibrary::getNode(QStringList path, QString &error) {
    ResourceTreeNode *node = &m_root;
    for (QString &segment : path) {
        if (!node->isDir()) {
            error = "Got file instead of dir";
            return nullptr;
        }
        ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
        node = binSearchNode(dir->children(), qt_hash(segment));
        if (!node) {
            error = "Entry not found";
            return nullptr;
        }
    }
    return node;
}
