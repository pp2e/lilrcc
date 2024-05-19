#ifndef LILRCC_H
#define LILRCC_H

#include <QTextStream>
#include <QString>

class LilResourceLibrary {
    enum Flags
    {
        // must match qresource.cpp
        NoFlags = 0x00,
        Compressed = 0x01,
        Directory = 0x02,
        CompressedZstd = 0x04
    };

public:
    LilResourceLibrary(QIODevice *device);

    void printTree(QTextStream &out);
    bool getFile(QString path, QTextStream &out, QString &error);
    // bool rmFile(QString path, QTextStream &out);

private:
    void readHeader();
    quint8 readNumber();
    quint16 readNumber2();
    quint32 readNumber4();

    QString readName(quint32 offset);
    void printFileTree(int offset, QTextStream &out);

    quint32 findChild(quint32 parentOffset, quint32 searchHash, QString &error);
    quint32 getEntry(QString path, QString &error);

    QIODevice *m_device;
    quint32 m_version;
    quint32 m_treeOffset;
    quint32 m_dataOffset;
    quint32 m_namesOffset;
    quint32 m_overallFlags = 0;
    int m_treeEntrySize;
};

#endif // LILRCC_H
