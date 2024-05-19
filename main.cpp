// Copyright (C) 2018 The Qt Company Ltd.
// Copyright (C) 2018 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

// #include "rcc.h"
#include "lilrcc.h"

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

    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription("Qt Resource reCompiler version");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Rcc file to work with."));

    QCommandLineOption catOption(QStringLiteral("cat"), QStringLiteral("Print <path> file to console. Use pipes to save"), QStringLiteral("path"));
    parser.addOption(catOption);

    //parse options
    parser.process(app);

    if (parser.positionalArguments().isEmpty()) {
        qCritical() << "No files specified";
        return 1;
    }
    if (parser.positionalArguments().size() > 1) {
        qCritical() << "Too many files";
        return 1;
    }
    QFile file(parser.positionalArguments().first());
    file.open(QIODeviceBase::ReadOnly);
    LilResourceLibrary lillib(&file);
    QTextStream out(stdout);
    if (parser.isSet(catOption)) {
        if (parser.value(catOption).isEmpty()) {
            qCritical() << "Please specify path to file after cat option\n";
            return 1;
        }
        QString error_string;
        lillib.getFile(parser.value(catOption), out, error_string);
        if (!error_string.isEmpty()) {
            QTextStream error(stderr);
            error << error_string << "\n";
            return 1;
        }
    } else {
        lillib.printTree(out);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    return runRcc(argc, argv);
}
