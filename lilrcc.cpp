#include "lilrcc.h"

#include <QDebug>
#include <QIODevice>

ResourceLibrary::ResourceLibrary(QIODevice *device)
    : m_reader(device)
    , m_root(":", 0)
{
    TreeEntry root = m_reader.readTreeEntry(0);
    appendChildNodes(&m_root, &root);

}

void ResourceLibrary::printTree(QTextStream &out) {
    out << m_root.name() << "\n";
    printDirTree(&m_root, out);
    if (m_reader.overallFlags() & Flags::CompressedZstd) {
        out << "Looks like some files was compessed with zstd. Zstd currently is not supported by lilrcc\n";
    }
}

bool ResourceLibrary::ls(QString path, QString &error) {
    ResourceTreeNode *node = getNode(path, error);
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
    ResourceTreeNode *node = getNode(path, error);
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

// bool LilResourceLibrary::rmFile(QString path, QString &error) {
//     const TreeEntry entry = getEntry(path, error);
//     if (!error.isEmpty()) return false;
//     qDebug() << m_reader.readName(entry);
//     return false;
// }

// void LilResourceLibrary::save(QTextStream &out) {
//     out << "123";
// }

void ResourceLibrary::appendChildNodes(ResourceTreeDir *dirNode, TreeEntry *dirEntry) {
    for (int i = 0; i < dirEntry->childrenCount; i++) {
        TreeEntry child = m_reader.readTreeEntry(dirEntry->firstChild+i);
        QString name = m_reader.readName(child);
        quint32 nameHash = m_reader.readHash(child);
        if (child.isDir()) {
            ResourceTreeDir *dir = new ResourceTreeDir(name, nameHash);
            appendChildNodes(dir, &child);
            dirNode->appendChild(dir);
        } else if (child.isZlib()) {
            ResourceTreeFile *file = new ZlibResourceTreeFile(name, nameHash, &m_reader, child.dataOffset);
            dirNode->appendChild(file);
        } else if (child.isZstd()) {
            ResourceTreeFile *file = new UnimplementedResourceTreeFile(name, nameHash);
            dirNode->appendChild(file);
        } else {
            ResourceTreeFile *file = new UncompressedResourceTreeFile(name, nameHash, &m_reader, child.dataOffset);
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

// uses binary search for fast finding child node with specified hash
ResourceTreeNode *ResourceLibrary::findChild(ResourceTreeDir *parent, quint32 searchHash, QString &error) {
    QList<ResourceTreeNode*> children = parent->children();
    if (children.isEmpty()) {
        error = "Node has no children";
        return nullptr;
    }
    int childrenCount = children.size();
    int firstChild = 0;

    ResourceTreeNode *node = children.at(firstChild+childrenCount/2);
    quint32 hash = node->nameHash();
    while (searchHash != hash) {
        if (childrenCount == 1) {
            // Hashes not compared and we have no other options
            error = "Child not found";
            return nullptr;
        }
        if (searchHash < hash)
            childrenCount /= 2;
        else {
            firstChild += childrenCount/2;
            childrenCount -= childrenCount/2;
        }
        node = children.at(firstChild+childrenCount/2);
        hash = node->nameHash();
    }
    return node;
}
ResourceTreeNode *ResourceLibrary::getNode(QString path, QString &error) {
    // User provided rcc styled path, no problem
    if (path.startsWith(":/")) path.remove(0, 2);
    // Find file
    QStringList pathSegments = path.split('/', Qt::SkipEmptyParts);

    ResourceTreeNode *node = &m_root;
    for (QString &segment : pathSegments) {
        if (!node->isDir()) {
            error = "Got file instead of dir";
            return nullptr;
        }
        ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
        node = findChild(dir, qt_hash(segment), error);
        if (!error.isEmpty()) return {};
    }
    return node;
}
