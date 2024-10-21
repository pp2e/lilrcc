#include "error.h"

void Lilrcc::printError(Error error) {
    switch (error) {
    case NoError:
        qDebug() << "Lilrcc: No error";
        break;
    case InputFileIsNotRcc:
        qCritical() << "Lilrcc: File tried open is not rcc";
        break;
    case CannotUncompress:
        qCritical() << "Lilrcc: Cannot read compressed data";
        break;
    case GotFileInsteadOfDir:
        qCritical() << "Lilrcc: Got file instead of the dir";
        break;
    case EntryNotFound:
        qCritical() << "Lilrcc: Entry not found";
        break;
    case GotDirInsteadOfFile:
        qCritical() << "Lilrcc: Got directory instead of the file";
        break;
    default:
        qDebug() << "Could not find error" << error;
    }
}
