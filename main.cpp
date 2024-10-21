// This code is part of lilrcc project -> https://gitlab.com/pp2e/lilrcc
#include "lilrcc.h"
#include "resourcereader.h"
#include "resourcewriter.h"
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
                                                                          "header\n"
                                                                          "entries\n"
                                                                          "names\n"
                                                                          "ls [path]\n"
                                                                          "cat <file>\n"
                                                                          "tree\n"
                                                                          "rm <file>\n"
                                                                          "mv <source> <dest>\n"
                                                                          "add <source> <dest>\n"
                                                                          "repack\n"));
    parser.addPositionalArgument(QStringLiteral("[<args>]"), QStringLiteral("Arguments for command"));

    parser.process(app);

    QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        qCritical() << "Please specify file";
        parser.showHelp(1);
    }
    QString inFile = args.first();
    QFile file(inFile);
    if (!file.exists()) {
        qCritical() << "File" << inFile << "does not exist";
        return 1;
    }
    if (args.size() < 2) {
        qCritical() << "Please specity action";
        parser.showHelp(1);
    }
    file.open(QIODeviceBase::ReadOnly);
    ResourceReader reader(&file);
    QTextStream out(stdout);
    if (args[1] == "header") {
        reader.printHeader(out);
        return 0;
    }
    if (args[1] == "entries") {
        reader.printEntries(out);
        return 0;
    }
    if (args[1] == "names") {
        reader.printNames(out);
        return 0;
    }
    ResourceLibrary lillib(&reader);
    if (args[1] == "cat") {
        ASSERT(args.size() >= 3, "Please specify path to file after cat option")
        Lilrcc::Error error;
        out << lillib.getFile(args[2], error);
        if (error != Lilrcc::NoError) {
            printError(error);
            return 1;
        }
    } else if (args[1] == "ls") {
        QString path = args.size() < 3 ? "/" : args[2];
        Lilrcc::Error error;
        qDebug() << lillib.ls(path, error).join("\n");
        if (error != Lilrcc::NoError) {
            printError(error);
            return 1;
        }
    } else if (args[1] == "tree") {
        lillib.printTree(out);
    } else if (args[1] == "rm") {
        if (args.size() < 3) {
            qCritical() << "Please specify path to file after rm option\n";
            return 1;
        }
        Lilrcc::Error error;
        lillib.rmFile(args[2], error);
        if (error != Lilrcc::NoError) {
            printError(error);
            return 1;
        }
        ResourceWriter writer(out.device());
        lillib.save(&writer);
    } else if (args[1] == "mv") {
        if (args.size() < 3) {
            qCritical() << "Please specify path to source entry after mv option\n";
            return 1;
        }
        if (args.size() < 4) {
            qCritical() << "Please specify path to destination directory after mv option\n";
            return 1;
        }
        Lilrcc::Error error;
        lillib.mvFile(args[2], args[3], error);
        if (error != Lilrcc::NoError) {
            printError(error);
            return 1;
        }
        ResourceWriter writer(out.device());
        lillib.save(&writer);
    } else if (args[1] == "add") {
        if (args.size() < 3) {
            qCritical() << "Please specify path to source file after add option\n";
            return 1;
        }
        QFile addFile(args[2]);
        addFile.open(QIODeviceBase::ReadOnly);
        if (!addFile.exists()) {
            qCritical() << "File does not exist";
            return 1;
        }
        QString addFileName = args[2];
        addFileName = addFileName.mid(addFileName.lastIndexOf("/")+1);

        if (args.size() < 4) {
            qCritical() << "Please specify path to destination directory after add option\n";
            return 1;
        }
        Lilrcc::Error error;
        lillib.addFile(addFile.readAll(), addFileName, args[3], error);
        if (error != Lilrcc::NoError) {
            printError(error);
            return 1;
        }
        ResourceWriter writer(out.device());
        lillib.save(&writer);
    } else if (args[1] == "repack") {
        ResourceWriter writer(out.device());
        lillib.save(&writer);
    } else {
        qCritical() << "Unknown action specified, please select smarter";
        parser.showHelp(1);
    }
    return 0;
}
