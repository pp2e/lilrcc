// Copyright (C) 2018 The Qt Company Ltd.
// Copyright (C) 2018 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "rcc.h"

#include <qdebug.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qhashfunctions.h>
#include <qtextstream.h>
#include <qatomic.h>
#include <qglobal.h>
#include <qcoreapplication.h>
#include <qcommandlineoption.h>
#include <qcommandlineparser.h>

using namespace Qt::StringLiterals;

int runRcc(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // QCoreApplication::setApplicationVersion(QStringLiteral(QT_VERSION_STR));

    // Note that rcc isn't translated.
    // If you use this code as an example for a translated app, make sure to translate the strings.
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription("Qt Resource Compiler version " QT_VERSION_STR ""_L1);
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption outputOption(QStringList() << QStringLiteral("o") << QStringLiteral("output"));
    outputOption.setDescription(QStringLiteral("Write output to <file> rather than stdout."));
    outputOption.setValueName(QStringLiteral("file"));
    parser.addOption(outputOption);

    // QCommandLineOption nameOption(QStringLiteral("name"), QStringLiteral("Create an external initialization function with <name>."), QStringLiteral("name"));
    // parser.addOption(nameOption);

    QCommandLineOption rootOption(QStringLiteral("root"), QStringLiteral("Prefix resource access path with root path."), QStringLiteral("path"));
    parser.addOption(rootOption);

#if QT_CONFIG(zstd) && !defined(QT_NO_COMPRESS)
#  define ALGOS     "[zstd], zlib, none"
#elif QT_CONFIG(zstd)
#  define ALGOS     "[zstd], none"
#elif !defined(QT_NO_COMPRESS)
#  define ALGOS     "[zlib], none"
#else
#  define ALGOS     "[none]"
#endif
    const QString &algoDescription =
            QStringLiteral("Compress input files using algorithm <algo> (" ALGOS ").");
    QCommandLineOption compressionAlgoOption(QStringLiteral("compress-algo"), algoDescription, QStringLiteral("algo"));
    parser.addOption(compressionAlgoOption);
#undef ALGOS

    QCommandLineOption compressOption(QStringLiteral("compress"), QStringLiteral("Compress input files by <level>."), QStringLiteral("level"));
    parser.addOption(compressOption);

    QCommandLineOption nocompressOption(QStringLiteral("no-compress"), QStringLiteral("Disable all compression. Same as --compress-algo=none."));
    parser.addOption(nocompressOption);

    QCommandLineOption noZstdOption(QStringLiteral("no-zstd"), QStringLiteral("Disable usage of zstd compression."));
    parser.addOption(noZstdOption);

    QCommandLineOption thresholdOption(QStringLiteral("threshold"), QStringLiteral("Threshold to consider compressing files."), QStringLiteral("level"));
    parser.addOption(thresholdOption);

    QCommandLineOption namespaceOption(QStringLiteral("namespace"), QStringLiteral("Turn off namespace macros."));
    parser.addOption(namespaceOption);

    QCommandLineOption verboseOption(QStringLiteral("verbose"), QStringLiteral("Enable verbose mode."));
    parser.addOption(verboseOption);

    QCommandLineOption formatVersionOption(QStringLiteral("format-version"), QStringLiteral("The RCC format version to write"), QStringLiteral("number"));
    parser.addOption(formatVersionOption);

    parser.addPositionalArgument(QStringLiteral("inputs"), QStringLiteral("Input files (*.qrc)."));


    //parse options
    parser.process(app);

    QString errorMsg;

    quint8 formatVersion = 3;
    if (parser.isSet(formatVersionOption)) {
        bool ok = false;
        formatVersion = parser.value(formatVersionOption).toUInt(&ok);
        if (!ok) {
            errorMsg = "Invalid format version specified"_L1;
        } else if (formatVersion < 1 || formatVersion > 3) {
            errorMsg = "Unsupported format version specified"_L1;
        }
    }

    RCCResourceLibrary library(formatVersion);
    // if (parser.isSet(nameOption))
    //     library.setInitName(parser.value(nameOption));
    if (parser.isSet(rootOption)) {
        library.setResourceRoot(QDir::cleanPath(parser.value(rootOption)));
        if (library.resourceRoot().isEmpty() || library.resourceRoot().at(0) != u'/')
            errorMsg = "Root must start with a /"_L1;
    }

    if (parser.isSet(compressionAlgoOption))
        library.setCompressionAlgorithm(RCCResourceLibrary::parseCompressionAlgorithm(parser.value(compressionAlgoOption), &errorMsg));
    if (parser.isSet(noZstdOption))
        library.setNoZstd(true);
    if (library.compressionAlgorithm() == RCCResourceLibrary::CompressionAlgorithm::Zstd) {
        if (formatVersion < 3)
            errorMsg = "Zstandard compression requires format version 3 or higher"_L1;
        if (library.noZstd())
            errorMsg = "--compression-algo=zstd and --no-zstd both specified."_L1;
    }
    if (parser.isSet(nocompressOption))
        library.setCompressionAlgorithm(RCCResourceLibrary::CompressionAlgorithm::None);
    if (parser.isSet(compressOption) && errorMsg.isEmpty()) {
        int level = library.parseCompressionLevel(library.compressionAlgorithm(), parser.value(compressOption), &errorMsg);
        library.setCompressLevel(level);
    }
    if (parser.isSet(thresholdOption))
        library.setCompressThreshold(parser.value(thresholdOption).toInt());

    if (parser.isSet(namespaceOption))
        library.setUseNameSpace(!library.useNameSpace());
    if (parser.isSet(verboseOption))
        library.setVerbose(true);

    const QStringList filenamesIn = parser.positionalArguments();

    for (const QString &file : filenamesIn) {
        if (file == "-"_L1)
            continue;
        else if (!QFile::exists(file)) {
            qWarning("%s: File does not exist '%s'", argv[0], qPrintable(file));
            return 1;
        }
    }

    QString outFilename = parser.value(outputOption);

    if (filenamesIn.isEmpty())
        errorMsg = QStringLiteral("No input files specified.");

    if (!errorMsg.isEmpty()) {
        fprintf(stderr, "%s: %s\n", argv[0], qPrintable(errorMsg));
        parser.showHelp(1);
        return 1;
    }
    QFile errorDevice;
    errorDevice.open(stderr, QIODevice::WriteOnly|QIODevice::Text);

    if (library.verbose())
        errorDevice.write("Qt resource compiler\n");

    library.setInputFiles(filenamesIn);

    if (!library.readFiles(errorDevice))
        return 1;

    QFile out;

    if (outFilename.isEmpty() || outFilename == "-"_L1) {
        // using this overload close() only flushes.
        out.open(stdout, QIODevice::WriteOnly);
    } else {
        out.setFileName(outFilename);
        if (!out.open(QIODevice::WriteOnly)) {
            const QString msg = QString::fromLatin1("Unable to open %1 for writing: %2\n")
                                .arg(outFilename, out.errorString());
            errorDevice.write(msg.toUtf8());
            return 1;
        }
    }

    bool success = library.output(out, errorDevice);
    if (!success) {
        // erase the output file if we failed
        out.remove();
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    return runRcc(argc, argv);
}
