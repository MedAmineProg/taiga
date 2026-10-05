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
#include "stats_widget.hpp"

#include <QDateTime>
#include <QGridLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <chrono>
#include <cmath>
#include <functional>

#include "base/string.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/rating.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "media/stats.hpp"

namespace gui {

namespace {

constexpr int kPageMargin = 24;
constexpr int kBarThickness = 16;  // never fill the band
constexpr int kBarRowHeight = 28;
constexpr int kColumnWidth = 24;
constexpr int kRadius = 4;

// One blue hue (validated ordinal ramps for both color schemes).
QColor barColor() {
  return theme.isDark() ? QColor(0x55, 0x98, 0xe7) : QColor(0x2a, 0x78, 0xd6);
}

QColor heatColor(int level) {
  static const std::array<QColor, 4> light{QColor(0x86b6ef), QColor(0x5598e7), QColor(0x256abf),
                                           QColor(0x104281)};
  static const std::array<QColor, 4> dark{QColor(0x184f95), QColor(0x2a78d6), QColor(0x6da7ec),
                                          QColor(0xb7d3f6)};
  return (theme.isDark() ? dark : light)[std::clamp(level, 1, 4) - 1];
}

QColor recessive(const QPalette& palette, qreal alpha) {
  auto color = palette.color(QPalette::WindowText);
  color.setAlphaF(alpha);
  return color;
}

// A bar with a rounded data end and a square baseline.
QPainterPath barPath(const QRectF& rect, Qt::Edge dataEnd) {
  const qreal r = std::min<qreal>(kRadius, std::min(rect.width(), rect.height()) / 2);
  QPainterPath path;
  path.addRoundedRect(rect, r, r);
  QRectF square = rect;
  switch (dataEnd) {
    case Qt::RightEdge:
      square.setRight(rect.left() + rect.width() / 2);
      break;
    case Qt::TopEdge:
      square.setTop(rect.top() + rect.height() / 2);
      break;
    default:
      break;
  }
  QPainterPath base;
  base.addRect(square);
  return path.united(base);
}

QString formatTimeWatched(int minutes) {
  if (minutes < 60) return StatsWidget::tr("%1 min").arg(minutes);
  const double hours = minutes / 60.0;
  if (hours < 48) return StatsWidget::tr("%1 hours").arg(std::lround(hours));
  return StatsWidget::tr("%1 days").arg(QLocale().toString(hours / 24.0, 'f', 1));
}

QString formatDays(int days) {
  return days == 1 ? StatsWidget::tr("1 day") : StatsWidget::tr("%1 days").arg(days);
}

////////////////////////////////////////////////////////////////////////////////

class StatTile final : public QFrame {
public:
  StatTile(QWidget* parent, const QString& label, const QString& value, const QString& note = {})
      : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(2);

    auto labelWidget = new QLabel(label, this);
    labelWidget->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(labelWidget);

    auto valueWidget = new QLabel(value, this);
    auto font = valueWidget->font();
    font.setPointSizeF(font.pointSizeF() * 1.8);
    font.setWeight(QFont::DemiBold);
    valueWidget->setFont(font);
    layout->addWidget(valueWidget);

