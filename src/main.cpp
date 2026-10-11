// Точка входа: мини-аналог u-center для u-blox M9.
#include "core/language.h"
#include "ui/main_window.h"
#include "ui/style.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

#include <functional>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Задаёт каталоги данных (measurements.json), кэша тайлов и настроек
    QApplication::setApplicationName(QStringLiteral("UCenter"));
#ifdef Q_OS_WIN
    // Без организации QSettings в Windows ничего не сохраняет (реестр: Software\<организация>\<приложение>).
    // В macOS не задаём: иначе сменились бы уже используемые пути данных и настроек
    QApplication::setOrganizationName(QStringLiteral("UCenter"));
#endif
    QApplication::setApplicationVersion(QStringLiteral(UCENTER_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/ucenter.png")));
    QApplication::setStyle(QStringLiteral("Fusion"));  // одинаковая основа на macOS и Windows, остальное — в QSS
    language::apply(app, language::current());
    style::applyTheme(app);

    // Воспроизведение из командной строки — для замеров производительности (docs/roadmap.md, фаза 1)
    QCommandLineParser args;
    args.addHelpOption();
    args.addVersionOption();
    const QCommandLineOption replayOpt(QStringLiteral("replay"), QStringLiteral("Replay a recording on start."),
                                       QStringLiteral("file"));
    const QCommandLineOption speedOpt(QStringLiteral("speed"),
                                      QStringLiteral("Replay speed, times real time (default 1)."), QStringLiteral("x"),
                                      QStringLiteral("1"));
    const QCommandLineOption quitOpt(QStringLiteral("quit-at-end"), QStringLiteral("Quit when the replay ends."));
    args.addOptions({replayOpt, speedOpt, quitOpt});
    args.process(app);

    // Тексты виджетов задаются при создании, поэтому новый язык применяется к новому окну:
    // оно открывается раньше, чем закрывается старое, иначе приложение завершилось бы
    std::function<MainWindow *()> openWindow = [&] {
        auto *win = new MainWindow;
        win->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(win, &MainWindow::languageChangeRequested, &app, [&, win](const QString &choice) {
            language::save(choice);
            language::apply(app, choice);
            win->saveSettings();  // новое окно откроется с тем же размером и вкладкой
            openWindow();
            win->close();
        });
        win->show();
        return win;
    };
    MainWindow *first = openWindow();
    if (args.isSet(replayOpt))
        first->startReplay(args.value(replayOpt), args.value(speedOpt).toDouble(), args.isSet(quitOpt));
    return app.exec();
}
