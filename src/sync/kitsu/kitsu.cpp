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

#include "kitsu.hpp"

#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRestReply>
#include <QUrlQuery>
#include <ranges>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "sync/id_parsers.hpp"
#include "sync/kitsu/kitsu_error.hpp"
#include "sync/kitsu/kitsu_parsers.hpp"
#include "sync/kitsu/kitsu_utils.hpp"
#include "sync/queue.hpp"
#include "taiga/accounts.hpp"

// Kitsu API documentation:
// https://kitsu.docs.apiary.io

namespace taiga::sync::kitsu {

namespace {

constexpr int kLibraryPageLimit = 500;

// Kitsu's JSON:API configuration sets a maximum page size of 20 on this resource. Asking for
// more results returns an error: "Limit exceeds maximum page size of 20."
constexpr int kSearchPageLimit = 20;

}  // namespace

Service::Service() : taiga::sync::Service{ServiceId::Kitsu} {
  api_.setBaseUrl(QUrl{kApiUrl});

  auto headers = api_.commonHeaders();
  headers.append(QHttpHeaders::WellKnownHeader::Accept, kJsonApiMediaType);
  api_.setCommonHeaders(headers);

  if (const auto token = taiga::accounts.kitsuAccessToken(); !token.empty()) {
    api_.setBearerToken(QByteArray::fromStdString(token));
  }
}

Service* Service::instance() {
  static auto service = new Service();
  return service;
}

////////////////////////////////////////////////////////////////////////////////

void Service::fetchAnime(const int id) {
  const QUrlQuery query{{
      {"include", "categories,animeProductions,animeProductions.producer"},
      {"fields[anime]", animeFields()},
      {"fields[animeProductions]", "producer"},
      {"fields[categories]", "title"},
      {"fields[producers]", "name"},
  }};

  const auto callback = [this, id](QRestReply& reply) {
    if (isError(reply)) {
      if (retryOnTokenExpiry(reply, [this, id] { fetchAnime(id); })) return;
      if (reply.httpStatus() == 404) {
        taiga::sync::invalidateAnime(id);
      } else {
        handleError(*this, reply);
      }
      return;
    }

    const auto json = reply.readJson();
    if (!json) {
      handleError(*this, reply, "Could not parse anime object.");
      return;
    }

    const auto root = json->object();
    const auto item = parseAnime(root["data"], root["included"].toArray());

    if (!item) {
      handleError(*this, reply, "Could not parse anime object.");
      return;
    }

    anime::db.updateItem(*item);
  };

  manager_.get(api_.createRequest(u"/anime/%1"_s.arg(id), query), this, callback);
}

void Service::fetchListEntries(const int offset, QSet<int> fetchedIds) {
  // Library entries are filtered by numeric user ID rather than username, so it must be
  // resolved first.
  if (taiga::accounts.kitsuUserId().empty()) {
    resolveUser([this, offset, fetchedIds] { fetchListEntries(offset, fetchedIds); });
    return;
  }

  const QUrlQuery query{{
      {"filter[user_id]", QString::fromStdString(taiga::accounts.kitsuUserId())},
      {"filter[kind]", "anime"},
      {"include", "anime"},
      {"page[offset]", QString::number(offset)},
      {"page[limit]", QString::number(kLibraryPageLimit)},
      {"fields[anime]", animeFields(true)},
      {"fields[libraryEntries]", libraryEntryFields()},
  }};

  const auto callback = [this, offset, fetchedIds](QRestReply& reply) mutable {
    if (isError(reply)) {
      if (retryOnTokenExpiry(
              reply, [this, offset, fetchedIds] { fetchListEntries(offset, fetchedIds); })) {
        return;
      }
      handleError(*this, reply);
      return;
    }

    const auto json = reply.readJson();
    if (!json) {
      handleError(*this, reply, "Could not parse anime list.");
      return;
    }

    const auto root = json->object();

    QList<Anime> items;
    for (const auto& value : root["included"].toArray()) {
      if (const auto item = parseAnime(value)) {
        items.append(*item);
      }
    }
    anime::db.updateItems(items);

    QList<ListEntry> entries;
    for (const auto& value : root["data"].toArray()) {
      const auto entryObject = value.toObject();
      const auto animeId = entryObject["relationships"]["anime"]["data"]["id"].toVariant().toInt();
      if (const auto entry = parseListEntry(entryObject, animeId)) {
        entries.append(*entry);
        fetchedIds.insert(animeId);
      }
    }
    anime::db.updateEntries(entries);

    if (const auto nextOffset = pagingOffset(root["links"].toObject(), u"next"_s)) {
      fetchListEntries(*nextOffset, fetchedIds);
    } else {
      taiga::sync::pruneMissingEntries(fetchedIds);
      emit listEntriesFetched();
    }
  };

  manager_.get(api_.createRequest(u"/library-entries"_s, query), this, callback);
}

void Service::search(const SearchParams& params, const int offset) {
  QUrlQuery query;

  if (!params.text.isEmpty()) {
    query.addQueryItem(u"filter[text]"_s, params.text);
  }
  if (params.season) {
    query.addQueryItem(u"filter[season]"_s, fromSeasonName(*params.season));
  }
  if (params.year) {
    query.addQueryItem(u"filter[season_year]"_s, QString::number(*params.year));
  }
  if (params.type) {
    query.addQueryItem(u"filter[subtype]"_s, fromType(*params.type));
  }
  if (params.status) {
    query.addQueryItem(u"filter[status]"_s, fromStatus(*params.status));
  }
  if (const auto sort = fromSearchParams(params); !sort.isEmpty()) {
    query.addQueryItem(u"sort"_s, sort);
  }
  query.addQueryItem(u"page[offset]"_s, QString::number(offset));
  query.addQueryItem(u"page[limit]"_s, QString::number(kSearchPageLimit));
  query.addQueryItem(u"fields[anime]"_s, animeFields());

  const auto callback = [this, params, offset](QRestReply& reply) {
    if (isError(reply)) {
      if (retryOnTokenExpiry(reply, [this, params, offset] { search(params, offset); })) return;
      handleError(*this, reply);
      emit searchCompleted(params, {});
      return;
    }

    const auto json = reply.readJson();
    if (!json) {
      handleError(*this, reply, "Could not parse search results.");
      emit searchCompleted(params, {});
      return;
    }

    const auto root = json->object();

    QList<int> ids;
    for (const auto& value : root["data"].toArray()) {
      if (const auto item = parseAnime(value)) {
        anime::db.updateItem(*item);
        ids.append(item->id);
      }
    }

    emit searchCompleted(params, ids);

    // Auto-paginate only for season browsing.
    const bool seasonScoped = params.season.has_value() && params.year.has_value();
    if (seasonScoped) {
      if (const auto nextOffset = pagingOffset(root["links"].toObject(), u"next"_s)) {
        search(params, *nextOffset);
      }
    }
  };

  manager_.get(api_.createRequest(u"/anime"_s, query), this, callback);
}

void Service::addListEntry(const int id, const anime::list::Fields dirty) {
  const auto listEntry = anime::db.entry(id);
  if (!listEntry) return;

  createEntry(*listEntry, dirty, completeQueueItem);
}

void Service::updateListEntry(const int id, const anime::list::Fields dirty) {
  const auto listEntry = anime::db.entry(id);
  if (!listEntry) return;

  patchEntry(*listEntry, dirty, completeQueueItem);
}

void Service::deleteListEntry(const int id) {
  const auto listEntry = anime::db.entry(id);
  if (!listEntry) return;

  deleteEntry(listEntry->id, completeQueueItem);
}

void Service::completeQueueItem(const EntryResult& result) {
  if (result.remote) {
    taiga::sync::queue.complete(*result.remote);
  } else {
    taiga::sync::queue.complete(result.success, result.error);
  }
}

void Service::createEntry(const ListEntry& listEntry, const anime::list::Fields dirty,
                          EntryCallback done) {
  // New library entries belong to a user, whose ID must be resolved first.
  if (taiga::accounts.kitsuUserId().empty()) {
    resolveUser([this, listEntry, dirty, done] { createEntry(listEntry, dirty, done); });
    return;
  }

  const QUrlQuery query{{"fields[libraryEntries]", libraryEntryFields()}};
  auto request = api_.createRequest(u"/library-entries"_s, query);
  request.setHeader(QNetworkRequest::ContentTypeHeader, kJsonApiMediaType);

  const auto body = buildLibraryEntryObject(listEntry, dirty,
                                            QString::fromStdString(taiga::accounts.kitsuUserId()));

  const int id = listEntry.anime_id;

  const auto callback = [this, id, listEntry, dirty, done](QRestReply& reply) {
    const auto json = reply.readJson();

    // Kitsu returns 422 if the anime is already in the user's list. Treat this as a
    // successful (idempotent) add rather than an error.
    if (reply.httpStatus() == 422) {
      const auto errors = json ? json->object()["errors"].toArray() : QJsonArray{};
      const bool duplicate = std::ranges::any_of(errors, [](const QJsonValue& value) {
        return value["detail"].toString().contains(u"has already been taken"_s);
      });
      if (duplicate) {
        done({.success = true});
        return;
      }
    }

    if (isError(reply)) {
      if (retryOnTokenExpiry(
              reply, [this, listEntry, dirty, done] { createEntry(listEntry, dirty, done); })) {
        return;
      }
      handleError(*this, reply, json);
      done({.error = u"Failed to add list entry."_s});
      return;
    }

    done({
        .success = true,
        .remote = json ? parseListEntry(json->object()["data"].toObject(), id) : std::nullopt,
    });
  };

  manager_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact), this, callback);
}