    if (!note.isEmpty()) {
      auto noteWidget = new QLabel(note, this);
      noteWidget->setForegroundRole(QPalette::PlaceholderText);
      layout->addWidget(noteWidget);
    }
    layout->addStretch();
  }
};

////////////////////////////////////////////////////////////////////////////////

// A year of daily activity, one column per week (Monday first).
class ActivityHeatmap final : public QWidget {
public:
  ActivityHeatmap(QWidget* parent, const QMap<QDate, int>& activity, QDate today)
      : QWidget(parent), m_activity(activity), m_today(today) {
    for (const auto count : activity) m_max = std::max(m_max, count);
    setMinimumSize(kLabelWidth + kMinWeeks * kStep, kHeaderHeight + 7 * kStep + kLegendHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    updateFirstDay();
  }

  QSize sizeHint() const override {
    return {kLabelWidth + kMaxWeeks * kStep, minimumHeight()};
  }

protected:
  void resizeEvent(QResizeEvent* event) override {
    QWidget::resizeEvent(event);
    updateFirstDay();
  }

  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto textColor = palette().color(QPalette::PlaceholderText);
    painter.setPen(textColor);

    // Weekday labels
    const QLocale locale;
    for (const int row : {0, 2, 4}) {
      const QRect rect(0, kHeaderHeight + row * kStep, kLabelWidth - 6, kCell);
      painter.drawText(rect, Qt::AlignRight | Qt::AlignVCenter,
                       locale.dayName(row + 1, QLocale::ShortFormat));
    }

    // Cells and month labels
    int lastMonth = -1;
    for (auto date = m_firstDay; date <= m_today; date = date.addDays(1)) {
      const auto rect = cellRect(date);
      if (date.dayOfWeek() == 1 && date.month() != lastMonth && date.day() <= 7) {
        lastMonth = date.month();
        painter.setPen(textColor);
        painter.drawText(QRect(rect.left(), 0, kStep * 4, kHeaderHeight - 4),
                         Qt::AlignLeft | Qt::AlignBottom,
                         locale.monthName(date.month(), QLocale::ShortFormat));
      }
      painter.setPen(Qt::NoPen);
      painter.setBrush(colorFor(m_activity.value(date)));
      painter.drawRoundedRect(rect, 2, 2);
    }

    // Legend
    const int y = kHeaderHeight + 7 * kStep + 8;
    int x = kLabelWidth;
    painter.setPen(textColor);
    const auto less = StatsWidget::tr("Less");
    painter.drawText(QRect(x, y, 60, kCell), Qt::AlignLeft | Qt::AlignVCenter, less);
    x += painter.fontMetrics().horizontalAdvance(less) + 6;
    for (int level = 0; level <= 4; ++level) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(level == 0 ? recessive(palette(), 0.08) : heatColor(level));
      painter.drawRoundedRect(QRect(x, y, kCell, kCell), 2, 2);
      x += kStep;
    }
    painter.setPen(textColor);
    painter.drawText(QRect(x + 3, y, 60, kCell), Qt::AlignLeft | Qt::AlignVCenter,
                     StatsWidget::tr("More"));
  }

  bool event(QEvent* event) override {
    if (event->type() == QEvent::ToolTip) {
      const auto pos = static_cast<QHelpEvent*>(event)->pos();
      if (const auto date = dateAt(pos); date.isValid()) {
        const int count = m_activity.value(date);
        const auto text = (count == 1 ? StatsWidget::tr("1 episode")
                                      : StatsWidget::tr("%1 episodes").arg(count)) +
                          u" · "_s + QLocale().toString(date, QLocale::LongFormat);
        QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), text, this,
                           cellRect(date).adjusted(-1, -1, 1, 1));
      } else {
        QToolTip::hideText();
      }
      return true;
    }
    return QWidget::event(event);
  }

private:
  static constexpr int kCell = 11;
  static constexpr int kStep = 14;  // cell + 3px gap
  static constexpr int kLabelWidth = 36;
  static constexpr int kHeaderHeight = 18;
  static constexpr int kLegendHeight = 26;
  static constexpr int kMinWeeks = 12;
  static constexpr int kMaxWeeks = anime::stats::kActivityDays / 7;

  // Align the first column to a Monday, going back as many weeks as fit.
  void updateFirstDay() {
    const int weeks = std::clamp((width() - kLabelWidth) / kStep, kMinWeeks, kMaxWeeks);
    const auto monday = m_today.addDays(-(m_today.dayOfWeek() - 1));
    m_firstDay = monday.addDays(-7 * (weeks - 1));
  }

  QRect cellRect(QDate date) const {
    const int week = static_cast<int>(m_firstDay.daysTo(date) / 7);
    const int row = date.dayOfWeek() - 1;
    return {kLabelWidth + week * kStep, kHeaderHeight + row * kStep, kCell, kCell};
  }

  QDate dateAt(QPoint pos) const {
    if (pos.x() < kLabelWidth || pos.y() < kHeaderHeight) return {};
    const int week = (pos.x() - kLabelWidth) / kStep;
    const int row = (pos.y() - kHeaderHeight) / kStep;
    if (row > 6) return {};
    const auto date = m_firstDay.addDays(week * 7 + row);
    return date <= m_today ? date : QDate{};
  }

  QColor colorFor(int count) const {
    if (count <= 0 || m_max <= 0) return recessive(palette(), 0.08);
    return heatColor(static_cast<int>(std::ceil(4.0 * count / m_max)));
  }

  QMap<QDate, int> m_activity;
  QDate m_today;
  QDate m_firstDay;
  int m_max = 0;
};

