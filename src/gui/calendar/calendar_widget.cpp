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
#include "calendar_widget.hpp"

#include <QAction>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QTimer>
#include <chrono>

#include "base/string.hpp"
#include "gui/common/clickable_label.hpp"
#include "gui/common/poster_widget.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/media/media_menu.hpp"
#include "gui/utils/image_provider.hpp"
#include "gui/utils/theme.hpp"
#include "media/airing_schedule.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"
#include "taiga/settings.hpp"

namespace gui {

namespace {

constexpr int kPageMargin = 24;
constexpr QSize kPosterSize{28, 40};

std::time_t toTime(QDate date) {
  return QDateTime(date, QTime(0, 0)).toSecsSinceEpoch();
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
  (new MediaMenu(parent, {*item}, entries, nullptr, AnimeListContext::List))->popup();
}

// One episode in a day column.
QWidget* createEpisodeCard(QWidget* parent, const anime::schedule::Episode& episode,
                           const std::time_t now) {
  const auto item = anime::db.item(episode.anime_id);
  const auto entry = anime::db.entry(episode.anime_id);
  const int id = episode.anime_id;
  const bool aired = episode.time <= now;

  auto card = new QFrame(parent);
  card->setFrameShape(QFrame::StyledPanel);
  card->setCursor(Qt::PointingHandCursor);
  auto layout = new QHBoxLayout(card);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(8);

  auto poster = new PosterWidget(card);
  poster->setFixedSize(kPosterSize);
  poster->setCornerRadius(3);
  poster->setSpinnerSize(12);
  const auto pixmap = imageProvider.loadPoster(id);
  poster->setPixmap(pixmap);
  poster->setLoading(pixmap.isNull() && item && !item->image_url.empty());
  QObject::connect(&imageProvider, &ImageProvider::posterChanged, poster,
                   [poster, id](int changed) {
                     if (changed != id) return;
                     const auto pixmap = imageProvider.loadPoster(id);
                     poster->setPixmap(pixmap);
                     poster->setLoading(pixmap.isNull());
                   });
  QObject::connect(poster, &PosterWidget::clicked, card, [card, id](Qt::MouseButton button) {
    if (button == Qt::LeftButton) openDetails(card, id);
    if (button == Qt::RightButton) showMenu(card, id);
  });
  layout->addWidget(poster, 0, Qt::AlignTop);

  auto text = new QVBoxLayout();
  text->setSpacing(1);

  const auto airTime = QDateTime::fromSecsSinceEpoch(episode.time);
  auto time = new QLabel(QLocale().toString(airTime.time(), QLocale::ShortFormat), card);
  auto font = time->font();
  font.setWeight(QFont::DemiBold);
  time->setFont(font);
  if (aired) time->setForegroundRole(QPalette::PlaceholderText);
  time->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  text->addWidget(time);

  auto title = new ClickableLabel(card);
  title->setText(item ? QString::fromStdString(anime::preferredTitle(*item)) : QString{});
  title->setElidable(true);
  title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QObject::connect(title, &ClickableLabel::clicked, card, [card, id](Qt::MouseButton button) {
    if (button == Qt::LeftButton) openDetails(card, id);
    if (button == Qt::RightButton) showMenu(card, id);
  });
  text->addWidget(title);

  // Watched episodes are marked, so that it's clear what's left to catch up on.
  const bool watched = entry && entry->watched_episodes >= episode.number;
  auto details = new QLabel(card);
  details->setForegroundRole(QPalette::PlaceholderText);
  details->setText(watched ? CalendarWidget::tr("Ep %1 ✓").arg(episode.number)
                           : CalendarWidget::tr("Ep %1").arg(episode.number));
  details->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  text->addWidget(details);
  layout->addLayout(text, 1);

  QString tooltip = title->text() + u"\n"_s + CalendarWidget::tr("Episode %1").arg(episode.number) +
                    u" · "_s + QLocale().toString(airTime, QLocale::LongFormat);
  if (episode.estimated) {
    tooltip += u"\n"_s + CalendarWidget::tr("Estimated, assuming a weekly schedule");
  }
  if (watched) tooltip += u"\n"_s + CalendarWidget::tr("Watched");
  card->setToolTip(tooltip);

  return card;
}

}  // namespace

CalendarWidget::CalendarWidget(QWidget* parent) : PageWidget(parent) {
  // Toolbar
  {
    auto previous = new QAction(theme.getIcon("arrow_back"), tr("Previous week"), this);
    connect(previous, &QAction::triggered, this, &CalendarWidget::showPreviousWeek);
    m_toolbar->addAction(previous);

    m_actionThisWeek = new QAction(tr("This week"), this);
    connect(m_actionThisWeek, &QAction::triggered, this, &CalendarWidget::showThisWeek);
    m_toolbar->addAction(m_actionThisWeek);

    auto next = new QAction(theme.getIcon("arrow_forward"), tr("Next week"), this);
    connect(next, &QAction::triggered, this, &CalendarWidget::showNextWeek);
    m_toolbar->addAction(next);

    m_toolbar->addSeparator();

    auto watchingOnly = new QAction(tr("Watching only"), this);
    watchingOnly->setCheckable(true);
    watchingOnly->setChecked(taiga::settings.calendarWatchingOnly());
    watchingOnly->setToolTip(tr("Hide anime you're planning to watch"));
    connect(watchingOnly, &QAction::toggled, this, [this](bool checked) {
      taiga::settings.setCalendarWatchingOnly(checked);
      refresh();
    });
    m_toolbar->addAction(watchingOnly);

    auto notify = new QAction(tr("Notify me"), this);
    notify->setCheckable(true);
    notify->setChecked(taiga::settings.calendarNotificationsEnabled());
    notify->setToolTip(tr("Show a notification when an episode of an anime you're watching airs"));
    connect(notify, &QAction::toggled, this,
            [](bool checked) { taiga::settings.setCalendarNotificationsEnabled(checked); });
    m_toolbar->addAction(notify);
  }

  auto scrollArea = new QScrollArea(this);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout()->addWidget(scrollArea);

  m_content = new QWidget(scrollArea);
  m_contentLayout = new QVBoxLayout(m_content);
  m_contentLayout->setContentsMargins(kPageMargin, 0, kPageMargin, kPageMargin);
  m_contentLayout->setSpacing(16);
  scrollArea->setWidget(m_content);

  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(std::chrono::milliseconds{250});
  connect(m_refreshTimer, &QTimer::timeout, this, &CalendarWidget::refresh);

  // Keep "aired" states and today's highlight current.
  m_minuteTimer = new QTimer(this);
  m_minuteTimer->setInterval(std::chrono::minutes{1});
  connect(m_minuteTimer, &QTimer::timeout, this, &CalendarWidget::scheduleRefresh);
  m_minuteTimer->start();

  connect(&anime::db, &anime::Database::entryUpdated, this, &CalendarWidget::scheduleRefresh);
  connect(&anime::db, &anime::Database::entryDeleted, this, &CalendarWidget::scheduleRefresh);
  connect(&anime::db, &anime::Database::itemUpdated, this, &CalendarWidget::scheduleRefresh);
}

void CalendarWidget::showEvent(QShowEvent* event) {
  PageWidget::showEvent(event);
  refresh();
}

void CalendarWidget::showPreviousWeek() {
  --m_weekOffset;
  refresh();
}

void CalendarWidget::showNextWeek() {
  ++m_weekOffset;
  refresh();
}

void CalendarWidget::showThisWeek() {
  m_weekOffset = 0;
  refresh();
}

QDate CalendarWidget::weekStart() const {
  const auto today = QDate::currentDate();
  const int firstDay = static_cast<int>(QLocale().firstDayOfWeek());
  const int daysSinceStart = (today.dayOfWeek() - firstDay + 7) % 7;
  return today.addDays(-daysSinceStart + 7 * m_weekOffset);
}

void CalendarWidget::scheduleRefresh() {
  if (isVisible()) m_refreshTimer->start();
}

void CalendarWidget::refresh() {
  while (const auto item = m_contentLayout->takeAt(0)) {
    if (const auto widget = item->widget()) widget->deleteLater();
    delete item;
  }

  m_actionThisWeek->setEnabled(m_weekOffset != 0);

  const auto start = weekStart();
  const auto today = QDate::currentDate();
  const auto now = std::time(nullptr);

  QSet<anime::list::Status> statuses{anime::list::Status::Watching};
  if (!taiga::settings.calendarWatchingOnly()) statuses.insert(anime::list::Status::PlanToWatch);

  const auto episodes = anime::schedule::episodesBetween(
      anime::db.items(), anime::db.entries(), statuses, toTime(start), toTime(start.addDays(7)));

  // Header
  {
    auto header = new QWidget(m_content);
    auto layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto title = new QLabel(tr("Airing calendar"), header);
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.8);
    font.setWeight(QFont::DemiBold);
    title->setFont(font);
    layout->addWidget(title);

    const QLocale locale;
    const auto end = start.addDays(6);
    const auto range =
        u"%1 – %2"_s.arg(locale.toString(start, u"MMM d"_s))
            .arg(locale.toString(end, start.year() == end.year() && start.month() == end.month()
                                          ? u"d, yyyy"_s
                                          : u"MMM d, yyyy"_s));
    const auto count = static_cast<int>(episodes.size());
    const auto summary = count == 0   ? tr("No episodes")
                         : count == 1 ? tr("1 episode")
                                      : tr("%1 episodes").arg(count);
    auto subtitle = new QLabel(u"%1 · %2"_s.arg(range).arg(summary), header);
    subtitle->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(subtitle);
    m_contentLayout->addWidget(header);
  }