void Service::patchEntry(const ListEntry& listEntry, const anime::list::Fields dirty,
                         EntryCallback done) {
  const QUrlQuery query{{"fields[libraryEntries]", libraryEntryFields()}};
  auto request = api_.createRequest(u"/library-entries/%1"_s.arg(listEntry.id), query);
  request.setHeader(QNetworkRequest::ContentTypeHeader, kJsonApiMediaType);

  const auto body = buildLibraryEntryObject(listEntry, dirty,
                                            QString::fromStdString(taiga::accounts.kitsuUserId()));

  const int id = listEntry.anime_id;

  const auto callback = [this, id, listEntry, dirty, done](QRestReply& reply) {
    if (isError(reply)) {
      if (retryOnTokenExpiry(
              reply, [this, listEntry, dirty, done] { patchEntry(listEntry, dirty, done); })) {
        return;
      }
      handleError(*this, reply,
                  reply.httpStatus() == 404 ? u"Anime list entry does not exist."_s : QString{});
      done({.error = u"Failed to update list entry."_s});
      return;
    }

    const auto json = reply.readJson();
    done({
        .success = true,
        .remote = json ? parseListEntry(json->object()["data"].toObject(), id) : std::nullopt,
    });
  };

  manager_.patch(request, QJsonDocument(body).toJson(QJsonDocument::Compact), this, callback);
}

