#include "tree.h"

ResourceTreeNode::~ResourceTreeNode() {}

QString ResourceTreeNode::name() {
    return m_name;
}

// Dir
ResourceTreeDir::ResourceTreeDir(QString name) {
    m_name = name;
}

ResourceTreeDir::~ResourceTreeDir() {
    qDeleteAll(m_children);
}

bool ResourceTreeDir::isDir() {
    return true;
}

bool ResourceTreeDir::appendChild(ResourceTreeNode *node) {
    m_children.append(node);
    return true;
}

QList<ResourceTreeNode *> ResourceTreeDir::children() {
    return m_children;
}

// File
ResourceTreeFile::ResourceTreeFile(QString name) {
    m_name = name;
}

bool ResourceTreeFile::isDir() {
    return false;
}