  // Day columns
  auto grid = new QWidget(m_content);
  auto gridLayout = new QGridLayout(grid);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setHorizontalSpacing(8);
  gridLayout->setVerticalSpacing(6);

  for (int day = 0; day < 7; ++day) {
    const auto date = start.addDays(day);
    gridLayout->setColumnStretch(day, 1);

    // Day header, with today highlighted
    auto header = new QLabel(grid);
    header->setText(u"<b>%1</b> %2"_s.arg(QLocale().dayName(date.dayOfWeek(), QLocale::ShortFormat))
                        .arg(date.day()));
    header->setAlignment(Qt::AlignCenter);
    header->setContentsMargins(0, 4, 0, 4);
    if (date == today) {
      header->setAutoFillBackground(true);
      auto palette = header->palette();
      palette.setColor(QPalette::Window, palette.color(QPalette::Highlight));
      palette.setColor(QPalette::WindowText, palette.color(QPalette::HighlightedText));
      header->setPalette(palette);
    }
    gridLayout->addWidget(header, 0, day);

    auto column = new QVBoxLayout();
    column->setSpacing(6);
    const auto dayStart = toTime(date);
    const auto dayEnd = toTime(date.addDays(1));
    int count = 0;
    for (const auto& episode : episodes) {
      if (episode.time < dayStart || episode.time >= dayEnd) continue;
      column->addWidget(createEpisodeCard(grid, episode, now));
      ++count;
    }
    if (count == 0) {
      auto empty = new QLabel(u"–"_s, grid);
      empty->setAlignment(Qt::AlignCenter);
      empty->setForegroundRole(QPalette::PlaceholderText);
      column->addWidget(empty);
    }
    column->addStretch();
    gridLayout->addLayout(column, 1, day);
  }
  gridLayout->setRowStretch(1, 1);
  m_contentLayout->addWidget(grid, 1);

  if (episodes.empty() && m_weekOffset == 0) {
    auto hint = new QLabel(tr("Anime you're watching or planning to watch will show up here when "
                              "their next episodes have an air date."),
                           m_content);
    hint->setForegroundRole(QPalette::PlaceholderText);
    hint->setWordWrap(true);
    m_contentLayout->addWidget(hint);
  }
}

}  // namespace gui
