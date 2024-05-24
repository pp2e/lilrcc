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

    bool isDir() {
        return flags & Flags::Directory;
    }
};

class LilResourceReader {
public:
    LilResourceReader(QIODevice *device);

    const TreeEntry readTreeEntry(int entryNumber);
    QString readName(const TreeEntry &entry);
    quint32 readHash(const TreeEntry &entry);
    QByteArray readData(const TreeEntry &entry);

    quint32 overallFlags();

private:
    quint8 readNumber();
    quint16 readNumber2();
    quint32 readNumber4();

private:
    quint32 m_version;
    quint32 m_treeOffset;
    quint32 m_dataOffset;
    quint32 m_namesOffset;
    quint32 m_overallFlags;
    quint32 m_treeEntrySize;
    QIODevice *m_device;
};

#endif // LILRCCREADER_H
