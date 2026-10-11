#include "ui/location_popover.h"

#include "ui/style.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Страница настроек геолокации ОС (нет для Linux — кнопка не показывается)
#if defined(Q_OS_MACOS)
const char *const SettingsUrl = "x-apple.systempreferences:com.apple.preference.security?Privacy_LocationServices";
#elif defined(Q_OS_WIN)
const char *const SettingsUrl = "ms-settings:privacy-location";
#else
const char *const SettingsUrl = nullptr;
#endif

constexpr int Width = 240;

}  // namespace

LocationPopover::LocationPopover(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);  // скруглённые углы карточки
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("popover"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(card);

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(6);
    m_title = style::roleLabel(QString(), "section");
    m_text = style::roleLabel(QString(), "caption");
    m_text->setWordWrap(true);
    m_text->setFixedWidth(Width - 24);
    lay->addWidget(m_title);
    lay->addWidget(m_text);

    auto *buttons = new QHBoxLayout;
    if (SettingsUrl) {
        auto *settings = new QPushButton(tr("Settings"));
        settings->setToolTip(tr("Open the location settings of the OS"));
        settings->setProperty("variant", QStringLiteral("primary"));
        connect(settings, &QPushButton::clicked, this, [this] {
            QDesktopServices::openUrl(QUrl(QString::fromLatin1(SettingsUrl)));
            close();
        });
        buttons->addWidget(settings);
    }
    auto *retry = new QPushButton(tr("Retry"));
    connect(retry, &QPushButton::clicked, this, [this] {
        emit retryRequested();
        close();
    });
    buttons->addWidget(retry);
    buttons->addStretch();
    lay->addLayout(buttons);
}

void LocationPopover::showAt(QWidget *anchor, LocationLevel level, const QString &text)
{
    m_title->setText(level == LocationLevel::Error ? tr("No location") : tr("Locating…"));
    m_text->setText(text.isEmpty() ? tr("Waiting for location…") : text);
    adjustSize();
    const QPoint corner = anchor->mapToGlobal(QPoint(anchor->width(), anchor->height() + 6));
    move(corner.x() - width(), corner.y());
    show();
}