////////////////////////////////////////////////////////////////////////////////

// Horizontal bars with labels on the left and values at the tips.
class BarChart final : public QWidget {
public:
  struct Bar {
    QString label;
    int value = 0;
    QString tooltip;
  };

  BarChart(QWidget* parent, QList<Bar> bars) : QWidget(parent), m_bars(std::move(bars)) {
    for (const auto& bar : m_bars) m_max = std::max(m_max, bar.value);
    setMinimumHeight(static_cast<int>(m_bars.size()) * kBarRowHeight);
    setMinimumWidth(240);
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto metrics = painter.fontMetrics();

    int labelWidth = 0;
    for (const auto& bar : m_bars) {
      labelWidth = std::max(labelWidth, metrics.horizontalAdvance(bar.label));
    }
    labelWidth = std::min(labelWidth + 16, width() / 2);
    const int valueWidth = metrics.horizontalAdvance(QLocale().toString(m_max)) + 12;
    const int trackWidth = std::max(0, width() - labelWidth - valueWidth);

    for (qsizetype i = 0; i < m_bars.size(); ++i) {
      const auto& bar = m_bars[i];
      const int top = static_cast<int>(i) * kBarRowHeight;

      painter.setPen(palette().color(QPalette::WindowText));
      painter.drawText(QRect(0, top, labelWidth - 8, kBarRowHeight),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       metrics.elidedText(bar.label, Qt::ElideRight, labelWidth - 8));

      const qreal length = m_max > 0 ? std::max(2.0, trackWidth * double(bar.value) / m_max) : 0;
      const QRectF rect(labelWidth, top + (kBarRowHeight - kBarThickness) / 2.0, length,
                        kBarThickness);
      painter.setPen(Qt::NoPen);
      painter.setBrush(barColor());
      if (bar.value > 0) painter.drawPath(barPath(rect, Qt::RightEdge));

      painter.setPen(palette().color(QPalette::PlaceholderText));
      painter.drawText(QRectF(rect.right() + 6, top, valueWidth, kBarRowHeight),
                       Qt::AlignLeft | Qt::AlignVCenter, QLocale().toString(bar.value));
    }
  }

  bool event(QEvent* event) override {
    if (event->type() == QEvent::ToolTip) {
      const auto help = static_cast<QHelpEvent*>(event);
      const int row = help->pos().y() / kBarRowHeight;
      if (row >= 0 && row < m_bars.size() && !m_bars[row].tooltip.isEmpty()) {
        QToolTip::showText(help->globalPos(), m_bars[row].tooltip, this,
                           QRect(0, row * kBarRowHeight, width(), kBarRowHeight));
      } else {
        QToolTip::hideText();
      }
      return true;
    }
    return QWidget::event(event);
  }

private:
  QList<Bar> m_bars;
  int m_max = 0;
};

////////////////////////////////////////////////////////////////////////////////

// Vertical columns with labels below and values on the caps.
class ColumnChart final : public QWidget {
public:
  ColumnChart(QWidget* parent, QList<BarChart::Bar> columns)
      : QWidget(parent), m_columns(std::move(columns)) {
    for (const auto& column : m_columns) m_max = std::max(m_max, column.value);
    setMinimumHeight(180);
    setMinimumWidth(static_cast<int>(m_columns.size()) * (kColumnWidth + 8));
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int labelHeight = fontMetrics().height() + 6;
    const int valueHeight = fontMetrics().height() + 4;
    const int baseline = height() - labelHeight;
    const qreal slot = double(width()) / std::max<qsizetype>(m_columns.size(), 1);

    // Baseline
    painter.setPen(QPen(recessive(palette(), 0.2), 1));
    painter.drawLine(QPointF(0, baseline + 0.5), QPointF(width(), baseline + 0.5));

    for (qsizetype i = 0; i < m_columns.size(); ++i) {
      const auto& column = m_columns[i];
      const qreal center = slot * (i + 0.5);
      const qreal barWidth = std::min<qreal>(kColumnWidth, slot - 6);
      const qreal height = m_max > 0 ? (baseline - valueHeight) * double(column.value) / m_max : 0;

      if (column.value > 0) {
        const QRectF rect(center - barWidth / 2, baseline - height, barWidth, height);
        painter.setPen(Qt::NoPen);
        painter.setBrush(barColor());
        painter.drawPath(barPath(rect, Qt::TopEdge));

        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(QRectF(center - slot / 2, rect.top() - valueHeight, slot, valueHeight),
                         Qt::AlignHCenter | Qt::AlignBottom, QLocale().toString(column.value));
      }

      painter.setPen(palette().color(QPalette::WindowText));
      painter.drawText(QRectF(center - slot / 2, baseline + 4, slot, labelHeight),
                       Qt::AlignHCenter | Qt::AlignTop, column.label);
    }
  }

