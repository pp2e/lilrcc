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
    QCoreApplication::setApplicationVersion(QStringLiteral(QT_VERSION_STR));

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

    QCommandLineOption tempOption(QStringList() << QStringLiteral("t") << QStringLiteral("temp"));
    tempOption.setDescription(QStringLiteral("Use temporary <file> for big resources."));
    tempOption.setValueName(QStringLiteral("file"));
    parser.addOption(tempOption);

    QCommandLineOption nameOption(QStringLiteral("name"), QStringLiteral("Create an external initialization function with <name>."), QStringLiteral("name"));
    parser.addOption(nameOption);

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

    QCommandLineOption binaryOption(QStringLiteral("binary"), QStringLiteral("Output a binary file for use as a dynamic resource."));
    parser.addOption(binaryOption);

    QCommandLineOption generatorOption(QStringList{QStringLiteral("g"), QStringLiteral("generator")});
    generatorOption.setDescription(QStringLiteral("Select generator."));
    generatorOption.setValueName(QStringLiteral("cpp|python|python2"));
    parser.addOption(generatorOption);

    QCommandLineOption passOption(QStringLiteral("pass"), QStringLiteral("Pass number for big resources"), QStringLiteral("number"));
    parser.addOption(passOption);

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
    if (parser.isSet(nameOption))
        library.setInitName(parser.value(nameOption));
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
    if (parser.isSet(binaryOption))
        library.setFormat(RCCResourceLibrary::Binary);
    if (parser.isSet(generatorOption)) {
        auto value = parser.value(generatorOption);
        if (value == "cpp"_L1) {
            library.setFormat(RCCResourceLibrary::C_Code);
        } else if (value == "python"_L1) {
            library.setFormat(RCCResourceLibrary::Python_Code);
        } else if (value == "python2"_L1) { // ### fixme Qt 7: remove
            qWarning("Format python2 is no longer supported, defaulting to python.");
            library.setFormat(RCCResourceLibrary::Python_Code);
        } else {
            errorMsg = "Invalid generator: "_L1 + value;
        }
    }

    if (parser.isSet(passOption)) {
        if (parser.value(passOption) == "1"_L1)
            library.setFormat(RCCResourceLibrary::Pass1);
        else if (parser.value(passOption) == "2"_L1)
            library.setFormat(RCCResourceLibrary::Pass2);
        else
            errorMsg = "Pass number must be 1 or 2"_L1;
    }
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
    QString tempFilename = parser.value(tempOption);

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

    QFile out;

    // open output
    QIODevice::OpenMode mode = QIODevice::NotOpen;
    switch (library.format()) {
        case RCCResourceLibrary::C_Code:
        case RCCResourceLibrary::Pass1:
        case RCCResourceLibrary::Python_Code:
            mode = QIODevice::WriteOnly | QIODevice::Text;
            break;
        case RCCResourceLibrary::Pass2:
        case RCCResourceLibrary::Binary:
            mode = QIODevice::WriteOnly;
            break;
    }


    if (outFilename.isEmpty() || outFilename == "-"_L1) {
        // using this overload close() only flushes.
        out.open(stdout, mode);
    } else {
        out.setFileName(outFilename);
        if (!out.open(mode)) {
            const QString msg = QString::fromLatin1("Unable to open %1 for writing: %2\n")
                                .arg(outFilename, out.errorString());
            errorDevice.write(msg.toUtf8());
            return 1;
        }
    }

    QFile temp;
    if (!tempFilename.isEmpty()) {
        temp.setFileName(tempFilename);
        if (!temp.open(QIODevice::ReadOnly)) {
            const QString msg = QString::fromUtf8("Unable to open temporary file %1 for reading: %2\n")
                    .arg(tempFilename, out.errorString());
            errorDevice.write(msg.toUtf8());
            return 1;
        }
    }
    bool success = library.output(out, temp, errorDevice);
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
