#include "lilrcc.h"

#include <QDebug>
#include <QIODevice>

ResourceLibrary::ResourceLibrary(ResourceReader *reader)
    : m_root(":", 0)
{
    TreeEntry root = reader->readTreeEntry(0);
    appendChildNodes(&m_root, &root, reader);

}

void ResourceLibrary::printTree(QTextStream &out) {
    out << m_root.name() << "\n";
    printDirTree(&m_root, out);
    if (m_readerData.overallFlags & Flags::CompressedZstd) {
        out << "Looks like some files was compessed with zstd. Zstd currently is not supported by lilrcc\n";
    }
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

bool ResourceLibrary::getFile(QString path, QTextStream &out, QString &error) {
    QStringList pathSegments = parsePath(path);
    ResourceTreeNode *node = getNode(pathSegments, error);
    if (!error.isEmpty()) return false;
    if (node->isDir()) {
        qWarning() << "File is not file (directory)";
        return false;
    }
    ResourceTreeFile *file = static_cast<ResourceTreeFile*>(node);
    QByteArray data = file->read(error);
    if (!error.isEmpty()) return false;

    out << data;
    return true;
}

void ResourceLibrary::printAllFiles() {
    QList<ResourceTreeNode*> nodes = {&m_root};
    while (!nodes.isEmpty()) {
        ResourceTreeNode *node = nodes.takeFirst();
        if (node->isDir()) {
            ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
            nodes.append(dir->children());
        } else {
            qDebug() << node->name();
        }
    }
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
    if (!dir->removeChild(child))
        // dir always have this child, or we would'nt get it
        Q_UNREACHABLE();
    return true;
}

// void LilResourceLibrary::save(QTextStream &out) {
//     out << "123";
// }

void ResourceLibrary::appendChildNodes(ResourceTreeDir *dirNode, TreeEntry *dirEntry, ResourceReader *reader) {
    for (int i = 0; i < dirEntry->childrenCount; i++) {
        TreeEntry child = reader->readTreeEntry(dirEntry->firstChild+i);
        QString name = reader->readName(child);
        quint32 nameHash = reader->readHash(child);
        if (child.isDir()) {
            ResourceTreeDir *dir = new ResourceTreeDir(name, nameHash);
            appendChildNodes(dir, &child, reader);
            dirNode->appendChild(dir);
        } else if (child.isZlib()) {
            ResourceTreeFile *file = new ZlibResourceTreeFile(name, nameHash, reader, child.dataOffset);
            dirNode->appendChild(file);
        } else if (child.isZstd()) {
            ResourceTreeFile *file = new UnimplementedResourceTreeFile(name, nameHash);
            dirNode->appendChild(file);
        } else {
            ResourceTreeFile *file = new UncompressedResourceTreeFile(name, nameHash, reader, child.dataOffset);
            dirNode->appendChild(file);
        }
    }
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