  bool event(QEvent* event) override {
    if (event->type() == QEvent::ToolTip && !m_columns.isEmpty()) {
      const auto help = static_cast<QHelpEvent*>(event);
      const qreal slot = double(width()) / m_columns.size();
      const auto index = static_cast<qsizetype>(help->pos().x() / slot);
      if (index >= 0 && index < m_columns.size()) {
        const QRect rect(static_cast<int>(slot * index), 0, static_cast<int>(slot), height());
        QToolTip::showText(help->globalPos(), m_columns[index].tooltip, this, rect);
      }
      return true;
    }
    return QWidget::event(event);
  }

private:
  QList<BarChart::Bar> m_columns;
  int m_max = 0;
};

////////////////////////////////////////////////////////////////////////////////

QLabel* sectionTitle(QWidget* parent, const QString& text) {
  auto label = new QLabel(text, parent);
  auto font = label->font();
  font.setPointSizeF(font.pointSizeF() * 1.25);
  font.setWeight(QFont::DemiBold);
  label->setFont(font);
  return label;
}

QLabel* mutedLabel(QWidget* parent, const QString& text) {
  auto label = new QLabel(text, parent);
  label->setForegroundRole(QPalette::PlaceholderText);
  label->setWordWrap(true);
  return label;
}

QWidget* section(QWidget* parent, const QString& title, const QString& subtitle, QWidget* body) {
  auto widget = new QWidget(parent);
  auto layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  layout->addWidget(sectionTitle(widget, title));
  if (!subtitle.isEmpty()) layout->addWidget(mutedLabel(widget, subtitle));
  layout->addSpacing(6);
  body->setParent(widget);
  layout->addWidget(body);
  layout->addStretch();
  return widget;
}

QList<BarChart::Bar> toBars(const std::vector<std::pair<std::string, int>>& items) {
  QList<BarChart::Bar> bars;
  for (const auto& [name, count] : items) {
    const auto label = QString::fromStdString(name);
    bars.append({label, count,
                 count == 1 ? StatsWidget::tr("%1: 1 anime").arg(label)
                            : StatsWidget::tr("%1: %2 anime").arg(label).arg(count)});
  }
  return bars;
}

}  // namespace

StatsWidget::StatsWidget(QWidget* parent) : PageWidget(parent) {
  auto scrollArea = new QScrollArea(this);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout()->addWidget(scrollArea);

  m_content = new QWidget(scrollArea);
  m_contentLayout = new QVBoxLayout(m_content);
  m_contentLayout->setContentsMargins(kPageMargin, 0, kPageMargin, kPageMargin);
  m_contentLayout->setSpacing(24);
  scrollArea->setWidget(m_content);

  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(std::chrono::milliseconds{250});
  connect(m_refreshTimer, &QTimer::timeout, this, &StatsWidget::refresh);

  connect(&anime::db, &anime::Database::entryUpdated, this, &StatsWidget::scheduleRefresh);
  connect(&anime::db, &anime::Database::entryDeleted, this, &StatsWidget::scheduleRefresh);
  connect(&anime::db, &anime::Database::itemUpdated, this, &StatsWidget::scheduleRefresh);
  connect(&anime::history, &anime::History::changed, this, &StatsWidget::scheduleRefresh);
  connect(&theme, &Theme::colorSchemeChanged, this, &StatsWidget::scheduleRefresh);
}

void StatsWidget::showEvent(QShowEvent* event) {
  PageWidget::showEvent(event);
  refresh();
}

void StatsWidget::scheduleRefresh() {
  if (isVisible()) m_refreshTimer->start();
}

