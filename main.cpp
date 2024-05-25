// Copyright (C) 2018 The Qt Company Ltd.
// Copyright (C) 2018 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lilrcc.h"
#include "resourcereader.h"
#include "tree.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>

using namespace Qt::StringLiterals;

#define ASSERT(cond, message) if (!(cond)) {\
    qCritical() << message;\
    exit(1);\
}\

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription("lil Qt Resource reCompiler");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("<file>"), QStringLiteral("Existing rcc file in your filesystem or where you wanna create it"));
    parser.addPositionalArgument(QStringLiteral("<action>"), QStringLiteral("Linux command-like what program should do\n"
                                                                          "ls [path]\n"
                                                                          "cat <file>"
                                                                          "tree"
                                                                          "allfiles"
                                                                          "rm <file>"));
    parser.addPositionalArgument(QStringLiteral("[<args>]"), QStringLiteral("Arguments for command"));
    QCommandLineOption output({"o", "output"}, "Output file, used in modifying commands, should differ from input", "outfile");
    parser.addOption(output);

    parser.process(app);

    QStringList args = parser.positionalArguments();
    qDebug() << args;
    if (args.isEmpty()) {
        qCritical() << "Please specify file";
        parser.showHelp(1);
    }
    QString inFile = args.first();
    QFile file(inFile);
    if (!file.exists()) {
        qCritical() << "File does not exist";
        return 1;
    }
    if (args.size() < 2) {
        qCritical() << "Please specity action";
        parser.showHelp(1);
    }
    file.open(QIODeviceBase::ReadOnly);
    ResourceReader reader(&file);
    ResourceLibrary lillib(&reader);
    QTextStream out(stdout);
    if (args[1] == "cat") {
        ASSERT(args.size() >= 3, "Please specify path to file after cat option")
        QString error_string;
        lillib.getFile(args[2], out, error_string);
        if (!error_string.isEmpty()) {
            qCritical() << error_string << "\n";
            return 1;
        }
    } else if (args[1] == "ls") {
        QString path = args.size() < 3 ? "/" : args[2];
        QString error_string;
        lillib.ls(path, error_string);
        if (!error_string.isEmpty()) {
            qCritical() << error_string << "\n";
            return 1;
        }
    } else if (args[1] == "tree") {
        lillib.printTree(out);
    } else if (args[1] == "allfiles") {
        lillib.printAllFiles();
    } else if (args[1] == "rm") {
        QString outDir = parser.value(output);
        if (outDir.isEmpty()) {
            qCritical() << "Please specify output path via -o option\n";
            return 1;
        }
        if (QFileInfo(outDir).absoluteFilePath() == QFileInfo(inFile).absoluteFilePath()) {
            qCritical() << "You should not write to input file";
            return 1;
        }
        if (args.size() < 3) {
            qCritical() << "Please specify path to file after rm option\n";
            return 1;
        }
        QString error_string;
        lillib.rmFile(args[2], error_string);
        if (!error_string.isEmpty()) {
            qCritical() << error_string << "\n";
            return 1;
        }
        // lillib.save(out);
        lillib.printTree(out);
    } else {
        qCritical() << "Unknown action specified, please select smarter";
        parser.showHelp(1);
    }
    return 0;
}
