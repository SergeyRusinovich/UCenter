// Форматирование чисел для показа: десятичная точка независимо от локали ОС.
#pragma once

#include <QString>

#include <optional>

namespace fmt {

inline const QString Empty = QStringLiteral("—");

inline QString fixed(double v, int prec)
{
    return QString::number(v, 'f', prec);
}

// Со знаком всегда: «+1.250», «-0.300»
inline QString signedFixed(double v, int prec)
{
    const QString s = fixed(v, prec);
    return s.startsWith(u'-') ? s : u'+' + s;
}

// «—», если значения ещё нет
template <class T>
QString orEmpty(const std::optional<T> &v, int prec)
{
    return v ? fixed(double(*v), prec) : Empty;
}

inline QString orEmpty(const std::optional<int> &v)
{
    return v ? QString::number(*v) : Empty;
}

}  // namespace fmt
