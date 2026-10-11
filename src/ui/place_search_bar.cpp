#include "ui/place_search_bar.h"

#include "ui/geocoder.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int MenuTextW = 520;

QString coords(double lat, double lon)
{
    return QStringLiteral("%1, %2").arg(lat, 0, 'f', 7).arg(lon, 0, 'f', 7);
}

}  // namespace

PlaceSearchBar::PlaceSearchBar(std::function<Bounds()> viewBounds, QWidget *parent)
    : QWidget(parent)
    , m_viewBounds(std::move(viewBounds))
    , m_search(new PlaceSearch(this))
{
    connect(m_search, &PlaceSearch::results, this, &PlaceSearchBar::onResults);
    connect(m_search, &PlaceSearch::failed, this, &PlaceSearchBar::setStatus);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *row = new QHBoxLayout;
    m_edit = new QLineEdit;
    m_edit->setPlaceholderText(tr("Search for a place: address, street, city, landmark… (Enter)"));
    m_edit->setClearButtonEnabled(true);
    m_edit->setToolTip(PlaceSearch::attribution());
    connect(m_edit, &QLineEdit::returnPressed, this, &PlaceSearchBar::startSearch);
    connect(m_edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty())
            select(std::nullopt);
    });
    row->addWidget(m_edit, 1);
    for (PointKey key : PointKeys) {
        auto *btn = new QPushButton(QStringLiteral("→ ") + pointName(key));
        btn->setToolTip(tr("Put point %1 on the found place").arg(pointName(key)));
        connect(btn, &QPushButton::clicked, this, [this, key] { requestPoint(key); });
        btn->setEnabled(false);
        row->addWidget(btn);
        m_pointButtons.append(btn);
    }
    lay->addLayout(row);
    m_status = style::roleLabel(QString(), "caption");
    lay->addWidget(m_status);
}

void PlaceSearchBar::movePlace(double lat, double lon)
{
    if (!m_place)
        return;
    m_place->lat = lat;
    m_place->lon = lon;
    m_status->setToolTip(tr("%1\n%2 (flag moved)").arg(m_place->address, coords(lat, lon)));
}

void PlaceSearchBar::setStatus(const QString &text)
{
    m_status->setText(text);
}

void PlaceSearchBar::startSearch()
{
    if (m_edit->text().trimmed().isEmpty())
        return;
    setStatus(tr("Searching…"));
    m_search->search(m_edit->text(), m_viewBounds());
}

void PlaceSearchBar::onResults(const QString &text, const QList<Place> &found)
{
    if (text != m_edit->text().trimmed())
        return;  // пока ждали ответ, запрос уже изменили
    if (found.isEmpty()) {
        setStatus(tr("Nothing found: “%1”").arg(text));
        return;
    }
    if (found.size() == 1) {
        select(found.first());
        return;
    }
    const Bounds b = m_viewBounds();
    const QList<Place> sorted = places::sortByDistance(found, {(b.south + b.north) / 2, (b.west + b.east) / 2});
    setStatus(tr("Found: %1 — choose a place").arg(sorted.size()));
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    for (const Place &place : sorted) {
        QAction *action = menu->addAction(menuText(place));
        action->setToolTip(place.address);
        connect(action, &QAction::triggered, this, [this, place] { select(place); });
    }
    menu->setToolTipsVisible(true);
    menu->popup(m_edit->mapToGlobal(m_edit->rect().bottomLeft()));
}

QString PlaceSearchBar::menuText(const Place &place) const
{
    QString rest = place.address;
    if (rest.startsWith(place.name))
        rest.remove(0, place.name.size());
    while (rest.startsWith(u',') || rest.startsWith(u' '))
        rest.remove(0, 1);
    const QString text = rest.isEmpty() ? place.name : place.name + QStringLiteral(" — ") + rest;
    return fontMetrics().elidedText(text, Qt::ElideRight, MenuTextW);
}

void PlaceSearchBar::select(const std::optional<Place> &place)
{
    m_place = place;
    for (QPushButton *btn : std::as_const(m_pointButtons))
        btn->setEnabled(place.has_value());
    if (!place) {
        setStatus(QString());
    } else {
        setStatus(fontMetrics().elidedText(place->address, Qt::ElideRight, std::max(120, m_status->width())));
        m_status->setToolTip(QStringLiteral("%1\n%2").arg(place->address, coords(place->lat, place->lon)));
    }
    emit placeChanged(place);
}

void PlaceSearchBar::requestPoint(PointKey key)
{
    if (m_place)
        emit pointRequested(key, m_place->lat, m_place->lon);
}
