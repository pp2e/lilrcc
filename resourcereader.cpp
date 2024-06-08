#include "resourcereader.h"
#include "tree.h"

ResourceReader::ResourceReader(QIODevice *device) {
    m_device = device;

    if (m_device->read(4) != "qres") {
        m_error = Lilrcc::InputFileIsNotRcc;
        return;
    }

    m_version = readNumber4();
    m_treeOffset = readNumber4();
    m_dataOffset = readNumber4();
    m_namesOffset = readNumber4();

    if (m_version >= 3) {
        m_overallFlags = readNumber4();
    }

    // Calculate tree entry size
    m_treeEntrySize = 14;
    if (m_version >= 2)
        // Since version 2 rcc also have last modification date
        m_treeEntrySize += 8;
}

Lilrcc::Error ResourceReader::error() {
    return m_error;
}

quint8 ResourceReader::readNumber() {
    char out;
    m_device->getChar(&out);
    return out;
}

quint16 ResourceReader::readNumber2() {
    return (readNumber() << 8)
           + (readNumber() << 0);
}

quint32 ResourceReader::readNumber4() {
    return (readNumber() << 24)
           + (readNumber() << 16)
           + (readNumber() << 8)
           + (readNumber() << 0);
}

quint64 ResourceReader::readNumber8() {
    return ((quint64)readNumber() << 54)
           + ((quint64)readNumber() << 48)
           + ((quint64)readNumber() << 40)
           + ((quint64)readNumber() << 32)
           + (readNumber() << 24)
           + (readNumber() << 16)
           + (readNumber() << 8)
           + (readNumber() << 0);
}

void ResourceReader::readTreeDirChildren(ResourceTreeDir *dirNode, int nodeNumber) {
    m_device->seek(m_treeOffset + nodeNumber*m_treeEntrySize+6);
    quint32 childrenCount = readNumber4();
    quint32 firstChild = readNumber4();

    for (int i = 0; i < childrenCount; i++) {
        m_device->seek(m_treeOffset + (firstChild+i)*m_treeEntrySize);
        quint32 nameOffset = readNumber4();
        quint16 flags = readNumber2();

        QString name = readName(nameOffset);
        quint32 nameHash = readHash(nameOffset);
        if (flags & Flags::Directory) {
            ResourceTreeDir *dir = new ResourceTreeDir(name, nameHash);
            readTreeDirChildren(dir, firstChild+i);
            dirNode->appendChild(dir);
            continue;
        }

        // file, not dir
        m_device->seek(m_treeOffset + (firstChild+i)*m_treeEntrySize+10);
        quint32 dataOffset = readNumber4();
        m_device->seek(m_dataOffset + dataOffset);
        quint32 dataSize = 4+readNumber4();
        if (flags & Flags::Compressed) {
            ResourceTreeFile *file = new ZlibResourceTreeFile(name, nameHash, this, dataOffset, dataSize);
            dirNode->appendChild(file);
        } else if (flags & Flags::CompressedZstd) {
            ResourceTreeFile *file = new ZstdResourceTreeFile(name, nameHash, this, dataOffset, dataSize);
            dirNode->appendChild(file);
        } else {
            ResourceTreeFile *file = new UncompressedResourceTreeFile(name, nameHash, this, dataOffset, dataSize);
            dirNode->appendChild(file);
        }
    }
}

QString ResourceReader::readName(quint32 offset) {
    m_device->seek(m_namesOffset + offset);
    quint16 nameLength = readNumber2();
    // Name hash, we dont need here
    m_device->skip(4);
    QString name;
    name.resize(nameLength);
    for (int i = 0; i < nameLength; i++)
        name[i] = readNumber2();
    return name;
}

quint32 ResourceReader::readHash(quint32 offset) {
    m_device->seek(m_namesOffset + offset + 2);
    return readNumber4();
}

QByteArray ResourceReader::readData(quint32 dataOffset) {
    m_device->seek(m_dataOffset + dataOffset);
    quint32 dataLength = readNumber4();
    return m_device->read(dataLength);
}

void ResourceReader::printHeader(QTextStream &out) {
    out << "Version: " << m_version << "\n";
    out << "Tree: " << m_treeOffset << "\n";
    out << "Data: " << m_dataOffset << "\n";
    out << "Names: " << m_namesOffset << "\n";
    if (m_version >= 3)
        out << "OverallFlags: " << m_overallFlags << "\n";
}

void ResourceReader::printEntries(QTextStream &out) {
    m_device->seek(m_treeOffset);
    int pending = 1;
    while (pending > 0) {
        pending--;
        quint32 nameOffset = readNumber4();
        out << "Name: " << nameOffset;
        quint16 flags = readNumber2();
        if (flags & Flags::Directory) {
            quint32 children = readNumber4();
            out << " Children: " << children;
            pending += children;
            readNumber4(); // useless offset
        } else {
            out << " Language: " << readNumber2();
            out << " Territory: " << readNumber2();
            out << " Data: " << readNumber4();
        }
        if (m_version >= 2)
            readNumber8();
        out << "\n";
    }
}

void ResourceReader::printNames(QTextStream &out) {
    quint32 namesSize = m_treeOffset - m_namesOffset;
    m_device->seek(m_namesOffset);
    quint32 offset = 0;
    while (offset < namesSize) {
        quint16 nameSize = readNumber2();
        quint32 nameHash = readNumber4();
        out << offset << ": ";
        for (int i = 0; i < nameSize; i++)
            out << QChar(readNumber2());
        out << "\n";
        offset += 2+4+2*nameSize;
    }
}
