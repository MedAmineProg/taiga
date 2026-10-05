/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "home_widget.hpp"

#include <QAction>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <chrono>

#include "base/string.hpp"
#include "gui/common/clickable_label.hpp"
#include "gui/common/poster_widget.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/media/media_menu.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"
#include "media/home_feed.hpp"
#include "taiga/accounts.hpp"
#include "taiga/settings.hpp"
#include "track/library_index.hpp"
#include "track/play.hpp"

namespace gui {

namespace {

constexpr int kMaxContinueItems = 20;
constexpr int kMaxAiringItems = 12;
constexpr auto kAiringWindow = std::chrono::days{7};

constexpr QSize kCardPosterSize{150, 212};
constexpr QSize kRowPosterSize{40, 56};
constexpr int kCardSpacing = 16;
constexpr int kPageMargin = 24;

// A thin rounded bar showing watched episodes out of the total.
class EpisodeProgressBar final : public QWidget {
public:
  EpisodeProgressBar(QWidget* parent, double ratio) : QWidget(parent), m_ratio(ratio) {
    setFixedHeight(4);
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const qreal radius = height() / 2.0;
    auto track = palette().color(QPalette::WindowText);
    track.setAlphaF(0.12);
    painter.setBrush(track);
    painter.drawRoundedRect(rect(), radius, radius);

    if (m_ratio > 0) {
      auto fill = rect();
      fill.setWidth(std::max(static_cast<int>(width() * std::min(m_ratio, 1.0)), height()));
      painter.setBrush(palette().color(QPalette::Highlight));
      painter.drawRoundedRect(fill, radius, radius);
    }
  }

private:
  double m_ratio = 0;
};

QLabel* createBadge(QWidget* parent, const QString& text, const QColor& color) {
  auto badge = new QLabel(text, parent);
  badge->setStyleSheet(
      u"QLabel { background-color: %1; color: white; border-radius: 4px;"
      " padding: 2px 6px; font-weight: 600; }"_s.arg(color.name(QColor::HexArgb)));
  badge->adjustSize();
  return badge;
}

QString greeting() {
  const int hour = QTime::currentTime().hour();
  if (hour < 5) return HomeWidget::tr("Good night");
  if (hour < 12) return HomeWidget::tr("Good morning");
  if (hour < 18) return HomeWidget::tr("Good afternoon");
  return HomeWidget::tr("Good evening");
}

void openDetails(QWidget* parent, int animeId) {
  if (const auto item = anime::db.item(animeId)) {
    MediaDialog::show(parent, MediaDialogPage::Details, *item);
  }
}

void showMenu(QWidget* parent, int animeId) {
  const auto item = anime::db.item(animeId);
  if (!item) return;

  QMap<int, ListEntry> entries;
  if (const auto entry = anime::db.entry(animeId)) entries[animeId] = *entry;

  auto menu = new MediaMenu(parent, {*item}, entries, nullptr, AnimeListContext::List);
  menu->popup();
}

}  // namespace

HomeWidget::HomeWidget(QWidget* parent) : PageWidget(parent) {
  // Toolbar
  {
    auto actionScan = new QAction(theme.getIcon("pageview"), tr("Scan library folders"), this);
    actionScan->setToolTip(tr("Look for new episodes in library folders"));
    connect(actionScan, &QAction::triggered, track::libraryIndex(), &track::LibraryIndex::scan);
    m_toolbar->addAction(actionScan);
  }

  // Scrollable content
  auto scrollArea = new QScrollArea(this);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout()->addWidget(scrollArea);

  m_content = new QWidget(scrollArea);
  m_contentLayout = new QVBoxLayout(m_content);
  m_contentLayout->setContentsMargins(kPageMargin, 0, kPageMargin, kPageMargin);
  m_contentLayout->setSpacing(8);
  scrollArea->setWidget(m_content);

  // Coalesce bursts of changes (e.g. a list sync updating every entry)
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(std::chrono::milliseconds{250});
  connect(m_refreshTimer, &QTimer::timeout, this, &HomeWidget::refresh);

  m_countdownTimer = new QTimer(this);
  m_countdownTimer->setInterval(std::chrono::seconds{30});
  connect(m_countdownTimer, &QTimer::timeout, this, &HomeWidget::updateCountdowns);

  connect(&anime::db, &anime::Database::entryUpdated, this, &HomeWidget::scheduleRefresh);
  connect(&anime::db, &anime::Database::entryDeleted, this, &HomeWidget::scheduleRefresh);
  connect(&anime::db, &anime::Database::itemUpdated, this, &HomeWidget::scheduleRefresh);
  connect(track::libraryIndex(), &track::LibraryIndex::scanStarted, this,
          &HomeWidget::scheduleRefresh);
  connect(track::libraryIndex(), &track::LibraryIndex::updated, this, &HomeWidget::scheduleRefresh);
  connect(&imageProvider, &ImageProvider::posterChanged, this, &HomeWidget::updatePoster);
}

void HomeWidget::showEvent(QShowEvent* event) {
  PageWidget::showEvent(event);
  track::libraryIndex()->scanIfStale();
  refresh();
  m_countdownTimer->start();
}

void HomeWidget::scheduleRefresh() {
  // Pages that aren't visible are refreshed when they're shown.
  if (isVisible()) m_refreshTimer->start();
}

void HomeWidget::refresh() {
  m_posters.clear();
  m_countdowns.clear();

  // Rebuild the content from scratch, as sections are small.
  while (const auto item = m_contentLayout->takeAt(0)) {
    if (const auto widget = item->widget()) widget->deleteLater();
    delete item;
  }

  // Header
  {
    const auto username = taiga::accounts.serviceUsername(taiga::settings.service());
    auto title = new QLabel(
        username.empty() ? greeting()
                         : tr("%1, %2").arg(greeting()).arg(QString::fromStdString(username)),
        m_content);
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.8);
    font.setWeight(QFont::DemiBold);
    title->setFont(font);
    m_contentLayout->addWidget(title);

    m_summaryLabel = new QLabel(m_content);
    m_summaryLabel->setForegroundRole(QPalette::PlaceholderText);
    m_contentLayout->addWidget(m_summaryLabel);
    m_contentLayout->addSpacing(16);
  }

