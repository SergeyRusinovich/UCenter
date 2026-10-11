// Сохраняет эталонную запись в файл, чтобы воспроизвести её в приложении (дорожная карта, фаза 1):
//   build/make_recording reference.ubx [число эпох]
#include "recording.h"

#include <QCoreApplication>
#include <QFile>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2 || args.size() > 3) {
        std::fprintf(stderr, "usage: make_recording <out.ubx> [epochs, default %d]\n", recording::ReferenceEpochs);
        return 2;
    }
    bool ok = true;
    const int epochs = args.size() == 3 ? args[2].toInt(&ok) : recording::ReferenceEpochs;
    if (!ok || epochs <= 0) {
        std::fprintf(stderr, "epochs must be a positive number\n");
        return 2;
    }
    QFile file(args[1]);
    if (!file.open(QIODevice::WriteOnly) || file.write(recording::reference(epochs)) < 0) {
        std::fprintf(stderr, "cannot write %s: %s\n", qPrintable(args[1]), qPrintable(file.errorString()));
        return 1;
    }
    return 0;
}
