#include "lilrcc.h"

#include <QDebug>
#include <QIODevice>

LilResourceLibrary::LilResourceLibrary(QIODevice *device)
    : m_reader({device})
    , m_root(":")
{

    TreeEntry root = m_reader.readTreeEntry(0);
    appendChildNodes(&m_root, &root);

}

void LilResourceLibrary::printTree(QTextStream &out) {
    out << m_root.name() << "\n";
    printDirTree(&m_root, out);
    if (m_reader.overallFlags() & Flags::CompressedZstd) {
        out << "Looks like some files was compessed with zstd. Zstd currently is not supported by lilrcc\n";
    }
}

bool LilResourceLibrary::ls(QString path, QString &error) {
    for (ResourceTreeNode* node : m_root.children()) {
        qDebug(qPrintable(node->name() + (node->isDir() ? "/" : "")));
    }
    // const TreeEntry entry = getEntry(path, error);
    // if (!error.isEmpty()) return false;
    // if (!(entry.flags & Flags::Directory)) {
    //     error = "Tree entry is file";
    //     return false;
    // }
    // for (int i = 0; i < entry.childrenCount; i++) {
    //     qDebug() << m_reader.readName(m_reader.readTreeEntry(entry.firstChild+i));
    // }
    return true;
}

bool LilResourceLibrary::getFile(QString path, QTextStream &out, QString &error) {
    const TreeEntry entry = getEntry(path, error);
    if (!error.isEmpty()) return false;
    if (entry.flags & Flags::Directory) {
        qWarning() << "File is not file (directory)";
        return false;
    } else if (entry.flags & Flags::CompressedZstd) {
        qWarning() << "File encrypted zstd, cannot read";
        return false;
    }

    QByteArray data = m_reader.readData(entry);
    if (entry.flags & Flags::Compressed) {
        QByteArray uncompressed = qUncompress(data);
        out << uncompressed;
        return true;
    }
    out << data;
    return true;
}

void LilResourceLibrary::printAllFiles() {
    TreeEntry root = m_reader.readTreeEntry(0);
    int children = root.childrenCount;
    int i = 0;
    while (i < children) {
        TreeEntry entry = m_reader.readTreeEntry(++i);
        if (!entry.isDir())
            qDebug() << entry.dataOffset << m_reader.readName(entry);
        children += entry.childrenCount;
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

void LilResourceLibrary::appendChildNodes(ResourceTreeDir *dirNode, TreeEntry *dirEntry) {
    for (int i = 0; i < dirEntry->childrenCount; i++) {
        TreeEntry child = m_reader.readTreeEntry(dirEntry->firstChild+i);
        QString name = m_reader.readName(child);
        if (child.isDir()) {
            ResourceTreeDir *dir = new ResourceTreeDir(name);
            appendChildNodes(dir, &child);
            dirNode->appendChild(dir);
        } else {
            ResourceTreeFile *file = new ResourceTreeFile(name);
            dirNode->appendChild(file);
        }
    }
}

QString tab = "";
void LilResourceLibrary::printDirTree(ResourceTreeDir *rootNode, QTextStream &out) {
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
TreeEntry LilResourceLibrary::findChild(const TreeEntry &parent, quint32 searchHash, QString &error) {
    if (parent.childrenCount == 0) {
        error = "Node has no children";
        return {};
    }
    int childrenCount = parent.childrenCount;
    int firstChild = parent.firstChild;

    TreeEntry entry = m_reader.readTreeEntry(firstChild+childrenCount/2);
    quint32 hash = m_reader.readHash(entry);
    while (searchHash != hash) {
        if (childrenCount == 1) {
            // Hashes not compared and we have no other options
            error = "Child not found";
            return {};
        }
        if (searchHash < hash)
            childrenCount /= 2;
        else {
            firstChild += childrenCount/2;
            childrenCount -= childrenCount/2;
        }
        entry = m_reader.readTreeEntry(firstChild+childrenCount/2);
        hash = m_reader.readHash(entry);
    }
    return entry;
}

TreeEntry LilResourceLibrary::getEntry(QString path, QString &error) {
    // User provided rcc styled path, no problem
    if (path.startsWith(":/")) path.remove(0, 2);
    // Find file
    QStringList pathSegments = path.split('/', Qt::SkipEmptyParts);

    TreeEntry entry = m_reader.readTreeEntry(0);
    for (QString &segment : pathSegments) {
        entry = findChild(entry, qt_hash(segment), error);
        if (!error.isEmpty()) return {};
    }
    return entry;
}
