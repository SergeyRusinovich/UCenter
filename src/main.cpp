// Точка входа: мини-аналог u-center для u-blox M9.
#include "core/language.h"
#include "ui/main_window.h"
#include "ui/style.h"

#include <QApplication>
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

    // Тексты виджетов задаются при создании, поэтому новый язык применяется к новому окну:
    // оно открывается раньше, чем закрывается старое, иначе приложение завершилось бы
    std::function<void()> openWindow = [&] {
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
    };
    openWindow();
    return app.exec();
}
