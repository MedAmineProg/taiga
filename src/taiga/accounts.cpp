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

#include "accounts.hpp"

#include "base/string.hpp"
#include "sync/anilist/anilist_ratings.hpp"
#include "sync/kitsu/kitsu_ratings.hpp"
#include "taiga/path.hpp"

namespace taiga {

Accounts::Accounts() : QObject{} {}

QStringList Accounts::secretKeys() {
  return {
      u"anilist.token"_s,  u"kitsu.accessToken"_s,       u"kitsu.refreshToken"_s,
      u"kitsu.password"_s, u"myanimelist.accessToken"_s, u"myanimelist.refreshToken"_s,
  };
}

void Accounts::initSecrets() const {
  migrateSecrets(secretKeys());
}

QString Accounts::fileName() const {
  return u"%1/accounts.json"_s.arg(get_data_path());
}

////////////////////////////////////////////////////////////////////////////////

bool Accounts::anilistAuthenticated() const {
  return value("anilist.authenticated").toBool();
}

sync::anilist::RatingSystem Accounts::anilistRatingSystem() const {
  const auto ratingSystem = value("anilist.ratingSystem").toString();
  return sync::anilist::parseRatingSystem(ratingSystem);
}

std::string Accounts::anilistUsername() const {
  return value("anilist.username").toString().toStdString();
}

std::string Accounts::anilistToken() const {
  return secretValue(u"anilist.token"_s);
}

bool Accounts::kitsuAuthenticated() const {
  return value("kitsu.authenticated").toBool();
}

std::string Accounts::kitsuAccessToken() const {
  return secretValue(u"kitsu.accessToken"_s);
}

std::string Accounts::kitsuDisplayName() const {
  return value("kitsu.displayName").toString().toStdString();
}

std::string Accounts::kitsuEmail() const {
  return value("kitsu.email").toString().toStdString();
}

sync::kitsu::RatingSystem Accounts::kitsuRatingSystem() const {
  const auto ratingSystem = value("kitsu.ratingSystem").toString();
  return sync::kitsu::parseRatingSystem(ratingSystem);
}

std::string Accounts::kitsuRefreshToken() const {
  return secretValue(u"kitsu.refreshToken"_s);
}

std::string Accounts::kitsuUserId() const {
  return value("kitsu.userId").toString().toStdString();
}

std::string Accounts::kitsuUsername() const {
  return value("kitsu.username").toString().toStdString();
}

std::string Accounts::kitsuPassword() const {
  return secretValue(u"kitsu.password"_s);
}

bool Accounts::myanimelistAuthenticated() const {
  return value("myanimelist.authenticated").toBool();
}

std::string Accounts::myanimelistUsername() const {
  return value("myanimelist.username").toString().toStdString();
}

std::string Accounts::myanimelistAccessToken() const {
  return secretValue(u"myanimelist.accessToken"_s);
}

std::string Accounts::myanimelistRefreshToken() const {
  return secretValue(u"myanimelist.refreshToken"_s);
}

////////////////////////////////////////////////////////////////////////////////

void Accounts::setAnilistAuthenticated(bool authenticated) {
  setValue("anilist.authenticated", authenticated);
  emit authenticationChanged(authenticated);
}

void Accounts::setAnilistRatingSystem(const std::string& ratingSystem) const {
  setValue("anilist.ratingSystem", ratingSystem);
}

void Accounts::setAnilistUsername(const std::string& username) const {
  setValue("anilist.username", username);
}

void Accounts::setAnilistToken(const std::string& token) const {
  setSecretValue(u"anilist.token"_s, token);
}

void Accounts::setKitsuAuthenticated(bool authenticated) {
  setValue("kitsu.authenticated", authenticated);
  emit authenticationChanged(authenticated);
}

void Accounts::setKitsuAccessToken(const std::string& accessToken) const {
  setSecretValue(u"kitsu.accessToken"_s, accessToken);
}

void Accounts::setKitsuDisplayName(const std::string& displayName) const {
  setValue("kitsu.displayName", displayName);
}

void Accounts::setKitsuEmail(const std::string& email) const {
  setValue("kitsu.email", email);
}

void Accounts::setKitsuRatingSystem(const std::string& ratingSystem) const {
  setValue("kitsu.ratingSystem", ratingSystem);
}

void Accounts::setKitsuRefreshToken(const std::string& refreshToken) const {
  setSecretValue(u"kitsu.refreshToken"_s, refreshToken);
}

void Accounts::setKitsuUserId(const std::string& userId) const {
  setValue("kitsu.userId", userId);
}

void Accounts::setKitsuUsername(const std::string& username) const {
  setValue("kitsu.username", username);
}

void Accounts::setKitsuPassword(const std::string& password) const {
  setSecretValue(u"kitsu.password"_s, password);
}

void Accounts::setMyanimelistAuthenticated(bool authenticated) {
  setValue("myanimelist.authenticated", authenticated);
  emit authenticationChanged(authenticated);
}

void Accounts::setMyanimelistUsername(const std::string& username) const {
  setValue("myanimelist.username", username);
}

void Accounts::setMyanimelistAccessToken(const std::string& accessToken) const {
  setSecretValue(u"myanimelist.accessToken"_s, accessToken);
}

void Accounts::setMyanimelistRefreshToken(const std::string& refreshToken) const {
  setSecretValue(u"myanimelist.refreshToken"_s, refreshToken);
}

////////////////////////////////////////////////////////////////////////////////

std::string Accounts::serviceUsername(const std::string& service) const {
  if (service == "anilist") return anilistUsername();
  if (service == "kitsu") return kitsuUsername();
  if (service == "myanimelist") return myanimelistUsername();
  return {};
}

}  // namespace taiga
