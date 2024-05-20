// Copyright (C) 2018 The Qt Company Ltd.
// Copyright (C) 2018 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lilrcc.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
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
                                                                          "tree"));
    parser.addPositionalArgument(QStringLiteral("[<args>]"), QStringLiteral("Arguments for command"));

    parser.process(app);

    QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        qCritical() << "Please specify file";
        parser.showHelp(1);
    }
    QFile file(args.first());
    if (!file.exists()) {
        qCritical() << "File does not exist";
        return 1;
    }
    if (args.size() < 2) {
        qCritical() << "Please specity action";
        parser.showHelp(1);
    }
    file.open(QIODeviceBase::ReadOnly);
    LilResourceLibrary lillib(&file);
    QTextStream out(stdout);
    if (args[1] == "cat") {
        if (args.size() < 3) {
            qCritical() << "Please specify path to file after cat option\n";
            return 1;
        }
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
    } else {
        qCritical() << "Unknown action specified, please select smarter";
        parser.showHelp(1);
    }
    return 0;
}
