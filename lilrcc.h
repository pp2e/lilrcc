#ifndef LILRCC_H
#define LILRCC_H

#include <QTextStream>
#include <QString>

#include "resourcereader.h"
#include "resourcewriter.h"
#include "tree.h"

class ResourceLibrary {
public:
    ResourceLibrary(ResourceReader *reader);

    void getHeader(QTextStream &out);
    void printTree(QTextStream &out);
    bool ls(QString path, QString &error);
    bool getFile(QString path, QTextStream &out, QString &error);
    void printAllFiles();
    bool rmFile(QString path, QString &error);
    void save(ResourceWriter *writer);

private:
    void printDirTree(ResourceTreeDir *rootNode, QTextStream &out);

    static QStringList parsePath(QString path);

    ResourceTreeNode *binSearchNode(QList<ResourceTreeNode*> children, quint32 searchHash);
    ResourceTreeNode *getNode(QStringList path, QString &error);

    ReaderData m_readerData;

    ResourceTreeDir m_root;
};

#endif // LILRCC_H
