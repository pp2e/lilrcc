// Copyright (C) 2018 The Qt Company Ltd.
// Copyright (C) 2018 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "rcc.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QLocale>
#include <QStack>
#include <qxmlstream.h>

#include <algorithm>

#if QT_CONFIG(zstd)
#  include <zstd.h>
#endif

// Note: A copy of this file is used in Qt Designer (qttools/src/designer/src/lib/shared/rcc.cpp)

using namespace Qt::StringLiterals;

enum {
    CONSTANT_USENAMESPACE = 1,
    CONSTANT_COMPRESSLEVEL_DEFAULT = -1,
    CONSTANT_ZSTDCOMPRESSLEVEL_CHECK = 1,   // Zstd level to check if compressing is a good idea
    CONSTANT_ZSTDCOMPRESSLEVEL_STORE = 14,  // Zstd level to actually store the data
    CONSTANT_COMPRESSTHRESHOLD_DEFAULT = 70
};

void RCCResourceLibrary::write(const char *str, int len)
{
    int n = m_out.size();
    m_out.resize(n + len);
    memcpy(m_out.data() + n, str, len);
}

void RCCResourceLibrary::writeByteArray(const QByteArray &other)
{
    m_out.append(other);
}

static inline QString msgOpenReadFailed(const QString &fname, const QString &why)
{
    return QString::fromLatin1("Unable to open %1 for reading: %2\n").arg(fname, why);
}


///////////////////////////////////////////////////////////
//
// RCCFileInfo
//
///////////////////////////////////////////////////////////

class RCCFileInfo
{
public:
    enum Flags
    {
        // must match qresource.cpp
        NoFlags = 0x00,
        Compressed = 0x01,
        Directory = 0x02,
        CompressedZstd = 0x04
    };


    RCCFileInfo() = default;
    RCCFileInfo(const QString &name, const QFileInfo &fileInfo, QLocale::Language language,
                QLocale::Territory territory, uint flags,
                RCCResourceLibrary::CompressionAlgorithm compressAlgo, int compressLevel,
                int compressThreshold, bool noZstd, bool isEmpty);

    ~RCCFileInfo();
    RCCFileInfo(const RCCFileInfo &) = delete;
    RCCFileInfo &operator=(const RCCFileInfo &) = delete;
    RCCFileInfo(RCCFileInfo &&) = default;
    RCCFileInfo &operator=(RCCFileInfo &&other) = delete;

    QString resourceName() const;

public:
    qint64 writeDataBlob(RCCResourceLibrary &lib, qint64 offset, QString *errorMessage);
    qint64 writeDataName(RCCResourceLibrary &, qint64 offset);
    void writeDataInfo(RCCResourceLibrary &lib);

    int m_flags = NoFlags;
    QLocale::Language m_language = QLocale::C;
    QLocale::Territory m_territory = QLocale::AnyTerritory;
    QString m_name;
    QFileInfo m_fileInfo;
    RCCFileInfo *m_parent = nullptr;
    QMultiHash<QString, RCCFileInfo *> m_children;

    RCCResourceLibrary::CompressionAlgorithm m_compressAlgo = RCCResourceLibrary::CompressionAlgorithm::Best;
    int m_compressLevel = CONSTANT_COMPRESSLEVEL_DEFAULT;
    int m_compressThreshold = CONSTANT_COMPRESSTHRESHOLD_DEFAULT;
    bool m_noZstd = false;
    bool m_isEmpty = false;

    qint64 m_nameOffset = 0;
    qint64 m_dataOffset = 0;
    qint64 m_childOffset = 0;
};

RCCFileInfo::RCCFileInfo(const QString &name, const QFileInfo &fileInfo, QLocale::Language language,
                         QLocale::Territory territory, uint flags,
                         RCCResourceLibrary::CompressionAlgorithm compressAlgo, int compressLevel,
                         int compressThreshold, bool noZstd, bool isEmpty)
    : m_flags(flags),
      m_language(language),
      m_territory(territory),
      m_name(name),
      m_fileInfo(fileInfo),
      m_compressAlgo(compressAlgo),
      m_compressLevel(compressLevel),
      m_compressThreshold(compressThreshold),
      m_noZstd(noZstd),
      m_isEmpty(isEmpty)
{
}

RCCFileInfo::~RCCFileInfo()
{
    qDeleteAll(m_children);
}

QString RCCFileInfo::resourceName() const
{
    QString resource = m_name;
    for (RCCFileInfo *p = m_parent; p; p = p->m_parent)
        resource = resource.prepend(p->m_name + u'/');
    resource.prepend(u':');
    return resource;
}

