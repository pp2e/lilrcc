#ifndef RESOURCEWRITER_H
#define RESOURCEWRITER_H

#include <QIODevice>

class ResourceWriter {
public:
    ResourceWriter(QIODevice *device);

private:
    QIODevice *m_device;
};

#endif // RESOURCEWRITER_H