  m_contentLayout->addWidget(createContinueSection());
  m_contentLayout->addSpacing(24);
  m_contentLayout->addWidget(createAiringSection());
  m_contentLayout->addStretch();
}

QWidget* HomeWidget::createContinueSection() {
  const auto& available = track::libraryIndex()->episodes();
  const auto items = anime::home::continueWatching(anime::db.items(), anime::db.entries(),
                                                   available, kMaxContinueItems);

  // Summary
  {
    int ready = 0;
    for (const auto& item : items) {
      if (item.next_episode_available) ready += item.available_count;
    }
    const auto now = std::time(nullptr);
    const auto airingToday =
        static_cast<int>(anime::home::airingSoon(anime::db.items(), anime::db.entries(), now,
                                                 std::chrono::days{1}, kMaxAiringItems)
                             .size());

    QStringList parts;
    if (ready == 1) parts.append(tr("1 episode ready to watch"));
    if (ready > 1) parts.append(tr("%1 episodes ready to watch").arg(ready));
    if (airingToday == 1) parts.append(tr("1 episode airing in the next 24 hours"));
    if (airingToday > 1) {
      parts.append(tr("%1 episodes airing in the next 24 hours").arg(airingToday));
    }
    if (track::libraryIndex()->isScanning()) parts.append(tr("Scanning library folders..."));
    m_summaryLabel->setText(parts.isEmpty() ? tr("You're all caught up.") : parts.join(u" · "_s));
  }

  auto section = new QWidget(m_content);
  auto layout = new QVBoxLayout(section);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);
  layout->addWidget(createSectionTitle(tr("Continue watching")));

  if (items.empty()) {
    layout->addWidget(
        createEmptyLabel(tr("Nothing in progress. Anime you're watching will show up here.")));
    return section;
  }

  // Horizontally scrolling row of cards
  auto scrollArea = new QScrollArea(section);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidgetResizable(true);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setBackgroundRole(QPalette::Window);
  layout->addWidget(scrollArea);

  auto row = new QWidget(scrollArea);
  auto rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(kCardSpacing);

  for (const auto& entry : items) {
    const auto item = anime::db.item(entry.anime_id);
    if (!item) continue;
    const int id = entry.anime_id;

    auto card = new QWidget(row);
    card->setFixedWidth(kCardPosterSize.width());
    auto cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(6);

    // Poster with badges
    auto poster = createPoster(id, kCardPosterSize);
    poster->setParent(card);
    if (entry.next_episode_available) {
      const auto text = entry.available_count > 1 ? tr("%1 READY").arg(entry.available_count)
                                                  : tr("EP %1 READY").arg(entry.next_episode);
      createBadge(poster, text, theme.successColor())->move(8, 8);
    } else if (entry.aired_unwatched > 0) {
      createBadge(poster, tr("%1 NEW").arg(entry.aired_unwatched), theme.warningColor())
          ->move(8, 8);
    }
    cardLayout->addWidget(poster);

    // Title
    auto title = new ClickableLabel(card);
    title->setText(QString::fromStdString(anime::preferredTitle(*item)));
    title->setElidable(true);
    title->setCursor(Qt::PointingHandCursor);
    connect(title, &ClickableLabel::clicked, this, [this, id](Qt::MouseButton button) {
      if (button == Qt::LeftButton) openDetails(this, id);
      if (button == Qt::RightButton) showMenu(this, id);
    });
    cardLayout->addWidget(title);

    // Progress
    if (entry.episode_count > 0) {
      cardLayout->addWidget(new EpisodeProgressBar(
          card, static_cast<double>(entry.watched_episodes) / entry.episode_count));
    }
    auto progress = new QLabel(card);
    progress->setForegroundRole(QPalette::PlaceholderText);
    progress->setText(
        entry.episode_count > 0
            ? tr("Episode %1 of %2").arg(entry.watched_episodes).arg(entry.episode_count)
            : tr("Episode %1").arg(entry.watched_episodes));
    cardLayout->addWidget(progress);

    // Play
    if (entry.next_episode_available) {
      auto play = new QToolButton(card);
      play->setIcon(theme.getIcon("play_arrow"));
      play->setText(tr("Play episode %1").arg(entry.next_episode));
      play->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
      play->setAutoRaise(true);
      play->setCursor(Qt::PointingHandCursor);
      const int next = entry.next_episode;
      connect(play, &QToolButton::clicked, this, [id, next]() { track::playEpisode(id, next); });
      cardLayout->addWidget(play, 0, Qt::AlignLeft);
    }

    cardLayout->addStretch();
    rowLayout->addWidget(card, 0, Qt::AlignTop);
  }
  rowLayout->addStretch();

  scrollArea->setWidget(row);
  // Fit the tallest card, plus room for a scroll bar.
  scrollArea->setFixedHeight(row->sizeHint().height() +
                             scrollArea->horizontalScrollBar()->sizeHint().height());

  return section;
}