void RCCFileInfo::writeDataInfo(RCCResourceLibrary &lib)
{
    //pointer data
    if (m_flags & RCCFileInfo::Directory) {
        // name offset
        lib.writeNumber4(m_nameOffset);

        // flags
        lib.writeNumber2(m_flags);

        // child count
        lib.writeNumber4(m_children.size());

        // first child offset
        lib.writeNumber4(m_childOffset);
    } else {
        // name offset
        lib.writeNumber4(m_nameOffset);

        // flags
        lib.writeNumber2(m_flags);

        // locale
        lib.writeNumber2(m_territory);
        lib.writeNumber2(m_language);

        //data offset
        lib.writeNumber4(m_dataOffset);
    }

    if (lib.formatVersion() >= 2) {
        // last modified time stamp
        const QDateTime lastModified = m_fileInfo.lastModified(QTimeZone::UTC);
        quint64 lastmod = quint64(lastModified.isValid() ? lastModified.toMSecsSinceEpoch() : 0);
        static const quint64 sourceDate = 1000 * qgetenv("QT_RCC_SOURCE_DATE_OVERRIDE").toULongLong();
        if (sourceDate != 0)
            lastmod = sourceDate;
        static const quint64 sourceDate2 = 1000 * qgetenv("SOURCE_DATE_EPOCH").toULongLong();
        if (sourceDate2 != 0)
            lastmod = sourceDate2;
        lib.writeNumber8(lastmod);
    }
}

qint64 RCCFileInfo::writeDataBlob(RCCResourceLibrary &lib, qint64 offset,
    QString *errorMessage)
{
    //capture the offset
    m_dataOffset = offset;
    QByteArray data;

    if (!m_isEmpty) {
        //find the data to be written
        QFile file(m_fileInfo.absoluteFilePath());
        if (!file.open(QFile::ReadOnly)) {
            *errorMessage = msgOpenReadFailed(m_fileInfo.absoluteFilePath(), file.errorString());
            return 0;
        }

        data = file.readAll();
    }

    // Check if compression is useful for this file
    if (data.size() != 0) {
#if QT_CONFIG(zstd)
        if (m_compressAlgo == RCCResourceLibrary::CompressionAlgorithm::Best && !m_noZstd) {
            m_compressAlgo = RCCResourceLibrary::CompressionAlgorithm::Zstd;
            m_compressLevel = 19;   // not ZSTD_maxCLevel(), as 20+ are experimental
        }
        if (m_compressAlgo == RCCResourceLibrary::CompressionAlgorithm::Zstd && !m_noZstd) {
            if (lib.m_zstdCCtx == nullptr)
                lib.m_zstdCCtx = ZSTD_createCCtx();
            qsizetype size = data.size();
            size = ZSTD_COMPRESSBOUND(size);

            int compressLevel = m_compressLevel;
            if (compressLevel < 0)
                compressLevel = CONSTANT_ZSTDCOMPRESSLEVEL_CHECK;

            QByteArray compressed(size, Qt::Uninitialized);
            char *dst = const_cast<char *>(compressed.constData());
            size_t n = ZSTD_compressCCtx(lib.m_zstdCCtx, dst, size,
                                         data.constData(), data.size(),
                                         compressLevel);
            if (n * 100.0 < data.size() * 1.0 * (100 - m_compressThreshold) ) {
                // compressing is worth it
                if (m_compressLevel < 0) {
                    // heuristic compression, so recompress
                    n = ZSTD_compressCCtx(lib.m_zstdCCtx, dst, size,
                                          data.constData(), data.size(),
                                          CONSTANT_ZSTDCOMPRESSLEVEL_STORE);
                }
                if (ZSTD_isError(n)) {
                    QString msg = QString::fromLatin1("%1: error: compression with zstd failed: %2\n")
                            .arg(m_name, QString::fromUtf8(ZSTD_getErrorName(n)));
                    lib.m_errorDevice->write(msg.toUtf8());
                } else if (lib.verbose()) {
                    QString msg = QString::fromLatin1("%1: note: compressed using zstd (%2 -> %3)\n")
                            .arg(m_name).arg(data.size()).arg(n);
                    lib.m_errorDevice->write(msg.toUtf8());
                }

                lib.m_overallFlags |= CompressedZstd;
                m_flags |= CompressedZstd;
                data = std::move(compressed);
                data.truncate(n);
            } else if (lib.verbose()) {
                QString msg = QString::fromLatin1("%1: note: not compressed\n").arg(m_name);
                lib.m_errorDevice->write(msg.toUtf8());
            }
        }
#endif
#ifndef QT_NO_COMPRESS
        if (m_compressAlgo == RCCResourceLibrary::CompressionAlgorithm::Best) {
            m_compressAlgo = RCCResourceLibrary::CompressionAlgorithm::Zlib;
            m_compressLevel = 9;
        }
        if (m_compressAlgo == RCCResourceLibrary::CompressionAlgorithm::Zlib) {
            QByteArray compressed =
                    qCompress(reinterpret_cast<uchar *>(data.data()), data.size(), m_compressLevel);

            int compressRatio = int(100.0 * (data.size() - compressed.size()) / data.size());
            if (compressRatio >= m_compressThreshold) {
                if (lib.verbose()) {
                    QString msg = QString::fromLatin1("%1: note: compressed using zlib (%2 -> %3)\n")
                            .arg(m_name).arg(data.size()).arg(compressed.size());
                    lib.m_errorDevice->write(msg.toUtf8());
                }
                data = compressed;
                lib.m_overallFlags |= Compressed;
                m_flags |= Compressed;
            } else if (lib.verbose()) {
                QString msg = QString::fromLatin1("%1: note: not compressed\n").arg(m_name);
                lib.m_errorDevice->write(msg.toUtf8());
            }
        }
#endif // QT_NO_COMPRESS
    }

    // write the length
    lib.writeNumber4(data.size());
    offset += 4;

    // write the payload
    lib.writeByteArray(data);
    offset += data.size();

    return offset;
}

