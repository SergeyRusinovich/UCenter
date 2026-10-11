#include "ui/filters.h"

#include "ui/style.h"
#include "ui/theme.h"

#include <QBoxLayout>
#include <QLabel>
#include <QPainter>

namespace {

constexpr int Box = 16;

}  // namespace

ColorCheckBox::ColorCheckBox(const QString &text, const QColor &color, QWidget *parent)
    : QCheckBox(text, parent)
    , m_color(color)
{}

QSize ColorCheckBox::sizeHint() const
{
    const int w = fontMetrics().horizontalAdvance(text()) + Box + 12;
    return {w, std::max(Box + 6, fontMetrics().height() + 4)};
}

void ColorCheckBox::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box(1, (height() - Box) / 2.0, Box, Box);
    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        p.drawRoundedRect(box, 3, 3);
        p.setPen(QPen(QColor("#222"), 2));
        p.setBrush(Qt::NoBrush);
        const double x = box.x(), y = box.y();
        const QPointF tick[] = {{x + 3.5, y + 8.5}, {x + 6.5, y + 11.5}, {x + 12.5, y + 4.5}};
        p.drawPolyline(tick, 3);
    } else {
        p.setPen(QPen(palette().mid().color(), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, 3, 3);
    }
    p.setPen(palette().text().color());
    p.drawText(QRectF(Box + 8, 0, width() - Box - 8, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}

ConstellationFilters::ConstellationFilters(bool vertical, bool showCounts, QWidget *parent)
    : QWidget(parent)
{
    m_showLostCb = new ColorCheckBox(tr("Show not tracked"), theme::lostColor());
    m_showLostCb->setChecked(true);
    connect(m_showLostCb, &QCheckBox::toggled, this, &ConstellationFilters::changed);

    QBoxLayout *lay = nullptr;
    if (vertical) {
        lay = new QVBoxLayout(this);
        lay->addWidget(style::roleLabel(tr("Filter satellites"), "section"));
        lay->addWidget(m_showLostCb);
        lay->addSpacing(8);
        lay->addWidget(style::roleLabel(tr("GNSS constellation"), "section"));
    } else {
        lay = new QHBoxLayout(this);
        lay->addWidget(m_showLostCb);
    }

    for (const auto &[ch, name] : systemNames()) {
        auto *cb = new ColorCheckBox(name, theme::systemColor(ch));
        cb->setChecked(true);
        connect(cb, &QCheckBox::toggled, this, &ConstellationFilters::changed);
        m_sysCbs.insert(ch, cb);
        if (showCounts) {
            auto *row = new QHBoxLayout;
            auto *cnt = new QLabel(QStringLiteral("0/0"));
            row->addWidget(cb, 1);
            row->addWidget(cnt);
            lay->addLayout(row);
            m_sysCounts.insert(ch, cnt);
        } else {
            lay->addWidget(cb);
        }
    }
    lay->addStretch();
}

bool ConstellationFilters::accepts(const Satellite &sat) const
{
    const ColorCheckBox *cb = m_sysCbs.value(sat.sys);
    const bool tracked = sat.cno && *sat.cno;
    return cb && cb->isChecked() && (tracked || m_showLostCb->isChecked());
}

void ConstellationFilters::setCounts(const QHash<char, std::pair<int, int>> &counts)
{
    for (auto it = m_sysCounts.cbegin(); it != m_sysCounts.cend(); ++it) {
        const auto [used, visible] = counts.value(it.key(), {0, 0});
        it.value()->setText(QStringLiteral("%1/%2").arg(used).arg(visible));
    }
}
