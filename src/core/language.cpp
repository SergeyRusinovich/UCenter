#include "core/language.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace language {

namespace {

const char *const Setting = "ui/language";

// Установленные переводчики: при смене языка их нужно снять
QList<QTranslator *> &installed()
{
    static QList<QTranslator *> list;
    return list;
}

}  // namespace

const QList<Language> &available()
{
    static const QList<Language> list = {
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("ru"), QStringLiteral("Русский")},
        {QStringLiteral("es"), QStringLiteral("Español")},
        {QStringLiteral("zh_CN"), QStringLiteral("中文（简体）")},
    };
    return list;
}

bool isAvailable(const QString &code)
{
    for (const Language &lang : available())
        if (lang.code == code)
            return true;
    return false;
}

QString current()
{
    const QString code = QSettings().value(QLatin1String(Setting)).toString();
    return isAvailable(code) ? code : Default;
}

void save(const QString &code)
{
    QSettings().setValue(QLatin1String(Setting), code);
}

void apply(QCoreApplication &app, const QString &code)
{
    for (QTranslator *t : std::as_const(installed())) {
        QCoreApplication::removeTranslator(t);
        delete t;
    }
    installed().clear();
    QLocale::setDefault(QLocale(code));
    if (code == English)
        return;
    for (const char *name : {"ucenter", "qtbase"}) {
        auto *translator = new QTranslator(&app);
        const QString file = QStringLiteral(":/i18n/%1_%2.qm").arg(QLatin1String(name), code);
        if (translator->load(file)) {
            QCoreApplication::installTranslator(translator);
            installed().append(translator);
        } else {
            delete translator;
        }
    }
}

}  // namespace language