qint64 RCCFileInfo::writeDataName(RCCResourceLibrary &lib, qint64 offset)
{
    // capture the offset
    m_nameOffset = offset;

    // write the length
    lib.writeNumber2(m_name.size());
    offset += 2;

    // write the hash
    lib.writeNumber4(qt_hash(m_name));
    offset += 4;

    // write the m_name
    const QChar *unicode = m_name.unicode();
    for (int i = 0; i < m_name.size(); ++i) {
        lib.writeNumber2(unicode[i].unicode());
    }
    offset += m_name.size()*2;

    return offset;
}


///////////////////////////////////////////////////////////
//
// RCCResourceLibrary
//
///////////////////////////////////////////////////////////

RCCResourceLibrary::Strings::Strings() :
   TAG_RCC("RCC"_L1),
   TAG_RESOURCE("qresource"_L1),
   TAG_FILE("file"_L1),
   ATTRIBUTE_LANG("lang"_L1),
   ATTRIBUTE_PREFIX("prefix"_L1),
   ATTRIBUTE_ALIAS("alias"_L1),
   ATTRIBUTE_EMPTY("empty"_L1),
   ATTRIBUTE_THRESHOLD("threshold"_L1),
   ATTRIBUTE_COMPRESS("compress"_L1),
   ATTRIBUTE_COMPRESSALGO(QStringLiteral("compression-algorithm"))
{
}

RCCResourceLibrary::RCCResourceLibrary(quint8 formatVersion)
  : m_root(nullptr),
    m_verbose(false),
    m_compressionAlgo(CompressionAlgorithm::Best),
    m_compressLevel(CONSTANT_COMPRESSLEVEL_DEFAULT),
    m_compressThreshold(CONSTANT_COMPRESSTHRESHOLD_DEFAULT),
    m_treeOffset(0),
    m_namesOffset(0),
    m_dataOffset(0),
    m_overallFlags(0),
    m_useNameSpace(CONSTANT_USENAMESPACE),
    m_errorDevice(nullptr),
    m_outDevice(nullptr),
    m_formatVersion(formatVersion),
    m_noZstd(false)
{
    m_out.reserve(30 * 1000 * 1000);
#if QT_CONFIG(zstd)
    m_zstdCCtx = nullptr;
#endif
}

RCCResourceLibrary::~RCCResourceLibrary()
{
    delete m_root;
#if QT_CONFIG(zstd)
    ZSTD_freeCCtx(m_zstdCCtx);
#endif
}

enum RCCXmlTag {
    RccTag,
    ResourceTag,
    FileTag
};
Q_DECLARE_TYPEINFO(RCCXmlTag, Q_PRIMITIVE_TYPE);

static bool parseBoolean(QStringView value, QString *errorMsg)
{
    if (value.compare("true"_L1, Qt::CaseInsensitive) == 0)
        return true;
    if (value.compare("false"_L1, Qt::CaseInsensitive) == 0)
        return false;

    *errorMsg = QString::fromLatin1("Invalid value for boolean attribute: '%1'").arg(value);
    return false;
}