QWidget* HomeWidget::createAiringSection() {
  const auto now = std::time(nullptr);
  const auto items = anime::home::airingSoon(anime::db.items(), anime::db.entries(), now,
                                             kAiringWindow, kMaxAiringItems);

  auto section = new QWidget(m_content);
  auto layout = new QVBoxLayout(section);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  layout->addWidget(createSectionTitle(tr("Airing this week")));

  if (items.empty()) {
    layout->addWidget(
        createEmptyLabel(tr("No upcoming episodes for anime you're watching or "
                            "planning to watch.")));
    return section;
  }

  for (const auto& entry : items) {
    const auto item = anime::db.item(entry.anime_id);
    if (!item) continue;
    const int id = entry.anime_id;

    auto row = new QWidget(section);
    auto rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);

    rowLayout->addWidget(createPoster(id, kRowPosterSize));

    auto textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    auto title = new ClickableLabel(row);
    title->setText(QString::fromStdString(anime::preferredTitle(*item)));
    title->setElidable(true);
    title->setCursor(Qt::PointingHandCursor);
    connect(title, &ClickableLabel::clicked, this, [this, id](Qt::MouseButton button) {
      if (button == Qt::LeftButton) openDetails(this, id);
      if (button == Qt::RightButton) showMenu(this, id);
    });
    textLayout->addWidget(title);

    const auto airTime = QDateTime::fromSecsSinceEpoch(entry.time).toLocalTime();
    auto details = new QLabel(row);
    details->setForegroundRole(QPalette::PlaceholderText);
    details->setText(tr("Episode %1 · %2 · %3")
                         .arg(entry.episode)
                         .arg(QLocale().toString(airTime, u"ddd, HH:mm"_s))
                         .arg(formatListStatus(entry.status)));
    textLayout->addWidget(details);
    rowLayout->addLayout(textLayout, 1);

    auto countdown = new QLabel(row);
    auto font = countdown->font();
    font.setWeight(QFont::DemiBold);
    countdown->setFont(font);
    countdown->setToolTip(QLocale().toString(airTime, QLocale::LongFormat));
    rowLayout->addWidget(countdown);
    m_countdowns.append({countdown, entry.time});

    layout->addWidget(row);
  }

  updateCountdowns();

  return section;
}

QLabel* HomeWidget::createSectionTitle(const QString& text) {
  auto label = new QLabel(text, m_content);
  auto font = label->font();
  font.setPointSizeF(font.pointSizeF() * 1.25);
  font.setWeight(QFont::DemiBold);
  label->setFont(font);
  return label;
}

QLabel* HomeWidget::createEmptyLabel(const QString& text) {
  auto label = new QLabel(text, m_content);
  label->setForegroundRole(QPalette::PlaceholderText);
  label->setWordWrap(true);
  return label;
}

PosterWidget* HomeWidget::createPoster(int animeId, QSize size) {
  auto poster = new PosterWidget(m_content);
  poster->setFixedSize(size);
  poster->setCornerRadius(size.width() > 100 ? 8 : 4);
  poster->setSpinnerSize(size.width() > 100 ? 32 : 16);
  poster->setCursor(Qt::PointingHandCursor);
  connect(poster, &PosterWidget::clicked, this, [this, animeId](Qt::MouseButton button) {
    if (button == Qt::LeftButton) openDetails(this, animeId);
    if (button == Qt::RightButton) showMenu(this, animeId);
  });

  m_posters[animeId].append(poster);
  updatePoster(animeId);

  return poster;
}

void HomeWidget::updatePoster(int animeId) {
  const auto it = m_posters.find(animeId);
  if (it == m_posters.end()) return;

  const auto item = anime::db.item(animeId);
  const auto pixmap = imageProvider.loadPoster(animeId);

  for (const auto poster : *it) {
    poster->setPixmap(pixmap);
    poster->setLoading(pixmap.isNull() && item && !item->image_url.empty());
  }
}

void HomeWidget::updateCountdowns() {
  const auto now = std::time(nullptr);
  for (const auto& [label, time] : m_countdowns) {
    label->setText(time > now ? tr("in %1").arg(formatCountdown(time - now)) : tr("Aired"));
  }
}

}  // namespace gui
