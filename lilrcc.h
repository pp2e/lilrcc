#ifndef LILRCC_H
#define LILRCC_H

#include <QTextStream>
#include <QString>

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

class LilResourceLibrary {
public:
    LilResourceLibrary(QIODevice *device);

    void printTree(QTextStream &out);
    bool ls(QString path, QString &error);
    bool getFile(QString path, QTextStream &out, QString &error);
    void printAllFiles();
    bool rmFile(QString path, QString &error);
    void save(QTextStream &out);
private:
    // Various functions which move QIODevice read position, probably should be moved into separate class
    void readHeader();
    quint8 readNumber();
    quint16 readNumber2();
    quint32 readNumber4();
    const TreeEntry readTreeEntry(int entryNumber);
    QString readName(const TreeEntry &entry);
    quint32 readHash(const TreeEntry &entry);
    QByteArray readData(const TreeEntry &entry);

    void printDirTree(const TreeEntry &rootEntry, QTextStream &out);

    TreeEntry findChild(const TreeEntry &parent, quint32 searchHash, QString &error);
    TreeEntry getEntry(QString path, QString &error);

    QIODevice *m_device;
    quint32 m_version;
    quint32 m_treeOffset;
    quint32 m_dataOffset;
    quint32 m_namesOffset;
    quint32 m_overallFlags = 0;
    int m_treeEntrySize;
};

#endif // LILRCC_H