bool RCCResourceLibrary::interpretResourceFile(QIODevice *inputDevice,
    const QString &fname, QString currentPath, bool listMode)
{
    Q_ASSERT(m_errorDevice);
    const QChar slash = u'/';
    if (!currentPath.isEmpty() && !currentPath.endsWith(slash))
        currentPath += slash;

    QXmlStreamReader reader(inputDevice);
    QStack<RCCXmlTag> tokens;

    QString prefix;
    QLocale::Language language = QLocale::c().language();
    QLocale::Territory territory = QLocale::c().territory();
    QString alias;
    bool empty = false;
    auto compressAlgo = m_compressionAlgo;
    int compressLevel = m_compressLevel;
    int compressThreshold = m_compressThreshold;

    while (!reader.atEnd()) {
        QXmlStreamReader::TokenType t = reader.readNext();
        switch (t) {
        case QXmlStreamReader::StartElement:
            if (reader.name() == m_strings.TAG_RCC) {
                if (!tokens.isEmpty())
                    reader.raiseError("expected <RCC> tag"_L1);
                else
                    tokens.push(RccTag);
            } else if (reader.name() == m_strings.TAG_RESOURCE) {
                if (tokens.isEmpty() || tokens.top() != RccTag) {
                    reader.raiseError("unexpected <RESOURCE> tag"_L1);
                } else {
                    tokens.push(ResourceTag);

                    QXmlStreamAttributes attributes = reader.attributes();
                    language = QLocale::c().language();
                    territory = QLocale::c().territory();

                    if (attributes.hasAttribute(m_strings.ATTRIBUTE_LANG)) {
                        QString attribute = attributes.value(m_strings.ATTRIBUTE_LANG).toString();
                        QLocale lang = QLocale(attribute);
                        language = lang.language();
                        if (2 == attribute.size()) {
                            // Language only
                            territory = QLocale::AnyTerritory;
                        } else {
                            territory = lang.territory();
                        }
                    }

                    prefix.clear();
                    if (attributes.hasAttribute(m_strings.ATTRIBUTE_PREFIX))
                        prefix = attributes.value(m_strings.ATTRIBUTE_PREFIX).toString();
                    if (!prefix.startsWith(slash))
                        prefix.prepend(slash);
                    if (!prefix.endsWith(slash))
                        prefix += slash;
                }
            } else if (reader.name() == m_strings.TAG_FILE) {
                if (tokens.isEmpty() || tokens.top() != ResourceTag) {
                    reader.raiseError("unexpected <FILE> tag"_L1);
                } else {
                    tokens.push(FileTag);

                    QXmlStreamAttributes attributes = reader.attributes();
                    alias.clear();
                    if (attributes.hasAttribute(m_strings.ATTRIBUTE_ALIAS))
                        alias = attributes.value(m_strings.ATTRIBUTE_ALIAS).toString();

                    compressAlgo = m_compressionAlgo;
                    compressLevel = m_compressLevel;
                    compressThreshold = m_compressThreshold;

                    QString errorString;
                    if (attributes.hasAttribute(m_strings.ATTRIBUTE_EMPTY))
                        empty = parseBoolean(attributes.value(m_strings.ATTRIBUTE_EMPTY), &errorString);
                    else
                        empty = false;

                    if (attributes.hasAttribute(m_strings.ATTRIBUTE_COMPRESSALGO))
                        compressAlgo = parseCompressionAlgorithm(attributes.value(m_strings.ATTRIBUTE_COMPRESSALGO), &errorString);
                    if (errorString.isEmpty() && attributes.hasAttribute(m_strings.ATTRIBUTE_COMPRESS)) {
                        QString value = attributes.value(m_strings.ATTRIBUTE_COMPRESS).toString();
                        compressLevel = parseCompressionLevel(compressAlgo, value, &errorString);
                    }

                    // Special case for -no-compress
                    if (m_compressLevel == -2)
                        compressAlgo = CompressionAlgorithm::None;

                    if (attributes.hasAttribute(m_strings.ATTRIBUTE_THRESHOLD))
                        compressThreshold = attributes.value(m_strings.ATTRIBUTE_THRESHOLD).toString().toInt();

                    if (!errorString.isEmpty())
                        reader.raiseError(errorString);
                }
            } else {
                reader.raiseError("unexpected tag: %1"_L1.arg(reader.name().toString()));
            }
            break;

        case QXmlStreamReader::EndElement:
            if (reader.name() == m_strings.TAG_RCC) {
                if (!tokens.isEmpty() && tokens.top() == RccTag)
                    tokens.pop();
                else
                    reader.raiseError("unexpected closing tag"_L1);
            } else if (reader.name() == m_strings.TAG_RESOURCE) {
                if (!tokens.isEmpty() && tokens.top() == ResourceTag)
                    tokens.pop();
                else
                    reader.raiseError("unexpected closing tag"_L1);
            } else if (reader.name() == m_strings.TAG_FILE) {
                if (!tokens.isEmpty() && tokens.top() == FileTag)
                    tokens.pop();
                else
                    reader.raiseError("unexpected closing tag"_L1);
            }
            break;

        case QXmlStreamReader::Characters:
            if (reader.isWhitespace())
                break;
            if (tokens.isEmpty() || tokens.top() != FileTag) {
                reader.raiseError("unexpected text"_L1);
            } else {
                QString fileName = reader.text().toString();
                if (fileName.isEmpty()) {
                    const QString msg = QString::fromLatin1("RCC: Warning: Null node in XML of '%1'\n").arg(fname);
                    m_errorDevice->write(msg.toUtf8());
                }

                if (alias.isNull())
                    alias = fileName;

                alias = QDir::cleanPath(alias);
                while (alias.startsWith("../"_L1))
                    alias.remove(0, 3);
                alias = QDir::cleanPath(m_resourceRoot) + prefix + alias;

                QString absFileName = fileName;
                if (QDir::isRelativePath(absFileName))
                    absFileName.prepend(currentPath);
                QFileInfo file(absFileName);
                if (file.isDir()) {
                    QDir dir(file.filePath());
                    if (!alias.endsWith(slash))
                        alias += slash;

                    QStringList filePaths;
                    qCritical() << Q_FUNC_INFO << "Some code here was disabled for some reasons, result may be bad";
                    // using F = QDirListing::IteratorFlag;
                    // for (const auto &entry : QDirListing(dir, F::FollowSymlinks | F::Recursive)) {
                    //     const QString &fileName = entry.fileName();
                    //     if (fileName == "."_L1 || fileName == ".."_L1)
                    //         continue;
                    //     filePaths.emplace_back(entry.filePath());
                    // }

                    // make rcc output deterministic
                    std::sort(filePaths.begin(), filePaths.end());

                    for (const QString &filePath : filePaths) {
                        QFileInfo child(filePath);
                        const bool arc =
                                addFile(alias + child.fileName(),
                                        RCCFileInfo(child.fileName(), child, language, territory,
                                                    child.isDir() ? RCCFileInfo::Directory
                                                                  : RCCFileInfo::NoFlags,
                                                    compressAlgo, compressLevel, compressThreshold,
                                                    m_noZstd, empty));
                        if (!arc)
                            m_failedResources.push_back(child.fileName());
                    }
                } else if (listMode || file.isFile()) {
                    const bool arc =
                        addFile(alias,
                                RCCFileInfo(alias.section(slash, -1),
                                            file,
                                            language,
                                            territory,
                                            RCCFileInfo::NoFlags,
                                            compressAlgo,
                                            compressLevel,
                                            compressThreshold,
                                            m_noZstd, empty)
                                );
                    if (!arc)
                        m_failedResources.push_back(absFileName);
                } else if (file.exists()) {
                    m_failedResources.push_back(absFileName);
                    const QString msg = QString::fromLatin1("RCC: Error in '%1': Entry '%2' is neither a file nor a directory\n")
                                        .arg(fname, fileName);
                    m_errorDevice->write(msg.toUtf8());
                    return false;
                } else {
                    m_failedResources.push_back(absFileName);
                    const QString msg = QString::fromLatin1("RCC: Error in '%1': Cannot find file '%2'\n")
                                        .arg(fname, fileName);
                    m_errorDevice->write(msg.toUtf8());
                    return false;
                }
            }
            break;

        default:
            break;
        }
    }

    if (reader.hasError()) {
        int errorLine = reader.lineNumber();
        int errorColumn = reader.columnNumber();
        QString errorMessage = reader.errorString();
        QString msg = QString::fromLatin1("RCC Parse Error: '%1' Line: %2 Column: %3 [%4]\n").arg(fname).arg(errorLine).arg(errorColumn).arg(errorMessage);
        m_errorDevice->write(msg.toUtf8());
        return false;
    }

    if (m_root == nullptr) {
        const QString msg = QString::fromLatin1("RCC: Warning: No resources in '%1'.\n").arg(fname);
        m_errorDevice->write(msg.toUtf8());
        if (!listMode) {
            // create dummy entry, otherwise loading with QResource will crash
            m_root = new RCCFileInfo{};
            m_root->m_flags = RCCFileInfo::Directory;
        }
    }

    return true;
}

