// Хранение измерений: JSON (между запусками) и экспорт в CSV.
#pragma once

#include "core/baseline.h"

#include <QList>
#include <QString>

namespace baseline_store {

// Каждая функция возвращает текст ошибки; пустая строка — успех
QString writeCsv(const QString &path, const QList<Baseline> &items);
// Сохраняет атомарно: сбой записи не портит прежний файл
QString saveJson(const QString &path, const QList<Baseline> &items);
// Пустой список, если файла нет; ошибка — если он не читается или повреждён
QString loadJson(const QString &path, QList<Baseline> *items);

}  // namespace baseline_store
