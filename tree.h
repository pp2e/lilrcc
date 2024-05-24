#ifndef TREE_H
#define TREE_H

#include <QString>
#include <QList>

class ResourceTreeNode {
public:
    virtual ~ResourceTreeNode();
    QString name();
    virtual bool isDir()=0;

protected:
    QString m_name;
};

class ResourceTreeDir : public ResourceTreeNode {
public:
    ResourceTreeDir(QString name);
    ~ResourceTreeDir();

    bool isDir() override;
    bool appendChild(ResourceTreeNode *node);
    QList<ResourceTreeNode*> children();
private:
    QList<ResourceTreeNode*> m_children;
};

class ResourceTreeFile : public ResourceTreeNode {
public:
    ResourceTreeFile(QString name);

    bool isDir() override;
private:

};

#endif // TREE_H
