#include "resourcewriter.h"
#include "tree.h"

#include <QDebug>

ResourceWriter::ResourceWriter(QIODevice *device) {
    m_device = device;
}

void ResourceWriter::write(ResourceTreeDir *dir, quint32 version) {
    m_version = version;
    writeHeader();
    m_dataOffset = 20;
    if (m_version >= 3) m_dataOffset += 4;
    // data
    quint32 dataSize = writeData(dir);
    m_namesOffset = m_dataOffset + dataSize;
    // names
    quint32 namesSize = writeNames(dir);
    m_treeOffset = m_namesOffset + namesSize;
    // tree
    m_overallFlags = 0;
    writeDataTree(dir);
    rewriteHeader();
}

void ResourceWriter::writeNumber(quint8 number) {
    m_device->write((char*)&number, 1);
}

void ResourceWriter::writeNumber2(quint16 number) {
    writeNumber(number >> 8);
    writeNumber(number >> 0);
}

void ResourceWriter::writeNumber4(quint32 number) {
    writeNumber(number >> 24);
    writeNumber(number >> 16);
    writeNumber(number >> 8);
    writeNumber(number >> 0);
}

void ResourceWriter::writeNumber8(quint64 number) {
    writeNumber(number >> 56);
    writeNumber(number >> 48);
    writeNumber(number >> 40);
    writeNumber(number >> 32);
    writeNumber(number >> 24);
    writeNumber(number >> 16);
    writeNumber(number >> 8);
    writeNumber(number >> 0);
}

void ResourceWriter::writeHeader() {
    m_device->write("qres");
    writeNumber4(m_version);
    // write zeroes to rewrite later
    writeNumber4(0); // tree offset
    writeNumber4(0); // data offset
    writeNumber4(0); // name offset
    if (m_version >= 3)
        writeNumber4(0); // overall flags
}

quint32 ResourceWriter::writeData(ResourceTreeDir *dir) {
    QList<ResourceTreeDir*> pending;
    pending << dir;
    int dataOffset = 0;
    while (!pending.isEmpty()) {
        ResourceTreeDir *dir = pending.takeFirst();
        for (ResourceTreeNode *child : dir->children()) {
            if (child->isDir())
                pending << static_cast<ResourceTreeDir*>(child);
            else {
                ResourceTreeFile *file = static_cast<ResourceTreeFile*>(child);
                qDebug() << "Wrote" << file->name() << m_dataOffset + dataOffset;
                QByteArray data = file->getCompressed();
                writeNumber4(data.size());
                m_device->write(data);
                dataOffset += 4 + data.size();
            }
        }
    }
    return dataOffset;
}

quint32 ResourceWriter::writeName(QString name) {
    writeNumber2(name.size());
    writeNumber4(qt_hash(name));
    for (int i = 0; i < name.size(); i++)
        writeNumber2(name[i].unicode());
    return name.size()*2+6;
}

quint32 ResourceWriter::writeNames(ResourceTreeDir *dir) {
    QList<ResourceTreeDir*> pending;
    pending << dir;
    int nameOffset = 0;
    while (!pending.isEmpty()) {
        ResourceTreeDir *dir = pending.takeFirst();
        for (ResourceTreeNode *child : dir->children()) {
            if (child->isDir())
                pending << static_cast<ResourceTreeDir*>(child);

            QString name = child->name();
            if (!m_names.contains(name)) {
                m_names.insert(name, nameOffset);
                nameOffset += writeName(name);
            }
        }
    }
    return nameOffset;
}

void ResourceWriter::writeDataTree(ResourceTreeDir *dir) {
    QList<ResourceTreeNode*> pending;
    pending << dir;
    int nodesCount = 1;
    while (!pending.isEmpty()) {
        ResourceTreeNode *node = pending.takeFirst();
        writeNumber4(m_names.value(node->name()));
        if (node->isDir()) {
            // Dir flag
            writeNumber2(Flags::Directory);
            ResourceTreeDir *dir = static_cast<ResourceTreeDir*>(node);
            writeNumber4(dir->children().size());
            writeNumber4(nodesCount);
            pending << dir->children();
            nodesCount += dir->children().size();
        } else {
            ResourceTreeFile *file = static_cast<ResourceTreeFile*>(node);
            Compression compr = file->getCompression();
            m_overallFlags |= compr;
            writeNumber2(compr);
            writeNumber2(0);
            writeNumber2(0);
            writeNumber4(0);
        }
        if (m_version >= 2)
            writeNumber8(0);
    }
}

void ResourceWriter::rewriteHeader() {
    m_device->seek(8);
    writeNumber4(m_treeOffset);
    writeNumber4(m_dataOffset);
    writeNumber4(m_namesOffset);
    if (m_version >= 3)
        writeNumber4(m_overallFlags);
}
