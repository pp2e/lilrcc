#include "error.h"

void Lilrcc::printError(Error error) {
    switch (error) {
    NoError:
        break;
    InputFileIsNotRcc:
        qCritical() << "Lilrcc: File tried open is not rcc";
        break;
    CannotUncompress:
        qCritical() << "Lilrcc: Cannot read compressed data";
        break;
    GotFileInsteadOfDir:
        qCritical() << "Lilrcc: Got file instead of the dir";
        break;
    EntryNotFound:
        qCritical() << "Lilrcc: Entry not found";
        break;
    GotDirInsteadOfFil:
        qCritical() << "Lilrcc: Got directory instead of the file";
        break;
    }
}
