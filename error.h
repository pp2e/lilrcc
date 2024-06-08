#ifndef ERROR_H
#define ERROR_H

#include <QDebug>

namespace Lilrcc {

enum Error {
    NoError = 0,
    InputFileIsNotRcc,
    CannotUncompress,
    GotFileInsteadOfDir,
    EntryNotFound,
    GotDirInsteadOfFile
};

void printError(Error error);

}

#endif // ERROR_H
