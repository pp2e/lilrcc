#include "lilrcc.h"

#include <QDebug>
#include <QIODevice>

LilResourceLibrary::LilResourceLibrary(QIODevice *device)
    : m_device(device) {
    if (!m_device->reset()) {
        qDebug() << "Cannot reset";
    }
    if (m_device->read(4) != "qres") {
        qDebug() << "not rcc";
        return;
    }
    readHeader();
}

void LilResourceLibrary::printTree(QTextStream &out) {
    // Root node is automatically directory, right?
    out << ":\n";

    TreeEntry rootEntry = readTreeEntry(0);
    printDirTree(rootEntry, out);
    if (m_overallFlags & Flags::CompressedZstd) {
        out << "Looks like some files was compessed with zstd. Zstd currently is not supported by lilrcc\n";
    }
}

bool LilResourceLibrary::ls(QString path, QString &error) {
    const TreeEntry entry = getEntry(path, error);
    if (!error.isEmpty()) return false;
    if (!(entry.flags & Flags::Directory)) {
        error = "Tree entry is file";
        return false;
    }
    for (int i = 0; i < entry.childrenCount; i++) {
        qDebug() << readName(readTreeEntry(entry.firstChild+i));
    }
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

    QByteArray data = readData(entry);
    if (entry.flags & Flags::Compressed) {
        QByteArray uncompressed = qUncompress(data);
        out << uncompressed;
        return true;
    }
    out << data;
    return true;
}

void LilResourceLibrary::printAllFiles() {
    TreeEntry root = readTreeEntry(0);
    int children = root.childrenCount;
    int i = 0;
    while (i < children) {
        TreeEntry entry = readTreeEntry(++i);
        if (!entry.isDir())
            qDebug() << entry.dataOffset << readName(entry);
        children += entry.childrenCount;
    }
}

bool LilResourceLibrary::rmFile(QString path, QString &error) {
    const TreeEntry entry = getEntry(path, error);
    if (!error.isEmpty()) return false;
    qDebug() << readName(entry);
    return false;
}

void LilResourceLibrary::save(QTextStream &out) {
    out << "123";
}

void LilResourceLibrary::readHeader() {
    m_version = readNumber4();
    m_treeOffset = readNumber4();
    m_dataOffset = readNumber4();
    m_namesOffset = readNumber4();
    // Calculate tree entry size
    m_treeEntrySize = 14;
    if (m_version >= 2)
        // Since version 2 rcc also have last modification date
        m_treeEntrySize += 8;

    if (m_version >= 3) {
        m_overallFlags = readNumber4();
    }
}

quint8 LilResourceLibrary::readNumber() {
    char out;
    m_device->getChar(&out);
    return out;
}

quint16 LilResourceLibrary::readNumber2() {
    return (readNumber() << 8)
           + (readNumber() << 0);
}

quint32 LilResourceLibrary::readNumber4() {
    return (readNumber() << 24)
           + (readNumber() << 16)
           + (readNumber() << 8)
           + (readNumber() << 0);
}

const TreeEntry LilResourceLibrary::readTreeEntry(int entryNumber) {
    m_device->seek(m_treeOffset + entryNumber*m_treeEntrySize);
    quint32 nameOffset = readNumber4();
    quint16 flags = readNumber2();
    if (flags & Flags::Directory) {
        quint32 childrenCount = readNumber4();
        quint32 firstChild = readNumber4();
        return {nameOffset, flags, childrenCount, firstChild, 0, 0, 0};
    } else {
        quint16 language = readNumber2();
        quint16 territory = readNumber2();
        quint32 dataOffset = readNumber4();
        return {nameOffset, flags, 0, 0, language, territory, dataOffset};
    }
}

QString LilResourceLibrary::readName(const TreeEntry &entry) {
    m_device->seek(m_namesOffset + entry.nameOffset);
    quint16 nameLength = readNumber2();
    // Name hash, we dont need here
    m_device->skip(4);
    QString name;
    name.resize(nameLength);
    for (int i = 0; i < nameLength; i++)
        name[i] = readNumber2();
    return name;
}

quint32 LilResourceLibrary::readHash(const TreeEntry &entry) {
    m_device->seek(m_namesOffset + entry.nameOffset + 2);
    return readNumber4();
}

QByteArray LilResourceLibrary::readData(const TreeEntry &entry) {
    m_device->seek(m_dataOffset + entry.dataOffset);
    quint32 dataLength = readNumber4();
    return m_device->read(dataLength);
}

QString tab = "";
void LilResourceLibrary::printDirTree(const TreeEntry &rootEntry, QTextStream &out) {
    for (int i = 0; i < rootEntry.childrenCount; i++) {
        bool last = i == rootEntry.childrenCount-1;
        const TreeEntry entry = readTreeEntry(rootEntry.firstChild+i);
        out << tab << (last ? "└── " : "├── ") << readName(entry);
        if (entry.flags & Flags::Directory) {
            out << "\n";
            tab += last ? "    " : "│   ";
            printDirTree(entry, out);
            tab.chop(4);
        } else {
            if (entry.flags & Flags::CompressedZstd)
                out << " - zstd, cannot read";
            out << "\n";
        }
//        if (entry.flags & Flags::Directory) {
//            printDirTree(entry, out);
//            tab.chop(4);
//        } else {
//            out << tab << (last ? "    " : "│   ");
//            if (entry.flags & Flags::CompressedZstd)
//                out << tab << "├── " << readName(entry) << " - zstd, cannot read\n";
//            else
//                out << tab << "├── " << readName(entry) << "\n";
//        }
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

    TreeEntry entry = readTreeEntry(firstChild+childrenCount/2);
    quint32 hash = readHash(entry);
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
        entry = readTreeEntry(firstChild+childrenCount/2);
        hash = readHash(entry);
    }
    return entry;
}

TreeEntry LilResourceLibrary::getEntry(QString path, QString &error) {
    // User provided rcc styled path, no problem
    if (path.startsWith(":/")) path.remove(0, 2);
    // Find file
    QStringList pathSegments = path.split('/', Qt::SkipEmptyParts);

    TreeEntry entry = readTreeEntry(0);
    for (QString &segment : pathSegments) {
        entry = findChild(entry, qt_hash(segment), error);
        if (!error.isEmpty()) return {};
    }
    return entry;
}