void StatsWidget::refresh() {
  while (const auto item = m_contentLayout->takeAt(0)) {
    if (const auto widget = item->widget()) widget->deleteLater();
    delete item;
  }

  QList<std::time_t> watchTimes;
  for (const auto& item : anime::history.items()) watchTimes.append(item.time);

  const auto today = QDate::currentDate();
  const auto stats =
      anime::stats::compute(anime::db.items(), anime::db.entries(), watchTimes, today);

  // Header
  {
    auto header = new QWidget(m_content);
    auto layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto title = new QLabel(tr("Statistics"), header);
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.8);
    font.setWeight(QFont::DemiBold);
    title->setFont(font);
    layout->addWidget(title);
    layout->addWidget(mutedLabel(header, tr("Based on your anime list and watch history.")));
    m_contentLayout->addWidget(header);
  }

  if (stats.anime_count == 0) {
    m_contentLayout->addWidget(
        mutedLabel(m_content, tr("Add anime to your list to see statistics here.")));
    m_contentLayout->addStretch();
    return;
  }

  // Tiles
  {
    auto tiles = new QWidget(m_content);
    auto layout = new QGridLayout(tiles);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    const auto meanScore = stats.scored_count > 0
                               ? formatRating(static_cast<int>(std::lround(stats.mean_score)))
                               : u"-"_s;

    const QList<QWidget*> widgets{
        new StatTile(
            tiles, tr("Anime"), QLocale().toString(stats.anime_count),
            tr("%1 completed").arg(stats.status_counts.value(anime::list::Status::Completed))),
        new StatTile(tiles, tr("Episodes"), QLocale().toString(stats.episodes_watched),
                     tr("including rewatches")),
        new StatTile(tiles, tr("Time watched"), formatTimeWatched(stats.minutes_watched),
                     tr("estimated")),
        new StatTile(tiles, tr("Mean score"), meanScore,
                     tr("of %1 scored").arg(stats.scored_count)),
        new StatTile(tiles, tr("Current streak"), formatDays(stats.current_streak),
                     tr("longest: %1").arg(formatDays(stats.longest_streak))),
    };
    for (int i = 0; i < widgets.size(); ++i) {
      layout->addWidget(widgets[i], 0, i);
      layout->setColumnStretch(i, 1);
    }
    m_contentLayout->addWidget(tiles);
  }

  // Activity
  {
    int episodes = 0;
    for (const auto count : stats.activity) episodes += count;
    const auto subtitle = episodes > 0 ? tr("%1 episodes on %2 in the last year")
                                             .arg(QLocale().toString(episodes))
                                             .arg(formatDays(stats.active_days))
                                       : tr("Episodes you watch with Taiga will show up here.");
    m_contentLayout->addWidget(section(m_content, tr("Activity"), subtitle,
                                       new ActivityHeatmap(nullptr, stats.activity, today)));
  }

  // Charts, two per row
  {
    auto grid = new QWidget(m_content);
    auto layout = new QGridLayout(grid);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(48);
    layout->setVerticalSpacing(24);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    // Scores
    QList<BarChart::Bar> scores;
    for (int i = 0; i < 10; ++i) {
      const auto label = formatRating((i + 1) * 10);
      const int count = stats.score_buckets[i];
      scores.append({label, count, tr("Scored %1: %2 anime").arg(label).arg(count)});
    }
    layout->addWidget(
        section(grid, tr("Scores"),
                stats.scored_count > 0 ? tr("How you rate what you watch") : tr("No scores yet."),
                new ColumnChart(nullptr, scores)),
        0, 0);

    // List status
    QList<BarChart::Bar> statuses;
    for (const auto status : anime::list::kStatuses) {
      const int count = stats.status_counts.value(status);
      statuses.append({formatListStatus(status), count,
                       tr("%1: %2 anime").arg(formatListStatus(status)).arg(count)});
    }
    layout->addWidget(section(grid, tr("Anime list"), {}, new BarChart(nullptr, statuses)), 0, 1);

    // Genres and studios
    const auto genres = toBars(stats.top_genres);
    layout->addWidget(section(grid, tr("Top genres"),
                              genres.isEmpty() ? tr("No genre information yet.") : QString{},
                              new BarChart(nullptr, genres)),
                      1, 0);
    const auto studios = toBars(stats.top_studios);
    layout->addWidget(section(grid, tr("Top studios"),
                              studios.isEmpty() ? tr("No studio information yet.") : QString{},
                              new BarChart(nullptr, studios)),
                      1, 1);

    m_contentLayout->addWidget(grid);
  }

  m_contentLayout->addStretch();
}

}  // namespace gui