bool RCCResourceLibrary::addFile(const QString &alias, RCCFileInfo file)
{
    Q_ASSERT(m_errorDevice);
    if (file.m_fileInfo.size() > 0xffffffff) {
        const QString msg = QString::fromLatin1("File too big: %1\n").arg(file.m_fileInfo.absoluteFilePath());
        m_errorDevice->write(msg.toUtf8());
        return false;
    }
    if (!m_root) {
        m_root = new RCCFileInfo{};
        m_root->m_flags = RCCFileInfo::Directory;
    }

    RCCFileInfo *parent = m_root;
    const QStringList nodes = alias.split(u'/');
    for (int i = 1; i < nodes.size()-1; ++i) {
        const QString node = nodes.at(i);
        if (node.isEmpty())
            continue;
        if (!parent->m_children.contains(node)) {
            RCCFileInfo *s = new RCCFileInfo{};
            s->m_name = node;
            s->m_flags = RCCFileInfo::Directory;
            s->m_parent = parent;
            parent->m_children.insert(node, s);
            parent = s;
        } else {
            parent = *parent->m_children.constFind(node);
        }
    }

    const QString filename = nodes.at(nodes.size()-1);
    RCCFileInfo *s = new RCCFileInfo(std::move(file));
    s->m_parent = parent;
    auto cbegin = parent->m_children.constFind(filename);
    auto cend = parent->m_children.constEnd();
    for (auto it = cbegin; it != cend; ++it) {
        if (it.key() == filename && it.value()->m_language == s->m_language &&
            it.value()->m_territory == s->m_territory) {
            for (const QString &name : std::as_const(m_fileNames)) {
                qWarning("%s: Warning: potential duplicate alias detected: '%s'",
                qPrintable(name), qPrintable(filename));
            }
            break;
        }
    }
    parent->m_children.insert(filename, s);
    return true;
}

