#ifndef LILRCC_H
#define LILRCC_H

#include "resourcereader.h"
#include "resourcewriter.h"
#include "tree.h"

#include <QTextStream>
#include <QString>

class ResourceLibrary {
public:
    ResourceLibrary(ResourceReader *reader);

    void printTree(QTextStream &out);
    bool ls(QString path, Lilrcc::Error &error);
    QByteArray getFile(QString path, Lilrcc::Error &error);
    bool rmFile(QString path, Lilrcc::Error &error);
    bool mvFile(QString source, QString dest, Lilrcc::Error &error);
    bool addFile(QByteArray data, QString name, QString dest, Lilrcc::Error &error);
    void save(ResourceWriter *writer);

private:
    void printDirTree(ResourceTreeDir *rootNode, QTextStream &out);

    static QStringList parsePath(QString path);

    ResourceTreeNode *binSearchNode(QList<ResourceTreeNode*> children, quint32 searchHash);
    ResourceTreeNode *getNode(QStringList path, Lilrcc::Error &error);

    ResourceTreeDir m_root;
};

#endif // LILRCC_H
