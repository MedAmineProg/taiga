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

#include "rating.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <cmath>
#include <limits>

#include "sync/anilist/anilist_ratings.hpp"
#include "sync/kitsu/kitsu_ratings.hpp"
#include "sync/myanimelist/myanimelist_ratings.hpp"
#include "sync/service.hpp"
#include "taiga/accounts.hpp"

namespace gui {

QList<taiga::sync::Rating> currentRatingList() {
  switch (taiga::sync::currentServiceId()) {
    case taiga::sync::ServiceId::MyAnimeList:
      return taiga::sync::myanimelist::ratingList();
    case taiga::sync::ServiceId::Kitsu:
      return taiga::sync::kitsu::ratingList(taiga::accounts.kitsuRatingSystem());
    case taiga::sync::ServiceId::AniList:
      return taiga::sync::anilist::ratingList(taiga::accounts.anilistRatingSystem());
    case taiga::sync::ServiceId::Unknown:
      break;
  }
  return {};
}

QString formatRating(int value, QString placeholder) {
  if (value <= 0) return placeholder;

  switch (taiga::sync::currentServiceId()) {
    case taiga::sync::ServiceId::MyAnimeList:
      return taiga::sync::myanimelist::formatRating(value);
    case taiga::sync::ServiceId::Kitsu:
      return taiga::sync::kitsu::formatRating(value, taiga::accounts.kitsuRatingSystem());
    case taiga::sync::ServiceId::AniList:
      return taiga::sync::anilist::formatRating(value, taiga::accounts.anilistRatingSystem());
    case taiga::sync::ServiceId::Unknown:
      break;
  }
  return QString::number(value);
}

void populateRatingComboBox(QComboBox* comboBox) {
  comboBox->clear();
  for (const auto& rating : currentRatingList()) {
    comboBox->addItem(rating.text, rating.value);
  }
}

void setRatingComboBoxValue(QComboBox* comboBox, int score) {
  int index = comboBox->findData(score);

  if (index < 0) {
    // Find the nearest match.
    int bestDiff = std::numeric_limits<int>::max();
    for (int i = 0; i < comboBox->count(); ++i) {
      const int diff = std::abs(comboBox->itemData(i).toInt() - score);
      if (diff < bestDiff) {
        bestDiff = diff;
        index = i;
      }
    }
  }

  comboBox->setCurrentIndex(index);
}

bool usesRatingSpinBox() {
  if (taiga::sync::currentServiceId() != taiga::sync::ServiceId::AniList) return false;

  switch (taiga::accounts.anilistRatingSystem()) {
    case taiga::sync::anilist::RatingSystem::Point_100:
    case taiga::sync::anilist::RatingSystem::Point_10_Decimal:
      return true;
    case taiga::sync::anilist::RatingSystem::Point_10:
    case taiga::sync::anilist::RatingSystem::Point_5:
    case taiga::sync::anilist::RatingSystem::Point_3:
      return false;
  }

  return false;
}

void populateRatingSpinBox(QDoubleSpinBox* spinBox) {
  switch (taiga::accounts.anilistRatingSystem()) {
    case taiga::sync::anilist::RatingSystem::Point_10_Decimal:
      spinBox->setDecimals(1);
      spinBox->setRange(0.0, 10.0);
      spinBox->setSingleStep(0.1);
      return;
    case taiga::sync::anilist::RatingSystem::Point_100:
    case taiga::sync::anilist::RatingSystem::Point_10:
    case taiga::sync::anilist::RatingSystem::Point_5:
    case taiga::sync::anilist::RatingSystem::Point_3:
      spinBox->setDecimals(0);
      spinBox->setRange(0, 100);
      spinBox->setSingleStep(1);
      return;
  }
}

void setRatingSpinBoxValue(QDoubleSpinBox* spinBox, int score) {
  spinBox->setValue(taiga::accounts.anilistRatingSystem() ==
                            taiga::sync::anilist::RatingSystem::Point_10_Decimal
                        ? score / 10.0
                        : score);
}

int ratingSpinBoxValue(const QDoubleSpinBox* spinBox) {
  if (taiga::accounts.anilistRatingSystem() ==
      taiga::sync::anilist::RatingSystem::Point_10_Decimal) {
    return static_cast<int>(std::lround(spinBox->value() * 10));
  }
  return static_cast<int>(std::lround(spinBox->value()));
}

}  // namespace gui
