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
    out << ":/\n";

    m_device->seek(m_treeOffset+6);
    int childCount = readNumber4();
    int firstChildOffset = readNumber4();
    for (int i = 0; i < childCount; i++) {
        printFileTree(i+firstChildOffset, out);
    }
    if (m_overallFlags & Flags::CompressedZstd) {
        out << "Looks like some files was compessed with zstd. Zstd currently is not supported by lilrcc\n";
    }
}

bool LilResourceLibrary::getFile(QString path, QTextStream &out, QString &error) {
    quint32 offset = getEntry(path, error);
    if (!error.isEmpty()) return false;
    m_device->seek(m_treeOffset+m_treeEntrySize*offset+4);
    quint16 flags = readNumber2();
    m_device->skip(4);
    if (flags & Flags::Directory) {
        qWarning() << "File is not file (directory)";
        return false;
    } else if (flags & Flags::CompressedZstd) {
        qWarning() << "File encrypted zstd, cannot read";
        return false;
    }
    quint32 dataOffset = readNumber4();
    m_device->seek(m_dataOffset+dataOffset);
    quint32 dataLength = readNumber4();
    QByteArray data = m_device->read(dataLength);
    if (flags & Flags::Compressed) {
        QByteArray uncompressed = qUncompress(data);
        out << uncompressed;
        return true;
    }
    out << data;
    return true;
}

// bool LilResourceLibrary::rmFile(QString path, QTextStream &out) {
//     quint32 offset = getEntry(path, error);
//     if (!error.isEmpty()) return false;
//     quint16 flags = readNumber2(m_treeOffset+m_treeEntrySize*offset+4);
// }

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

QString LilResourceLibrary::readName(quint32 offset) {
    offset += m_namesOffset;
    m_device->seek(offset);
    quint16 nameLength = readNumber2();
    // Name hash, we dont need here
    m_device->skip(4);
    QString name;
    name.resize(nameLength);
    for (int i = 0; i < nameLength; i++)
        name[i] = readNumber2();
    return name;
}

QString tab = "  ";
void LilResourceLibrary::printFileTree(int offset, QTextStream &out) {
    m_device->seek(m_treeOffset+offset*m_treeEntrySize);
    int nameOffset = readNumber4();
    int flags = readNumber2();
    if (flags & Flags::Directory) {
        int childCount = readNumber4();
        int firstChildOffset = readNumber4();
        out << tab << readName(nameOffset) << "\n";
        for (int i = 0; i < childCount; i++) {
            tab += "  ";
            printFileTree(i+firstChildOffset, out);
            tab.chop(2);
        }
    } else {
        if (flags & Flags::CompressedZstd)
            out << tab << readName(nameOffset) << " - zstd, cannot read\n";
        else
            out << tab << readName(nameOffset) << "\n";
    }
}

// uses binary search for fast finding child node with specified hash
quint32 LilResourceLibrary::findChild(quint32 parentOffset, quint32 searchHash, QString &error) {
    m_device->seek(m_treeOffset+parentOffset*m_treeEntrySize+6);
    int childCount = readNumber4();
    if (childCount == 0) {
        error = "Node has no children";
        return 0;
    }
    int childOffset = readNumber4();

    m_device->seek(m_treeOffset+(childOffset+childCount/2)*m_treeEntrySize);
    int nameOffset = readNumber4();
    m_device->seek(m_namesOffset+nameOffset+2);
    quint32 hash = readNumber4();
    while (childCount != 1) {
        if (searchHash == hash)
            return childOffset+childCount/2;
        else if (searchHash < hash)
            childCount /= 2;
        else {
            childOffset += childCount/2;
            childCount -= childCount/2;
        }
        m_device->seek(m_treeOffset+(childOffset+childCount/2)*m_treeEntrySize);
        int nameOffset = readNumber4();
        m_device->seek(m_namesOffset+nameOffset+2);
        hash = readNumber4();
    }
    if (searchHash != hash) {
        error = "Child not found";
        return 0;
    }
    return childOffset;
}

quint32 LilResourceLibrary::getEntry(QString path, QString &error) {
    // User provided rcc styled path, no problem
    if (path.startsWith(":/")) path.remove(0, 2);
    // Find file
    QStringList pathSegments = path.split('/', Qt::SkipEmptyParts);

    int offset = 0;
    for (QString &segment : pathSegments) {
        offset = findChild(offset, qt_hash(segment), error);
        if (!error.isEmpty()) return 0;
    }
    return offset;
}
