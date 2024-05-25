#ifndef LILRCCREADER_H
#define LILRCCREADER_H

#include <QString>
#include <QIODevice>

enum Flags {
    // must match qresource.cpp and rcc.h
    NoFlags = 0x00,
    Compressed = 0x01,
    Directory = 0x02,
    CompressedZstd = 0x04
};

struct TreeEntry {
    quint32 nameOffset;
    quint16 flags;
    // directory stuff
    quint32 childrenCount;
    quint32 firstChild;
    // file stuff
    quint16 language;
    quint16 terrirory;
    quint32 dataOffset;
    quint64 lastModified;

    bool isDir() {
        return flags & Flags::Directory;
    }
    bool isZlib() {
        return flags & Flags::Compressed;
    }
    bool isZstd() {
        return flags & Flags::CompressedZstd;
    }
};

struct ReaderData {
    quint32 version;
    quint32 treeOffset;
    quint32 dataOffset;
    quint32 namesOffset;
    quint32 overallFlags;
    quint32 treeEntrySize;
};

class ResourceReader {
public:
    ResourceReader(QIODevice *device);

    const TreeEntry readTreeEntry(int entryNumber);
    QString readName(const TreeEntry &entry);
    quint32 readHash(const TreeEntry &entry);
    QByteArray readData(quint32 dataOffset);
    ReaderData data();

private:
    quint8 readNumber();
    quint16 readNumber2();
    quint32 readNumber4();
    quint64 readNumber8();

private:
    ReaderData m_readerData;
    QIODevice *m_device;
};

#endif // LILRCCREADER_H
