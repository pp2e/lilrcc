#ifndef LILRCC_H
#define LILRCC_H

#include <QTextStream>
#include <QString>

#include "lilrccreader.h"
#include "tree.h"

class LilResourceLibrary {
public:
    LilResourceLibrary(QIODevice *device);

    void printTree(QTextStream &out);
    bool ls(QString path, QString &error);
    bool getFile(QString path, QTextStream &out, QString &error);
    void printAllFiles();
    // bool rmFile(QString path, QString &error);
    // void save(QTextStream &out);
private:
    void appendChildNodes(ResourceTreeDir *dirNode, TreeEntry *dirEntry);
    void printDirTree(ResourceTreeDir *rootNode, QTextStream &out);

    TreeEntry findChild(const TreeEntry &parent, quint32 searchHash, QString &error);
    TreeEntry getEntry(QString path, QString &error);

    LilResourceReader m_reader;
    ResourceTreeDir m_root;
};

#endif // LILRCC_H