void RCCResourceLibrary::reset()
{
     if (m_root) {
        delete m_root;
        m_root = nullptr;
    }
    m_errorDevice = nullptr;
    m_failedResources.clear();
}


bool RCCResourceLibrary::readFiles(QIODevice &errorDevice)
{
    reset();
    m_errorDevice = &errorDevice;
    //read in data
    if (m_verbose) {
        const QString msg = QString::fromLatin1("Processing %1 files\n")
            .arg(m_fileNames.size());
        m_errorDevice->write(msg.toUtf8());
    }
    for (int i = 0; i < m_fileNames.size(); ++i) {
        QFile fileIn;
        QString fname = m_fileNames.at(i);
        QString pwd;
        if (fname == "-"_L1) {
            fname = "(stdin)"_L1;
            pwd = QDir::currentPath();
            fileIn.setFileName(fname);
            if (!fileIn.open(stdin, QIODevice::ReadOnly)) {
                m_errorDevice->write(msgOpenReadFailed(fname, fileIn.errorString()).toUtf8());
                return false;
            }
        } else {
            pwd = QFileInfo(fname).path();
            fileIn.setFileName(fname);
            if (!fileIn.open(QIODevice::ReadOnly)) {
                m_errorDevice->write(msgOpenReadFailed(fname, fileIn.errorString()).toUtf8());
                return false;
            }
        }
        if (m_verbose) {
            const QString msg = QString::fromLatin1("Interpreting %1\n").arg(fname);
            m_errorDevice->write(msg.toUtf8());
        }

        if (!interpretResourceFile(&fileIn, fname, pwd, false))
            return false;
    }
    return true;
}

// QStringList RCCResourceLibrary::dataFiles() const
// {
//     QStringList ret;
//     QStack<RCCFileInfo*> pending;

//     if (!m_root)
//         return ret;
//     pending.push(m_root);
//     while (!pending.isEmpty()) {
//         RCCFileInfo *file = pending.pop();
//         for (auto it = file->m_children.begin();
//             it != file->m_children.end(); ++it) {
//             RCCFileInfo *child = it.value();
//             if (child->m_flags & RCCFileInfo::Directory)
//                 pending.push(child);
//             else
//                 ret.append(child->m_fileInfo.filePath());
//         }
//     }
//     return ret;
// }

// Determine map of resource identifier (':/newPrefix/images/p1.png') to file via recursion
static void resourceDataFileMapRecursion(const RCCFileInfo *m_root, const QString &path, RCCResourceLibrary::ResourceDataFileMap &m)
{
    const QChar slash = u'/';
    const auto cend = m_root->m_children.constEnd();
    for (auto it = m_root->m_children.constBegin(); it != cend; ++it) {
        const RCCFileInfo *child = it.value();
        const QString childName = path + slash + child->m_name;
        if (child->m_flags & RCCFileInfo::Directory) {
            resourceDataFileMapRecursion(child, childName, m);
        } else {
            m.insert(childName, child->m_fileInfo.filePath());
        }
    }
}

// RCCResourceLibrary::ResourceDataFileMap RCCResourceLibrary::resourceDataFileMap() const
// {
//     ResourceDataFileMap rc;
//     if (m_root)
//         resourceDataFileMapRecursion(m_root, QString(u':'),  rc);
//     return rc;
// }