void Service::deleteEntry(const int64_t entryId, EntryCallback done) {
  const auto callback = [this, entryId, done](QRestReply& reply) {
    if (isError(reply) && reply.httpStatus() != 404) {
      if (retryOnTokenExpiry(reply, [this, entryId, done] { deleteEntry(entryId, done); })) return;
      handleError(*this, reply);
      done({.error = u"Failed to delete list entry."_s});
      return;
    }

    done({.success = true});
  };

  manager_.deleteResource(api_.createRequest(u"/library-entries/%1"_s.arg(entryId)), this,
                          callback);
}

void Service::findEntryId(const int animeId, std::function<void(bool, int64_t)> done) {
  if (taiga::accounts.kitsuUserId().empty()) {
    resolveUser([this, animeId, done] { findEntryId(animeId, done); });
    return;
  }

  const QUrlQuery query{{
      {"filter[user_id]", QString::fromStdString(taiga::accounts.kitsuUserId())},
      {"filter[anime_id]", QString::number(animeId)},
      {"fields[libraryEntries]", "id"},
  }};

  const auto callback = [this, animeId, done](QRestReply& reply) {
    if (isError(reply)) {
      if (retryOnTokenExpiry(reply, [this, animeId, done] { findEntryId(animeId, done); })) return;
      handleError(*this, reply);
      done(false, anime::list::kUnknownId);
      return;
    }
    const auto json = reply.readJson();
    done(true, json ? parseFirstResourceId(json->object()) : anime::list::kUnknownId);
  };

  manager_.get(api_.createRequest(u"/library-entries"_s, query), this, callback);
}

void Service::findMalId(const int animeId, std::function<void(bool, int)> done) {
  const auto callback = [this, done](QRestReply& reply) {
    if (reply.httpStatus() == 404) {
      done(true, 0);
      return;
    }
    if (isError(reply)) {
      handleError(*this, reply);
      done(false, 0);
      return;
    }
    const auto json = reply.readJson();
    done(true, json ? parseMalIdFromMappings(json->object()) : 0);
  };

  manager_.get(api_.createRequest(u"/anime/%1/mappings"_s.arg(animeId)), this, callback);
}

void Service::findIdFromMal(const int malId, std::function<void(bool, int)> done) {
  const QUrlQuery query{{
      {"filter[externalSite]", "myanimelist/anime"},
      {"filter[externalId]", QString::number(malId)},
      {"include", "item"},
  }};

  const auto callback = [this, done](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      done(false, 0);
      return;
    }
    const auto json = reply.readJson();
    done(true, json ? parseAnimeIdFromMappings(json->object()) : 0);
  };

  manager_.get(api_.createRequest(u"/mappings"_s, query), this, callback);
}

}  // namespace taiga::sync::kitsu
