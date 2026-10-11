// Тайловая карта (OSM или спутник; только виджеты, без WebEngine): отрисовка и управление.
#pragma once

#include "core/geo.h"
#include "ui/tile_loader.h"

#include <QList>
#include <QPushButton>
#include <QWidget>

#include <optional>

class CrosshairButton;

class MapView : public QWidget
{
    Q_OBJECT

public:
    static constexpr int MinZoom = 3, MaxZoom = 19;
    static constexpr int FixZoom = 16;      // масштаб, с которым карта открывается на первой позиции
    static constexpr double ClickSlop = 4;  // px: сдвиг мыши меньше этого — щелчок, а не перетаскивание
    static constexpr int WheelStep = 120;   // angleDelta одного щелчка колеса

    // needFix=false — карта доступна и до первой позиции (показывает мир)
    explicit MapView(QWidget *parent = nullptr, bool needFix = true, const TileSource &source = tiles::osm());

    int zoom() const { return m_zoom; }
    LatLon center() const { return m_center; }
    // Восстановить вид (центр и масштаб), например сохранённый с прошлого запуска
    void setView(LatLon center, int zoom);
    void setPosition(std::optional<double> lat, std::optional<double> lon);
    void setSource(const TileSource &source);
    // Карта больше не уезжает за позицией (пользователь работает с ней сам)
    void stopFollowing() { m_follow = false; }
    void centerOn(double lat, double lon);
    // Показывает прямоугольник целиком: центр в его середине, наибольший подходящий масштаб
    void fitBounds(const Bounds &b, int maxZoom = 18);
    // Видимая область
    Bounds visibleBounds() const;
    QPointF toScreen(double lat, double lon) const;
    QPointF toScreen(LatLon ll) const { return toScreen(ll.lat, ll.lon); }
    LatLon toGeo(const QPointF &pt) const;

signals:
    void clicked(double lat, double lon);  // точка щелчка (без перетаскивания)
    void recentered();                     // нажат «прицел»: вернуться к текущей позиции

protected:
    template <class Button = QPushButton, class Slot>
    Button *makeButton(const QString &text, Slot slot)
    {
        auto *b = new Button(text, this);
        b->setObjectName(QStringLiteral("mapButton"));
        b->setFixedSize(28, 28);
        connect(b, &QPushButton::clicked, this, slot);
        return b;
    }

    // Дополнительные слои поверх тайлов (для наследников)
    virtual void paintOverlay(QPainter &) {}

    void resizeEvent(QResizeEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

    std::optional<LatLon> m_pos;
    CrosshairButton *m_btnCenter;
    QList<QPushButton *> m_buttons;  // столбец справа сверху

private:
    QPointF origin() const;  // мировые пиксели левого верхнего угла виджета
    bool ready() const { return m_pos.has_value() || !m_needFix; }
    // anchor — экранная точка, которая остаётся на месте (курсор); без неё — центр
    void setZoom(int z, std::optional<QPointF> anchor = std::nullopt);
    void recenter();
    void resetTiles(const TileSource &source);
    void paintTiles(QPainter &p);
    // Тайл (z, x, y) в прямоугольник target; false — его ещё нет
    bool drawTile(QPainter &p, const QRectF &target, int z, int x, int y, QSet<QString> *wanted);
    // Пока тайла нет — растянутый кусок тайла уровнем ниже из кэша
    void drawFallback(QPainter &p, const QRectF &target, int z, int x, int y);

    bool m_needFix;
    int m_zoom;
    bool m_follow = true;
    LatLon m_center;
    int m_wheelAccum = 0;  // накопленная прокрутка: тачпад шлёт много мелких событий
    std::optional<QPointF> m_dragLast;
    std::optional<QPointF> m_pressAt;
    TileLoader *m_tiles = nullptr;
};
