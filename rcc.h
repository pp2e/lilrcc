// Copyright (C) 2018 The Qt Company Ltd.
// Copyright (C) 2018 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

// Note: A copy of this file is used in Qt Designer (qttools/src/designer/src/lib/shared/rcc_p.h)

#ifndef RCC_H
#define RCC_H

#include <QStringList>
#include <QHash>
#include <QString>

typedef struct ZSTD_CCtx_s ZSTD_CCtx;

class RCCFileInfo;
class QIODevice;
class QTextStream;


class RCCResourceLibrary
{
    RCCResourceLibrary(const RCCResourceLibrary &);
    RCCResourceLibrary &operator=(const RCCResourceLibrary &);

public:
    RCCResourceLibrary(quint8 formatVersion);
    ~RCCResourceLibrary();

    bool output(QIODevice &outDevice, QIODevice &errorDevice);

    bool readFiles(QIODevice &errorDevice);

    void setInputFiles(const QStringList &files) { m_fileNames = files; }
    QStringList inputFiles() const { return m_fileNames; }

    void setVerbose(bool b) { m_verbose = b; }
    bool verbose() const { return m_verbose; }

    // void setInitName(const QString &name) { m_initName = name; }
    // QString initName() const { return m_initName; }

    void setOutputName(const QString &name) { m_outputName = name; }
    QString outputName() const { return m_outputName; }

    enum class CompressionAlgorithm {
        Zlib,
        Zstd,

        Best = 99,
        None = -1
    };

    static CompressionAlgorithm parseCompressionAlgorithm(QStringView algo, QString *errorMsg);
    void setCompressionAlgorithm(CompressionAlgorithm algo) { m_compressionAlgo = algo; }
    CompressionAlgorithm compressionAlgorithm() const { return m_compressionAlgo; }

    static int parseCompressionLevel(CompressionAlgorithm algo, const QString &level, QString *errorMsg);
    void setCompressLevel(int c) { m_compressLevel = c; }
    int compressLevel() const { return m_compressLevel; }

    void setCompressThreshold(int t) { m_compressThreshold = t; }
    int compressThreshold() const { return m_compressThreshold; }

    void setResourceRoot(const QString &root) { m_resourceRoot = root; }
    QString resourceRoot() const { return m_resourceRoot; }

    void setUseNameSpace(bool v) { m_useNameSpace = v; }
    bool useNameSpace() const { return m_useNameSpace; }

    QStringList failedResources() const { return m_failedResources; }

    int formatVersion() const { return m_formatVersion; }

    void setNoZstd(bool v) { m_noZstd = v; }
    bool noZstd() const { return m_noZstd; }

private:
    struct Strings {
        Strings();
        const QString TAG_RCC;
        const QString TAG_RESOURCE;
        const QString TAG_FILE;
        const QString ATTRIBUTE_LANG;
        const QString ATTRIBUTE_PREFIX;
        const QString ATTRIBUTE_ALIAS;
        const QString ATTRIBUTE_EMPTY;
        const QString ATTRIBUTE_THRESHOLD;
        const QString ATTRIBUTE_COMPRESS;
        const QString ATTRIBUTE_COMPRESSALGO;
    };
    friend class RCCFileInfo;
    void reset();
    bool addFile(const QString &alias, RCCFileInfo file);
    bool interpretResourceFile(QIODevice *inputDevice, const QString &file,
        QString currentPath = QString(), bool listMode = false);
    bool writeHeader();
    bool writeDataBlobs();
    bool writeDataNames();
    bool writeDataStructure();
    bool writeInitializer();
    void writeNumber2(quint16 number);
    void writeNumber4(quint32 number);
    void writeNumber8(quint64 number);
    void writeChar(char c) { m_out.append(c); }
    void writeByteArray(const QByteArray &);
    void write(const char *, int len);
    // void writeString(const char *s) { write(s, static_cast<int>(strlen(s))); }
    void writeString(const char *s);

#if QT_CONFIG(zstd)
    ZSTD_CCtx *m_zstdCCtx;
#endif

    const Strings m_strings;
    RCCFileInfo *m_root;
    QStringList m_fileNames;
    QString m_resourceRoot;
    // QString m_initName;
    QString m_outputName;
    bool m_verbose;
    CompressionAlgorithm m_compressionAlgo;
    int m_compressLevel;
    int m_compressThreshold;
    int m_treeOffset;
    int m_namesOffset;
    int m_dataOffset;
    quint32 m_overallFlags;
    bool m_useNameSpace;
    QStringList m_failedResources;
    QIODevice *m_errorDevice;
    QIODevice *m_outDevice;
    QByteArray m_out;
    quint8 m_formatVersion;
    bool m_noZstd;
};

#endif // RCC_H
