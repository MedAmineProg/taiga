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

#include "anilist.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRestReply>
#include <QSet>
#include <ranges>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "sync/anilist/anilist_error.hpp"
#include "sync/anilist/anilist_parsers.hpp"
#include "sync/anilist/anilist_utils.hpp"
#include "sync/id_parsers.hpp"
#include "sync/queue.hpp"
#include "taiga/accounts.hpp"

// AniList API documentation:
// https://docs.anilist.co/

namespace taiga::sync::anilist {

Service::Service() : taiga::sync::Service{ServiceId::AniList} {
  api_.setBaseUrl(QUrl{"https://graphql.anilist.co"});

  if (const auto token = taiga::accounts.anilistToken(); !token.empty()) {
    api_.setBearerToken(QByteArray::fromStdString(token));
  }
}

Service* Service::instance() {
  static auto service = new Service();
  return service;
}

////////////////////////////////////////////////////////////////////////////////

void Service::fetchAnime(const int id) {
  const QJsonDocument data{{
      {"query", gql("Media")},
      {"variables", QJsonObject{{"id", id}}},
  }};

  const auto callback = [this, id](QRestReply& reply) {
    if (isError(reply)) {
      if (reply.httpStatus() == 404) {
        taiga::sync::invalidateAnime(id);
      } else {
        handleError(*this, reply);
      }
      return;
    }

    const auto item = reply.readJson().and_then(
        [](const QJsonDocument& json) { return parseMedia(json["data"]["Media"]); });

    if (!item) {
      handleError(*this, reply, "Could not parse media object.");
      return;
    }

    anime::db.updateItem(*item);
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::search(const SearchParams& params, const int page) {
  QJsonObject variables{{"page", page}};
  if (!params.text.isEmpty()) variables["query"] = params.text;
  if (params.season) variables["season"] = fromSeasonName(*params.season);
  if (params.year) variables["seasonYear"] = *params.year;
  if (params.type) variables["format"] = fromType(*params.type);
  if (params.status) variables["status"] = fromStatus(*params.status);
  variables["sort"] = QJsonArray{fromSearchParams(params)};

  const QJsonDocument data{{
      {"query", gql("MediaSearch")},
      {"variables", variables},
  }};

  const auto callback = [this, params, page](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      emit searchCompleted(params, {});
      return;
    }

    const auto pageObject = reply.readJson().and_then([](const QJsonDocument& json) {
      return std::make_optional(json["data"]["Page"].toObject());
    });

    const auto items = pageObject.and_then([](const QJsonObject& page) {
      const auto value = page["media"];
      if (!value.isArray()) return std::optional<QList<std::optional<Anime>>>{};
      return std::make_optional(value.toArray() | std::views::transform(parseMedia) |
                                std::ranges::to<QList>());
    });

    if (!items) {
      handleError(*this, reply, "Could not parse search results.");
      emit searchCompleted(params, {});
      return;
    }

    QList<int> ids;
    for (const auto& item : *items) {
      if (!item) continue;
      anime::db.updateItem(*item);
      ids.append(item->id);
    }

    emit searchCompleted(params, ids);

    // Auto-paginate only for season browsing.
    const bool seasonScoped = params.season.has_value() && params.year.has_value();
    if (seasonScoped && (*pageObject)["pageInfo"]["hasNextPage"].toBool()) {
      search(params, page + 1);
    }
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::fetchListEntries() {
  const QJsonObject variables{
      {"userName", QString::fromStdString(taiga::accounts.anilistUsername())},
  };

  const QJsonDocument data{{
      {"query", gql("MediaListCollection")},
      {"variables", variables},
  }};

  const auto callback = [this](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      return;
    }

    const auto lists = reply.readJson().and_then([](const QJsonDocument& json) {
      const auto value = json["data"]["MediaListCollection"]["lists"];
      if (!value.isArray()) return std::optional<QJsonArray>{};
      return std::make_optional(value.toArray());
    });

    if (!lists) {
      handleError(*this, reply, "Could not parse anime list.");
      return;
    }

    QSet<int> fetchedIds;
    QList<Anime> items;
    QList<ListEntry> entries;

    for (const auto& list : *lists) {
      for (const auto& entryValue : list.toObject()["entries"].toArray()) {
        const auto entry = entryValue.toObject();

        if (const auto item = parseMedia(entry["media"])) {
          items.append(*item);
        }
        if (const auto listEntry = parseListEntry(entry)) {
          entries.append(*listEntry);
          fetchedIds.insert(listEntry->anime_id);
        }
      }
    }
    anime::db.updateItems(items);
    anime::db.updateEntries(entries);

    taiga::sync::pruneMissingEntries(fetchedIds);
    emit listEntriesFetched();
  };

  const auto reply = manager_.post(api_.createRequest(), data, this, callback);

  connect(reply, &QNetworkReply::downloadProgress, this, &Service::transferProgress);
}

void Service::addListEntry(const int id, const anime::list::Fields dirty) {
  updateListEntry(id, dirty);
}

void Service::deleteListEntry(const int id) {
  const auto listEntry = anime::db.entry(id);

  if (!listEntry) return;

  deleteEntry(listEntry->id, [](const EntryResult& result) {
    taiga::sync::queue.complete(result.success, result.error);
  });
}

void Service::updateListEntry(const int id, const anime::list::Fields dirty) {
  const auto listEntry = anime::db.entry(id);

  if (!listEntry) return;

  saveEntry(*listEntry, dirty, [](const EntryResult& result) {
    if (result.remote) {
      taiga::sync::queue.complete(*result.remote);
    } else {
      taiga::sync::queue.complete(result.success, result.error);
    }
  });
}

void Service::saveEntry(const ListEntry& listEntry, const anime::list::Fields dirty,
                        EntryCallback done) {
  QJsonObject variables{
      {"mediaId", listEntry.anime_id},
  };

  if (listEntry.id != anime::list::kUnknownId) {
    variables["id"] = static_cast<qint64>(listEntry.id);
  }

  using anime::list::Field;

  if (dirty & (Field::Status | Field::Rewatching)) {
    variables["status"] = listEntry.rewatching ? u"REPEATING"_s : fromListStatus(listEntry.status);
  }
  if (dirty & Field::Score) variables["scoreRaw"] = listEntry.score;
  if (dirty & Field::Episode) variables["progress"] = listEntry.watched_episodes;
  if (dirty & Field::RewatchedTimes) variables["repeat"] = listEntry.rewatched_times;
  if (dirty & Field::Notes) variables["notes"] = QString::fromStdString(listEntry.notes);
  if (dirty & Field::DateStarted) variables["startedAt"] = fromFuzzyDate(listEntry.date_started);
  if (dirty & Field::DateCompleted) {
    variables["completedAt"] = fromFuzzyDate(listEntry.date_completed);
  }

  const QJsonDocument data{{
      {"query", gql("SaveMediaListEntry")},
      {"variables", variables},
  }};

  const auto callback = [this, done](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      done({.error = u"Failed to update list entry."_s});
      return;
    }

    const auto entry = reply.readJson().and_then([](const QJsonDocument& json) {
      return std::make_optional(json["data"]["SaveMediaListEntry"].toObject());
    });

    if (!entry) {
      handleError(*this, reply, "Could not parse list entry.");
      done({.error = u"Could not parse list entry."_s});
      return;
    }

    // Only the main service's anime are in the database.
    if (id() == currentServiceId()) {
      if (const auto item = parseMedia((*entry)["media"])) {
        anime::db.updateItem(*item);
      }
    }
    done({.success = true, .remote = parseListEntry(*entry)});
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::deleteEntry(const int64_t entryId, EntryCallback done) {
  const QJsonDocument data{{
      {"query", gql("DeleteMediaListEntry")},
      {"variables", QJsonObject{{"id", static_cast<qint64>(entryId)}}},
  }};

  const auto callback = [this, done](QRestReply& reply) {
    if (isError(reply) && reply.httpStatus() != 404) {
      handleError(*this, reply);
      done({.error = u"Failed to delete list entry."_s});
      return;
    }

    done({.success = true});
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::findEntryId(const int mediaId, std::function<void(bool, int64_t)> done) {
  const QJsonDocument data{{
      {"query", gql("MediaListId")},
      {"variables",
       QJsonObject{
           {"userName", QString::fromStdString(taiga::accounts.anilistUsername())},
           {"mediaId", mediaId},
       }},
  }};

  const auto callback = [this, done](QRestReply& reply) {
    // AniList answers 404 when the anime isn't in the list.
    if (reply.httpStatus() == 404) {
      done(true, anime::list::kUnknownId);
      return;
    }
    if (isError(reply)) {
      handleError(*this, reply);
      done(false, anime::list::kUnknownId);
      return;
    }
    const auto json = reply.readJson();
    done(true, json ? parseMediaListId(json->object()) : anime::list::kUnknownId);
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::mapIds(const QList<int>& ids, const bool fromMal,
                     std::function<void(bool, QHash<int, int>)> done) {
  QJsonArray idArray;
  for (const auto id : ids) idArray.append(id);

  const QJsonDocument data{{
      {"query", gql(fromMal ? u"MediaIdsByMal"_s : u"MediaIds"_s)},
      {"variables", QJsonObject{{"ids", idArray}}},
  }};

  const auto callback = [this, fromMal, done](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      done(false, {});
      return;
    }
    const auto json = reply.readJson();
    done(true, json ? parseIdMappings(json->object(), fromMal) : QHash<int, int>{});
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

}  // namespace taiga::sync::anilist
