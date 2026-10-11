// Язык интерфейса: список поддерживаемых языков, выбор пользователя (язык системы не учитывается),
// установка переводов (строк приложения и стандартных диалогов Qt).
#pragma once

#include <QList>
#include <QString>

class QCoreApplication;

namespace language {

struct Language {
    QString code;        // суффикс файлов перевода: ucenter_<code>.qm, qtbase_<code>.qm
    QString nativeName;  // название на самом языке — так его узнают в списке
};

// Английский — язык исходных строк, ему файлы перевода не нужны
inline const QString English = QStringLiteral("en");
// Язык при первом запуске, пока пользователь ничего не выбрал
inline const QString Default = English;

// Поддерживаемые языки в порядке списка выбора
const QList<Language> &available();
bool isAvailable(const QString &code);

// Выбранный язык из настроек; если выбора нет или он неизвестен — Default
QString current();
void save(const QString &code);

// Ставит переводы для языка code (снимая прежние) и локаль по умолчанию.
// Переводы ищутся в ресурсах приложения: :/i18n/ucenter_<code>.qm и :/i18n/qtbase_<code>.qm
void apply(QCoreApplication &app, const QString &code);

}  // namespace language
