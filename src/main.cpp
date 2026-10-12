// Точка входа: мини-аналог u-center для u-blox M9.
#include "core/cpu_time.h"
#include "core/language.h"
#include "ui/main_window.h"
#include "ui/style.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QIcon>
#include <QTimer>
#include <QWindow>

#include <cstdio>
#include <functional>
#include <utility>

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
    const QCommandLineOption measureOpt(
        QStringLiteral("measure"),
        QStringLiteral("Measure CPU load between two moments after start, in seconds, then quit."),
        QStringLiteral("from,to"));
    const QCommandLineOption measureOutOpt(
        QStringLiteral("measure-out"), QStringLiteral("Append the --measure result to this file (default: stderr)."),
        QStringLiteral("file"));
    const QCommandLineOption tabOpt(QStringLiteral("tab"),
                                    QStringLiteral("Open this tab on start (0 = Console ... 4 = Configuration)."),
                                    QStringLiteral("index"));
    args.addOptions({replayOpt, speedOpt, quitOpt, measureOpt, measureOutOpt, tabOpt});
    args.process(app);
    QElapsedTimer sinceStart;
    sinceStart.start();

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
    first->setKeepSettings(!args.isSet(tabOpt) && !args.isSet(measureOpt));
    if (args.isSet(tabOpt))
        first->showTab(args.value(tabOpt).toInt());
    if (args.isSet(replayOpt))
        first->startReplay(args.value(replayOpt), args.value(speedOpt).toDouble(), args.isSet(quitOpt));
    if (args.isSet(measureOpt)) {
        // Нагрузка в установившемся режиме: от from до to секунд после запуска, без затрат на сам запуск.
        // Таймеры срабатывают в GUI-потоке, поэтому cpu_time::thread() — время GUI-потока
        const QStringList span = args.value(measureOpt).split(u',');
        const double from = span.value(0).toDouble(), to = span.value(1).toDouble();
        if (span.size() != 2 || from < 0 || to <= from)
            args.showHelp(1);
        struct Sample {
            double wall, gui, all;
        };
        const auto sample = [&sinceStart] {
            return Sample{double(sinceStart.nsecsElapsed()) * 1e-9, cpu_time::thread(), cpu_time::process()};
        };
        auto *start = new Sample(sample());
        // Перекрытое окно система не рисует и притормаживает: замер был бы занижен.
        // Окно поднимается наверх, а в отчёт идёт доля проверок (раз в секунду), когда его было видно
        first->raise();
        first->activateWindow();
        auto *visible = new std::pair<int, int>(0, 0);  // видно, всего проверок
        auto *probe = new QTimer(&app);
        QObject::connect(probe, &QTimer::timeout, &app, [first, visible] {
            visible->first += first->windowHandle() && first->windowHandle()->isExposed();
            ++visible->second;
        });
        QTimer::singleShot(int(from * 1000), &app, [start, sample, probe] {
            *start = sample();
            probe->start(1000);
        });
        QTimer::singleShot(
            int(to * 1000), &app, [start, sample, visible, probe, out = args.value(measureOutOpt), first] {
                probe->stop();  // до выхода: при выходе окно закрывается и удаляется
                const Sample end = sample();
                const double wall = end.wall - start->wall;
                const QString line = QStringLiteral("tab=%1 gui_thread=%2% all_threads=%3% span=%4s visible=%5%\n")
                                         .arg(first->currentTab())
                                         .arg(100 * (end.gui - start->gui) / wall, 0, 'f', 1)
                                         .arg(100 * (end.all - start->all) / wall, 0, 'f', 1)
                                         .arg(wall, 0, 'f', 1)
                                         .arg(visible->second ? 100 * visible->first / visible->second : 0);
                QFile file(out);
                if (!out.isEmpty() && file.open(QIODevice::Append | QIODevice::Text))
                    file.write(line.toUtf8());
                else
                    fputs(qPrintable(line), stderr);
                delete start;
                delete visible;
                QCoreApplication::quit();
            });
    }
    return app.exec();
}
