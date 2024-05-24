#include "resourcereader.h"

#include <QDebug>

ResourceReader::ResourceReader(QIODevice *device) {
    m_device = device;

    if (m_device->read(4) != "qres") {
        qDebug() << "not rcc";
        return;
    }

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

const TreeEntry ResourceReader::readTreeEntry(int entryNumber) {
    m_device->seek(m_treeOffset + entryNumber*m_treeEntrySize);
    quint32 nameOffset = readNumber4();
    quint16 flags = readNumber2();
    if (flags & Flags::Directory) {
        quint32 childrenCount = readNumber4();
        quint32 firstChild = readNumber4();
        return {nameOffset, flags, childrenCount, firstChild, 0, 0, 0, 0};
    } else {
        quint16 language = readNumber2();
        quint16 territory = readNumber2();
        quint32 dataOffset = readNumber4();
        quint64 lastModified = readNumber8();
        return {nameOffset, flags, 0, 0, language, territory, dataOffset, lastModified};
    }
}

QString ResourceReader::readName(const TreeEntry &entry) {
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

quint32 ResourceReader::readHash(const TreeEntry &entry) {
    m_device->seek(m_namesOffset + entry.nameOffset + 2);
    return readNumber4();
}

QByteArray ResourceReader::readData(quint32 dataOffset) {
    m_device->seek(m_dataOffset + dataOffset);
    quint32 dataLength = readNumber4();
    return m_device->read(dataLength);
}

quint32 ResourceReader::overallFlags() {
    return m_overallFlags;
}