RCCResourceLibrary::CompressionAlgorithm RCCResourceLibrary::parseCompressionAlgorithm(QStringView value, QString *errorMsg)
{
    if (value == "best"_L1)
        return CompressionAlgorithm::Best;
    if (value == "zlib"_L1) {
#ifdef QT_NO_COMPRESS
        *errorMsg = "zlib support not compiled in"_L1;
#else
        return CompressionAlgorithm::Zlib;
#endif
    } else if (value == "zstd"_L1) {
#if QT_CONFIG(zstd)
        return CompressionAlgorithm::Zstd;
#else
        *errorMsg = "Zstandard support not compiled in"_L1;
#endif
    } else if (value != "none"_L1) {
        *errorMsg = QString::fromLatin1("Unknown compression algorithm '%1'").arg(value);
    }

    return CompressionAlgorithm::None;
}

int RCCResourceLibrary::parseCompressionLevel(CompressionAlgorithm algo, const QString &level, QString *errorMsg)
{
    bool ok;
    int c = level.toInt(&ok);
    if (ok) {
        switch (algo) {
        case CompressionAlgorithm::None:
        case CompressionAlgorithm::Best:
            return 0;
        case CompressionAlgorithm::Zlib:
            if (c >= 1 && c <= 9)
                return c;
            break;
        case CompressionAlgorithm::Zstd:
#if QT_CONFIG(zstd)
            if (c >= 0 && c <= ZSTD_maxCLevel())
                return c;
#endif
            break;
        }
    }

    *errorMsg = QString::fromLatin1("invalid compression level '%1'").arg(level);
    return 0;
}

bool RCCResourceLibrary::output(QIODevice &outDevice, QIODevice &errorDevice)
{
    m_errorDevice = &errorDevice;

    //write out
    if (m_verbose)
        m_errorDevice->write("Outputting code\n");
    if (!writeHeader()) {
        m_errorDevice->write("Could not write header\n");
        return false;
    }
    if (m_root) {
        if (!writeDataBlobs()) {
            m_errorDevice->write("Could not write data blobs.\n");
            return false;
        }
        if (!writeDataNames()) {
            m_errorDevice->write("Could not write file names\n");
            return false;
        }
        if (!writeDataStructure()) {
            m_errorDevice->write("Could not write data tree\n");
            return false;
        }
    }
    if (!writeInitializer()) {
        m_errorDevice->write("Could not write footer\n");
        return false;
    }
    outDevice.write(m_out.constData(), m_out.size());
    return true;
}

// void RCCResourceLibrary::writeDecimal(int value)
// {
//     Q_ASSERT(m_format != RCCResourceLibrary::Binary);
//     char buf[std::numeric_limits<int>::digits10 + 2];
//     int n = snprintf(buf, sizeof(buf), "%d", value);
//     write(buf, n);
// }

// static const char hexDigits[] = "0123456789abcdef";

// inline void RCCResourceLibrary::write2HexDigits(quint8 number)
// {
//     writeChar(hexDigits[number >> 4]);
//     writeChar(hexDigits[number & 0xf]);
// }

// void RCCResourceLibrary::writeHex(quint8 tmp)
// {
//     writeChar('0');
//     writeChar('x');
//     if (tmp < 16)
//         writeChar(hexDigits[tmp]);
//     else
//         write2HexDigits(tmp);
//     writeChar(',');
// }

void RCCResourceLibrary::writeNumber2(quint16 number)
{
    writeChar(number >> 8);
    writeChar(number);
}

void RCCResourceLibrary::writeNumber4(quint32 number)
{
    writeChar(number >> 24);
    writeChar(number >> 16);
    writeChar(number >> 8);
    writeChar(number);
}

void RCCResourceLibrary::writeNumber8(quint64 number)
{
    writeChar(number >> 56);
    writeChar(number >> 48);
    writeChar(number >> 40);
    writeChar(number >> 32);
    writeChar(number >> 24);
    writeChar(number >> 16);
    writeChar(number >> 8);
    writeChar(number);
}

bool RCCResourceLibrary::writeHeader()
{
    writeString("qres");
    writeNumber4(0);
    writeNumber4(0);
    writeNumber4(0);
    writeNumber4(0);
    if (m_formatVersion >= 3)
        writeNumber4(m_overallFlags);
    return true;
}

bool RCCResourceLibrary::writeDataBlobs()
{
    Q_ASSERT(m_errorDevice);
    m_dataOffset = m_out.size();

    if (!m_root)
        return false;

    QStack<RCCFileInfo*> pending;
    pending.push(m_root);
    qint64 offset = 0;
    QString errorMessage;
    while (!pending.isEmpty()) {
        RCCFileInfo *file = pending.pop();
        for (auto it = file->m_children.cbegin(); it != file->m_children.cend(); ++it) {
            RCCFileInfo *child = it.value();
            if (child->m_flags & RCCFileInfo::Directory)
                pending.push(child);
            else {
                offset = child->writeDataBlob(*this, offset, &errorMessage);
                if (offset == 0) {
                    m_errorDevice->write(errorMessage.toUtf8());
                    return false;
                }
            }
        }
    }
    return true;
}

