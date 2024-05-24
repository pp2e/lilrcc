#ifndef LILRCC_H
#define LILRCC_H

#include <QTextStream>
#include <QString>

#include "resourcereader.h"
#include "tree.h"

class ResourceLibrary {
public:
    ResourceLibrary(QIODevice *device);

    void printTree(QTextStream &out);
    bool ls(QString path, QString &error);
    bool getFile(QString path, QTextStream &out, QString &error);
    void printAllFiles();
    // bool rmFile(QString path, QString &error);
    // void save(QTextStream &out);
private:
    void appendChildNodes(ResourceTreeDir *dirNode, TreeEntry *dirEntry);
    void printDirTree(ResourceTreeDir *rootNode, QTextStream &out);

    ResourceTreeNode *findChild(ResourceTreeDir *parent, quint32 searchHash, QString &error);
    ResourceTreeNode *getNode(QString path, QString &error);

    ResourceReader m_reader;
    ResourceTreeDir m_root;
};

#endif // LILRCC_H
