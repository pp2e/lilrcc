#include "resourcereader.h"
#include "tree.h"

#include <QDebug>

ResourceReader::ResourceReader(QIODevice *device) {
    m_device = device;

    if (m_device->read(4) != "qres") {
        qWarning() << "File is not rcc";
        return;
    }

    m_readerData.version = readNumber4();
    m_readerData.treeOffset = readNumber4();
    m_readerData.dataOffset = readNumber4();
    m_readerData.namesOffset = readNumber4();

    if (m_readerData.version >= 3) {
        m_readerData.overallFlags = readNumber4();
    }

    // Calculate tree entry size
    m_readerData.treeEntrySize = 14;
    if (m_readerData.version >= 2)
        // Since version 2 rcc also have last modification date
        m_readerData.treeEntrySize += 8;
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
           + ((quint64)readNumber() << 32);
           + (readNumber() << 24)
           + (readNumber() << 16)
           + (readNumber() << 8)
           + (readNumber() << 0);
}

void ResourceReader::readTreeDirChildren(ResourceTreeDir *dirNode, int nodeNumber) {
    m_device->seek(m_readerData.treeOffset + nodeNumber*m_readerData.treeEntrySize+6);
    quint32 childrenCount = readNumber4();
    quint32 firstChild = readNumber4();

    for (int i = 0; i < childrenCount; i++) {
        m_device->seek(m_readerData.treeOffset + (firstChild+i)*m_readerData.treeEntrySize);
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
        m_device->seek(m_readerData.treeOffset + (firstChild+i)*m_readerData.treeEntrySize+10);
        quint32 dataOffset = readNumber4();
        if (flags & Flags::Compressed) {
            ResourceTreeFile *file = new ZlibResourceTreeFile(name, nameHash, this, dataOffset);
            dirNode->appendChild(file);
        } else if (flags & Flags::CompressedZstd) {
            ResourceTreeFile *file = new ZstdResourceTreeFile(name, nameHash, this, dataOffset);
            dirNode->appendChild(file);
        } else {
            ResourceTreeFile *file = new UncompressedResourceTreeFile(name, nameHash, this, dataOffset);
            dirNode->appendChild(file);
        }
    }
}

QString ResourceReader::readName(quint32 offset) {
    m_device->seek(m_readerData.namesOffset + offset);
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
    m_device->seek(m_readerData.namesOffset + offset + 2);
    return readNumber4();
}

QByteArray ResourceReader::readData(quint32 dataOffset) {
    m_device->seek(m_readerData.dataOffset + dataOffset);
    quint32 dataLength = readNumber4();
    return m_device->read(dataLength);
}

ReaderData ResourceReader::data() {
    return m_readerData;
}
