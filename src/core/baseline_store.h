// Хранение измерений: JSON (между запусками) и экспорт в CSV.
#pragma once

#include "core/baseline.h"

#include <QList>
#include <QString>

namespace baseline_store {

// Формат measurements.json (ADR 0021): 1 — список замеров (Python-версия и сборки до 1.2), 2 — объект
// {"format": "ucenter-measurements", "version": 2, "measurements": [...]}. Читаются оба, пишется текущий
inline constexpr int FormatVersion = 2;

// Каждая функция возвращает текст ошибки; пустая строка — успех
QString writeCsv(const QString &path, const QList<Baseline> &items);
// Сохраняет атомарно: сбой записи не портит прежний файл
QString saveJson(const QString &path, const QList<Baseline> &items);
// Пустой список, если файла нет; ошибка — если он не читается, повреждён или записан более новой версией
QString loadJson(const QString &path, QList<Baseline> *items);
// Нечитаемый файл — в сторону, рядом: «<имя>.unreadable-ГГГГММДД-ЧЧММСС», чтобы следующее сохранение
// его не затёрло. Возвращает новый путь; пустая строка — переименовать не удалось (*error — почему)
QString setAside(const QString &path, QString *error);
// Переносит файл из прежнего каталога данных (ADR 0016), если в новом его ещё нет; существующий не перезаписывается
QString moveFromLegacy(const QString &legacyPath, const QString &path);

}  // namespace baseline_store