bool RCCResourceLibrary::writeDataNames()
{
    m_namesOffset = m_out.size();

    QHash<QString, int> names;
    QStack<RCCFileInfo*> pending;

    if (!m_root)
        return false;

    pending.push(m_root);
    qint64 offset = 0;
    while (!pending.isEmpty()) {
        RCCFileInfo *file = pending.pop();
        for (auto it = file->m_children.cbegin(); it != file->m_children.cend(); ++it) {
            RCCFileInfo *child = it.value();
            if (child->m_flags & RCCFileInfo::Directory)
                pending.push(child);
            if (names.contains(child->m_name)) {
                child->m_nameOffset = names.value(child->m_name);
            } else {
                names.insert(child->m_name, offset);
                offset = child->writeDataName(*this, offset);
            }
        }
    }
    return true;
}

struct qt_rcc_compare_hash
{
    typedef bool result_type;
    result_type operator()(const RCCFileInfo *left, const RCCFileInfo *right) const
    {
        return qt_hash(left->m_name) < qt_hash(right->m_name);
    }
};

bool RCCResourceLibrary::writeDataStructure()
{
    m_treeOffset = m_out.size();

    QStack<RCCFileInfo*> pending;

    if (!m_root)
        return false;

    //calculate the child offsets (flat)
    pending.push(m_root);
    int offset = 1;
    while (!pending.isEmpty()) {
        RCCFileInfo *file = pending.pop();
        file->m_childOffset = offset;

        //sort by hash value for binary lookup
        QList<RCCFileInfo*> m_children = file->m_children.values();
        std::sort(m_children.begin(), m_children.end(), qt_rcc_compare_hash());

        //write out the actual data now
        for (int i = 0; i < m_children.size(); ++i) {
            RCCFileInfo *child = m_children.at(i);
            ++offset;
            if (child->m_flags & RCCFileInfo::Directory)
                pending.push(child);
        }
    }

    //write out the structure (ie iterate again!)
    pending.push(m_root);
    m_root->writeDataInfo(*this);
    while (!pending.isEmpty()) {
        RCCFileInfo *file = pending.pop();

        //sort by hash value for binary lookup
        QList<RCCFileInfo*> m_children = file->m_children.values();
        std::sort(m_children.begin(), m_children.end(), qt_rcc_compare_hash());

        //write out the actual data now
        for (int i = 0; i < m_children.size(); ++i) {
            RCCFileInfo *child = m_children.at(i);
            child->writeDataInfo(*this);
            if (child->m_flags & RCCFileInfo::Directory)
                pending.push(child);
        }
    }
    return true;
}

void RCCResourceLibrary::writeMangleNamespaceFunction(const QByteArray &name)
{
    if (m_useNameSpace) {
        writeString("QT_RCC_MANGLE_NAMESPACE(");
        writeByteArray(name);
        writeChar(')');
    } else {
        writeByteArray(name);
    }
}

void RCCResourceLibrary::writeAddNamespaceFunction(const QByteArray &name)
{
    if (m_useNameSpace) {
        writeString("QT_RCC_PREPEND_NAMESPACE(");
        writeByteArray(name);
        writeChar(')');
    } else {
        writeByteArray(name);
    }
}

bool RCCResourceLibrary::writeInitializer()
{
    int i = 4;
    char *p = m_out.data();
    p[i++] = 0;
    p[i++] = 0;
    p[i++] = 0;
    p[i++] = m_formatVersion;

    p[i++] = (m_treeOffset >> 24) & 0xff;
    p[i++] = (m_treeOffset >> 16) & 0xff;
    p[i++] = (m_treeOffset >>  8) & 0xff;
    p[i++] = (m_treeOffset >>  0) & 0xff;

    p[i++] = (m_dataOffset >> 24) & 0xff;
    p[i++] = (m_dataOffset >> 16) & 0xff;
    p[i++] = (m_dataOffset >>  8) & 0xff;
    p[i++] = (m_dataOffset >>  0) & 0xff;

    p[i++] = (m_namesOffset >> 24) & 0xff;
    p[i++] = (m_namesOffset >> 16) & 0xff;
    p[i++] = (m_namesOffset >>  8) & 0xff;
    p[i++] = (m_namesOffset >>  0) & 0xff;

    if (m_formatVersion >= 3) {
        p[i++] = (m_overallFlags >> 24) & 0xff;
        p[i++] = (m_overallFlags >> 16) & 0xff;
        p[i++] = (m_overallFlags >>  8) & 0xff;
        p[i++] = (m_overallFlags >>  0) & 0xff;
    }
    return true;
}
