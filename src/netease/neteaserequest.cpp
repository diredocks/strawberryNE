#include <qlogging.h>
#include <qnetworkreply.h>
#include <utility>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QImageReader>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QScopeGuard>

#include "constants/timeconstants.h"
#include "utilities/imageutils.h"
#include "utilities/coverutils.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "neteaseservice.h"
#include "neteasebaserequest.h"
#include "neteaserequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kMaxConcurrentArtistsRequests = 1;
constexpr int kMaxConcurrentAlbumsRequests = 1;
constexpr int kMaxConcurrentSongsRequests = 1;
constexpr int kMaxConcurrentArtistAlbumsRequests = 1;
constexpr int kMaxConcurrentAlbumSongsRequests = 1;
constexpr int kMaxConcurrentAlbumCoverRequests = 10;
constexpr int kFlushRequestsDelay = 200;
}  // namespace

NeteaseRequest::NeteaseRequest(NeteaseService *service, NeteaseUrlHandler *url_handler, const SharedPtr<NetworkAccessManager> network, const Type type, QObject *parent)
    : NeteaseBaseRequest(service, network, parent),
      network_(network),
      url_handler_(url_handler),
      timer_flush_requests_(new QTimer(this)),
      type_(type),
      // TODO: fetchalbums_(service->fetchalbums()),
      query_id_(-1),
      finished_(false),
      artists_requests_total_(0),
      artists_requests_active_(0),
      artists_requests_received_(0),
      artists_total_(0),
      artists_received_(0),
      albums_requests_total_(0),
      albums_requests_active_(0),
      albums_requests_received_(0),
      albums_total_(0),
      albums_received_(0),
      songs_requests_total_(0),
      songs_requests_active_(0),
      songs_requests_received_(0),
      songs_total_(0),
      songs_received_(0),
      artist_albums_requests_total_(),
      artist_albums_requests_active_(0),
      artist_albums_requests_received_(0),
      artist_albums_total_(0),
      artist_albums_received_(0),
      album_songs_requests_active_(0),
      album_songs_requests_received_(0),
      album_songs_requests_total_(0),
      album_songs_total_(0),
      album_songs_received_(0),
      album_covers_requests_total_(0),
      album_covers_requests_active_(0),
      album_covers_requests_received_(0),
      no_results_(false) {

  timer_flush_requests_->setInterval(kFlushRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &NeteaseRequest::FlushRequests);

}

NeteaseRequest::~NeteaseRequest() {

  if (timer_flush_requests_->isActive()) {
    timer_flush_requests_->stop();
  }

}

void NeteaseRequest::Process() {

  if (!service_->authenticated()) {
    Q_EMIT UpdateStatus(query_id_, tr("Authenticating..."));
    return;
  }

  switch (type_) {
    case Type::FavouriteArtists:
      GetArtists();
      break;
    case Type::FavouriteAlbums:
      GetAlbums();
      break;
    case Type::FavouriteSongs:
      GetSongs();
      break;
    case Type::SearchArtists:
      ArtistsSearch();
      break;
    case Type::SearchAlbums:
      AlbumsSearch();
      break;
    case Type::SearchSongs:
      SongsSearch();
      break;
    default:
      Error(u"Invalid query type."_s);
      break;
  }

}

void NeteaseRequest::StartRequests() {

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void NeteaseRequest::FlushRequests() {

  if (!artists_requests_queue_.isEmpty()) {
    FlushArtistsRequests();
    return;
  }

  if (!albums_requests_queue_.isEmpty()) {
    FlushAlbumsRequests();
    return;
  }

  if (!artist_albums_requests_queue_.isEmpty()) {
    FlushArtistAlbumsRequests();
    return;
  }

  if (!album_songs_requests_queue_.isEmpty()) {
    FlushAlbumSongsRequests();
    return;
  }

  if (!songs_requests_queue_.isEmpty()) {
    FlushSongsRequests();
    return;
  }

  if (!album_cover_requests_queue_.isEmpty()) {
    FlushAlbumCoverRequests();
    return;
  }

  timer_flush_requests_->stop();

}

void NeteaseRequest::Search(const int query_id, const QString &search_text) {

  query_id_ = query_id;
  search_text_ = search_text;

}

void NeteaseRequest::GetArtists() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving artists..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsRequest();

}

void NeteaseRequest::AddArtistsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  artists_requests_queue_.enqueue(request);

  ++artists_requests_total_;

  StartRequests();

}

void NeteaseRequest::FlushArtistsRequests() {

  while (!artists_requests_queue_.isEmpty() && artists_requests_active_ < kMaxConcurrentArtistsRequests) {

    const Request request = artists_requests_queue_.dequeue();

    ParamList parameters = ParamList() << Param(u"type"_s, u"100"_s);
    // if (type_ == Type::SearchArtists) {
      parameters << Param(u"s"_s, search_text_);
    // }
    if (request.limit > 0)  parameters << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));

    QNetworkReply *reply = nullptr;
    // if (type_ == Type::FavouriteArtists) {
    //   reply = CreatePostRequest(u"me/following"_s, parameters);
    // }
    // if (type_ == Type::SearchArtists) {
      reply = CreatePostRequest(u"/weapi/search/get"_s, parameters);
    // }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistsReplyReceived(reply, request.limit, request.offset); });

    ++artists_requests_active_;

  }

}

void NeteaseRequest::GetAlbums() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving albums..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsRequest();

}

void NeteaseRequest::AddAlbumsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  albums_requests_queue_.enqueue(request);

  ++albums_requests_total_;

  StartRequests();

}

void NeteaseRequest::FlushAlbumsRequests() {

  while (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) {

    const Request request = albums_requests_queue_.dequeue();

    ParamList parameters = ParamList() << Param(u"type"_s, u"10"_s);
    // if (type_ == Type::SearchAlbums) {
      parameters << Param(u"s"_s, search_text_);
    // }
    if (request.limit > 0)  parameters << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));

    QNetworkReply *reply = nullptr;
    // if (type_ == Type::FavouriteAlbums) {
    //   reply = CreatePostRequest(u"me/albums"_s, parameters);
    // }
    // if (type_ == Type::SearchAlbums) {
      reply = CreatePostRequest(u"/weapi/search/get"_s, parameters);
    // }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumsReplyReceived(reply, request.limit, request.offset); });

    ++albums_requests_active_;

  }

}

void NeteaseRequest::GetSongs() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving songs..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsRequest();

}

void NeteaseRequest::AddSongsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  songs_requests_queue_.enqueue(request);

  ++songs_requests_total_;

  StartRequests();

}

void NeteaseRequest::FlushSongsRequests() {

  while (!songs_requests_queue_.isEmpty() && songs_requests_active_ < kMaxConcurrentSongsRequests) {

    const Request request = songs_requests_queue_.dequeue();

    ParamList parameters;
    // if (type_ == Type::SearchSongs) {
      parameters << Param(u"type"_s, u"1"_s);
      parameters << Param(u"s"_s, search_text_);
    // }
    if (request.limit > 0) parameters << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));

    QNetworkReply *reply = nullptr;
    // if (type_ == Type::FavouriteSongs) {
    //   reply = CreatePostRequest(u"me/tracks"_s, parameters);
    // }
    // if (type_ == Type::SearchSongs) {
      reply = CreatePostRequest(u"/weapi/cloudsearch/get/web"_s, parameters);
    // }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { SongsReplyReceived(reply, request.limit, request.offset); });

    ++songs_requests_active_;

  }

}

void NeteaseRequest::ArtistsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsSearchRequest();

}

void NeteaseRequest::AddArtistsSearchRequest(const int offset) {

  AddArtistsRequest(offset, service_->artistssearchlimit());

}

void NeteaseRequest::AlbumsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsSearchRequest();

}

void NeteaseRequest::AddAlbumsSearchRequest(const int offset) {

  AddAlbumsRequest(offset, service_->albumssearchlimit());

}

void NeteaseRequest::SongsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsSearchRequest();

}

void NeteaseRequest::AddSongsSearchRequest(const int offset) {

  AddSongsRequest(offset, service_->songssearchlimit());

}

void NeteaseRequest::ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  --artists_requests_active_;
  ++artists_requests_received_;

  if (finished_) return;

  int offset = 0;
  int artists_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, limit_requested, &offset, &artists_received]() {
    ArtistsFinishCheck(limit_requested, offset, artists_received);
  });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) return;

  if (!json_object.contains("code"_L1) || json_object["code"_L1].toInt() != 200) {
    Error(u"Unexpected response code."_s, json_object);
    return;
  }

  if (!json_object.contains("result"_L1) || !json_object["result"_L1].isObject()) {
    Error(u"Missing result object."_s, json_object);
    return;
  }

  const QJsonObject obj_result = json_object["result"_L1].toObject();
  if (!obj_result.contains("artists"_L1) || !obj_result["artists"_L1].isArray()) {
    Error(u"Missing artists array."_s, obj_result);
    return;
  }

  const QJsonArray array_artists = obj_result["artists"_L1].toArray();
  if (array_artists.isEmpty()) {
    if (offset_requested == 0) no_results_ = true;
    return;
  }

  if (offset_requested == 0)
    artists_total_ = array_artists.size();

  if (offset_requested == 0)
    Q_EMIT UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));

  for (const QJsonValue &v : array_artists) {
    if (!v.isObject()) {
      Error(u"Invalid JSON: artist entry not object."_s, v);
      continue;
    }

    const QJsonObject artist_obj = v.toObject();

    if (!artist_obj.contains("id"_L1) || !artist_obj.contains("name"_L1)) {
      Error(u"Invalid JSON: artist missing id or name."_s, artist_obj);
      continue;
    }

    const QString artist_id = artist_obj["id"_L1].toVariant().toString();
    const QString artist_name = artist_obj["name"_L1].toString();

    if (artist_albums_requests_pending_.contains(artist_id))
      continue;

    ArtistAlbumsRequest request;
    request.artist.artist_id = artist_id;
    request.artist.artist = artist_name;

    artist_albums_requests_pending_.insert(artist_id, request);
    ++artists_received;
  }

  artists_received_ += artists_received;

  if (offset_requested != 0)
    Q_EMIT UpdateProgress(query_id_, GetProgress(artists_total_, artists_received_));

}

void NeteaseRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

  if (finished_) return;

  if (artists_received > 0 && (limit == 0 || limit > artists_received) && artists_received_ < artists_total_) {
    int offset_next = offset + artists_received;
    if (offset_next > 0 && offset_next < artists_total_) {
      if (type_ == Type::FavouriteArtists) AddArtistsRequest(offset_next);
      else if (type_ == Type::SearchArtists) AddArtistsSearchRequest(offset_next);
    }
  }

  if (artists_requests_queue_.isEmpty() && artists_requests_active_ <= 0) {  // Artist query is finished, get all albums for all artists.

    // Get artist albums
    const QList<ArtistAlbumsRequest> requests = artist_albums_requests_pending_.values();
    for (const ArtistAlbumsRequest &request : requests) {
      AddArtistAlbumsRequest(request.artist);
    }
    artist_albums_requests_pending_.clear();

    if (artist_albums_requests_total_ > 0) {
      if (artist_albums_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving albums for %1 artist...").arg(artist_albums_requests_total_));
      else Q_EMIT UpdateStatus(query_id_, tr("Receiving albums for %1 artists...").arg(artist_albums_requests_total_));
      Q_EMIT UpdateProgress(query_id_, 0);
    }

  }

  FinishCheck();

}

void NeteaseRequest::AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --albums_requests_active_;
  ++albums_requests_received_;
  AlbumsReceived(reply, Artist(), limit_requested, offset_requested);

}

void NeteaseRequest::AddArtistAlbumsRequest(const Artist &artist, const int offset) {

  ArtistAlbumsRequest request;
  request.artist = artist;
  request.offset = offset;
  artist_albums_requests_queue_.enqueue(request);

  ++artist_albums_requests_total_;

  StartRequests();

}

void NeteaseRequest::FlushArtistAlbumsRequests() {

  while (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) {

    const ArtistAlbumsRequest request = artist_albums_requests_queue_.dequeue();

    const ParamList params = ParamList() << Param("offset"_L1, QString::number(request.offset));
                                         // << Param("limit"_L1, QString::number(request.limit));
    QNetworkReply *reply = CreatePostRequest(QStringLiteral("/weapi/artist/albums/%1").arg(request.artist.artist_id), params);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistAlbumsReplyReceived(reply, request.artist, request.offset); });

    ++artist_albums_requests_active_;

  }

}

void NeteaseRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const Artist &artist, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_requests_received_;
  Q_EMIT UpdateProgress(query_id_, GetProgress(artist_albums_requests_received_, artist_albums_requests_total_));
  ArtistAlbumsReceived(reply, artist, 0, offset_requested);

}

void NeteaseRequest::ArtistAlbumsReceived(QNetworkReply *reply, const Artist &artist_artist, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  int offset = offset_requested;
  int albums_total = 0;
  int albums_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, artist_artist, limit_requested, &offset, &albums_total, &albums_received]() {
    AlbumsFinishCheck(artist_artist, limit_requested, offset, albums_total, albums_received);
  });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    Error(u"Empty JSON object."_s);
    return;
  }

  if (!json_object.contains("code"_L1) || json_object["code"_L1].toInt() != 200) {
    Error(u"Invalid JSON reply: missing or non-200 code."_s, json_object);
    return;
  }

  if (!json_object.contains("hotAlbums"_L1) || !json_object["hotAlbums"_L1].isArray()) {
    Error(u"Missing 'hotAlbums' array."_s, json_object);
    return;
  }

  const QJsonArray hot_albums = json_object["hotAlbums"_L1].toArray();
  albums_total = hot_albums.size();
  albums_received = albums_total;

  for (const QJsonValue &value_item : hot_albums) {
    if (!value_item.isObject()) continue;
    const QJsonObject album_obj = value_item.toObject();

    Album album;
    Artist artist = artist_artist;

    album.album_id = QString::number(album_obj["id"_L1].toVariant().toLongLong());
    album.album = album_obj["name"_L1].toString();
    album.cover_url = QUrl(album_obj["picUrl"_L1].toString());

    if (album_obj.contains("artist"_L1) && album_obj["artist"_L1].isObject()) {
      const QJsonObject inner_artist = album_obj["artist"_L1].toObject();
      artist.artist_id = QString::number(inner_artist["id"_L1].toVariant().toLongLong());
      artist.artist = inner_artist["name"_L1].toString();
    }

    if (!album_songs_requests_pending_.contains(album.album_id)) {
      AlbumSongsRequest request;
      request.artist = artist;
      request.album = album;
      album_songs_requests_pending_.insert(album.album_id, request);
    }
  }

}

void NeteaseRequest::AlbumsReceived(QNetworkReply *reply, const Artist &artist_artist, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  int offset = offset_requested;
  int albums_total = 0;
  int albums_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, artist_artist, limit_requested, &offset, &albums_total, &albums_received]() {
    AlbumsFinishCheck(artist_artist, limit_requested, offset, albums_total, albums_received);
  });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    Error(u"Empty JSON object."_s);
    return;
  }

  if (!json_object.contains("code"_L1) || json_object["code"_L1].toInt() != 200) {
    Error(u"Invalid JSON reply: missing or non-200 code."_s, json_object);
    return;
  }

  if (!json_object.contains("result"_L1) || !json_object["result"_L1].isObject()) {
      Error(u"Missing 'result' object."_s, json_object);
      return;
  }

  const QJsonObject result_obj = json_object["result"_L1].toObject();

  if (!result_obj.contains("albums"_L1) || !result_obj["albums"_L1].isArray()) {
      Error(u"Missing 'albums' array in result."_s, result_obj);
      return;
  }

  const QJsonArray albums = result_obj["albums"_L1].toArray();
  albums_total = albums.size();
  albums_received = albums_total;

  for (const QJsonValue &value_item : albums) {
    if (!value_item.isObject()) continue;
    const QJsonObject album_obj = value_item.toObject();

    Album album;
    Artist artist = artist_artist;

    album.album_id = QString::number(album_obj["id"_L1].toVariant().toLongLong());
    album.album = album_obj["name"_L1].toString();
    album.cover_url = QUrl(album_obj["picUrl"_L1].toString());

    if (album_obj.contains("artist"_L1) && album_obj["artist"_L1].isObject()) {
      const QJsonObject inner_artist = album_obj["artist"_L1].toObject();
      artist.artist_id = QString::number(inner_artist["id"_L1].toVariant().toLongLong());
      artist.artist = inner_artist["name"_L1].toString();
    }

    if (!album_songs_requests_pending_.contains(album.album_id)) {
      AlbumSongsRequest request;
      request.artist = artist;
      request.album = album;
      album_songs_requests_pending_.insert(album.album_id, request);
    }
  }

  // if (type_ == Type::FavouriteAlbums || type_ == Type::SearchAlbums) {
    albums_received_ += albums_received;
    Q_EMIT UpdateProgress(query_id_, GetProgress(albums_received_, albums_total_));
  // }

}

void NeteaseRequest::AlbumsFinishCheck(const Artist &artist, const int limit, const int offset, const int albums_total, const int albums_received) {

  if (finished_) return;

  if (albums_received > 0 && (limit == 0 || limit > albums_received)) {
    int offset_next = offset + albums_received;
    if (offset_next > 0 && offset_next < albums_total) {
      switch (type_) {
        case Type::FavouriteAlbums:
          AddAlbumsRequest(offset_next);
          break;
        case Type::SearchAlbums:
          AddAlbumsSearchRequest(offset_next);
          break;
        case Type::FavouriteArtists:
        case Type::SearchArtists:
          AddArtistAlbumsRequest(artist, offset_next);
          break;
        default:
          break;
      }
    }
  }

  if (
      artists_requests_queue_.isEmpty() &&
      artists_requests_active_ <= 0 &&
      albums_requests_queue_.isEmpty() &&
      albums_requests_active_ <= 0 &&
      artist_albums_requests_queue_.isEmpty() &&
      artist_albums_requests_active_ <= 0
      ) { // Artist albums query is finished, get all songs for all albums.

    // Get songs for all the albums.

    for (QMap<QString, AlbumSongsRequest> ::const_iterator it = album_songs_requests_pending_.constBegin(); it != album_songs_requests_pending_.constEnd(); ++it) {
      AlbumSongsRequest request = it.value();
      AddAlbumSongsRequest(request.artist, request.album);
    }
    album_songs_requests_pending_.clear();

    if (album_songs_requests_total_ > 0) {
      if (album_songs_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving songs for %1 album...").arg(album_songs_requests_total_));
      else Q_EMIT UpdateStatus(query_id_, tr("Receiving songs for %1 albums...").arg(album_songs_requests_total_));
      Q_EMIT UpdateProgress(query_id_, 0);
    }
  }

  GetAlbumCoversCheck();

  FinishCheck();

}

void NeteaseRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  ++songs_requests_received_;
  // if (type_ == Type::SearchSongs && fetchalbums_) {
  //   AlbumsReceived(reply, Artist(), limit_requested, offset_requested);
  // }
  // else {
    SongsReceived(reply, Artist(), Album(), limit_requested, offset_requested);
  // }

}

void NeteaseRequest::AddAlbumSongsRequest(const Artist &artist, const Album &album, const int offset) {

  AlbumSongsRequest request;
  request.artist = artist;
  request.album = album;
  request.offset = offset;
  album_songs_requests_queue_.enqueue(request);

  ++album_songs_requests_total_;

  StartRequests();

}

void NeteaseRequest::FlushAlbumSongsRequests() {

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) {
    const AlbumSongsRequest request = album_songs_requests_queue_.dequeue();
    ++album_songs_requests_active_;
    // TODO: if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = CreatePostRequest(QStringLiteral("/weapi/v1/album/%1").arg(request.album.album_id), ParamList());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumSongsReplyReceived(reply, request.artist, request.album, request.offset); });
  }

}

void NeteaseRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int offset_requested) {

  --album_songs_requests_active_;
  ++album_songs_requests_received_;
  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(album_songs_requests_received_, album_songs_requests_total_));
  }
  AlbumSongsReceived(reply, artist, album, 0, offset_requested);

}

void NeteaseRequest::SongsReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  int songs_total = 0;
  int songs_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, artist, album, limit_requested, offset_requested, &songs_total, &songs_received]() {
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, songs_received);
  });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject root = json_object_result.json_object;
  if (root.isEmpty()) {
    Error(u"Empty JSON object."_s);
    return;
  }

  const int code = root.value("code"_L1).toInt();
  if (code != 200) {
    Error(QStringLiteral("Request failed with code %1").arg(code));
    return;
  }
  if (!root.contains("result"_L1) || !root["result"_L1].isObject()) {
    Error(u"Missing result object."_s, root);
    return;
  }

  const QJsonObject result_obj = root["result"_L1].toObject();
  if (!result_obj.contains("songs"_L1) || !result_obj["songs"_L1].isArray()) {
    Error(u"Missing songs array."_s, root);
    return;
  }

  const QJsonArray songs_array = result_obj["songs"_L1].toArray();
  songs_total = songs_array.size();
  if (songs_array.isEmpty()) {
    no_results_ = true;
    return;
  }

  SongList songs;
  songs.reserve(songs_array.size());
  bool compilation = false;
  bool multidisc = false;

  for (const QJsonValue &value_song : songs_array) {
    if (!value_song.isObject()) {
      Error(u"Invalid Json reply, song is not an object."_s);
      continue;
    }

    const QJsonObject song_obj = value_song.toObject();
    ++songs_received;

    Song song(Song::Source::Netease);
    ParseSong(song, song_obj, artist, album);

    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;

    songs << song;
  }

  for (int track_number = 1; Song song : std::as_const(songs)) {
    song.set_track(track_number++);
    if (compilation) song.set_compilation_detected(true);
    if (!multidisc) song.set_disc(0);
    songs_.insert(song.song_id(), song);
  }

  // if (type_ == Type::FavouriteSongs || type_ == Type::SearchSongs) {
    songs_total_ = songs_total;
    songs_received_ += songs_received;
    Q_EMIT UpdateProgress(query_id_, GetProgress(songs_received_, songs_total_));
  // }

}

void NeteaseRequest::AlbumSongsReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  int songs_total = 0;
  int songs_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, artist, album, limit_requested, offset_requested, &songs_total, &songs_received]() {
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, songs_received);
  });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject root = json_object_result.json_object;
  if (root.isEmpty()) {
    Error(u"Empty JSON object."_s);
    return;
  }

  const int code = root.value("code"_L1).toInt();
  if (code != 200) {
    Error(QStringLiteral("Request failed with code %1").arg(code));
    return;
  }

  // album info
  if (!root.contains("album"_L1) || !root["album"_L1].isObject()) {
    Error(u"Missing album object."_s, root);
    return;
  }

  const QJsonObject album_obj = root["album"_L1].toObject();
  const QString album_name = album_obj.value("name"_L1).toString();
  const QString album_pic_url = album_obj.value("picUrl"_L1).toString();
  const QString album_id = album_obj.value("id"_L1).toString();

  // songs array
  if (!root.contains("songs"_L1) || !root["songs"_L1].isArray()) {
    Error(u"Missing songs array."_s, root);
    return;
  }

  const QJsonArray songs_array = root["songs"_L1].toArray();
  songs_total = songs_array.size();
  if (songs_array.isEmpty()) {
    no_results_ = true;
    return;
  }

  SongList songs;
  songs.reserve(songs_array.size());
  bool compilation = false;
  bool multidisc = false;

  for (const QJsonValue &value_song : songs_array) {
    if (!value_song.isObject()) {
      Error(u"Invalid Json reply, song is not an object."_s);
      continue;
    }

    const QJsonObject song_obj = value_song.toObject();
    ++songs_received;

    Song song(Song::Source::Netease);
    ParseSong(song, song_obj, artist, album);

    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;

    songs << song;
  }

  for (int track_number = 1; Song song : std::as_const(songs)) {
    song.set_track(track_number++);
    if (compilation) song.set_compilation_detected(true);
    if (!multidisc) song.set_disc(0);
    songs_.insert(song.song_id(), song);
  }

  // if (type_ == Type::FavouriteSongs || type_ == Type::SearchSongs) {
  //   songs_total_ = songs_total;
  //   songs_received_ += songs_received;
  //   Q_EMIT UpdateProgress(query_id_, GetProgress(songs_received_, songs_total_));
  // }

}

void NeteaseRequest::SongsFinishCheck(const Artist &artist, const Album &album, const int limit, const int offset, const int songs_total, const int songs_received) {

  if (finished_) return;

  if (songs_received > 0 && (limit == 0 || limit > songs_received)) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (type_) {
        case Type::FavouriteSongs:
          AddSongsRequest(offset_next);
          break;
        case Type::SearchSongs:
          // If artist_id and album_id isn't zero it means that it's a songs search where we fetch all albums too. So fallthrough.
          if (artist.artist_id.isEmpty() && album.album_id.isEmpty()) {
            AddSongsSearchRequest(offset_next);
            break;
          }
          // fallthrough
        case Type::FavouriteArtists:
        case Type::SearchArtists:
        case Type::FavouriteAlbums:
        case Type::SearchAlbums:
          AddAlbumSongsRequest(artist, album, offset_next);
          break;
        default:
          break;
      }
    }
  }

  GetAlbumCoversCheck();

  FinishCheck();

}

void NeteaseRequest::ParseSong(Song &song, const QJsonObject &json_obj, const Artist &album_artist, const Album &album) {

  if (!json_obj.contains("id"_L1) || !json_obj.contains("name"_L1) || !json_obj.contains("dt"_L1)) {
    Error(u"Invalid Json reply, song missing required fields."_s, json_obj);
    return;
  }

  QString artist_id;
  QString artist_name;
  if (json_obj.contains("ar"_L1) && json_obj["ar"_L1].isArray()) {
    const QJsonArray array_artists = json_obj["ar"_L1].toArray();
    if (!array_artists.isEmpty() && array_artists.first().isObject()) {
      const QJsonObject first_artist = array_artists.first().toObject();
      artist_id = QString::number(first_artist.value("id"_L1).toInteger());
      artist_name = first_artist.value("name"_L1).toString();
    }
  }

  QString album_id;
  QString album_name;
  QUrl pic_url;
  if (json_obj.contains("al"_L1) && json_obj["al"_L1].isObject()) {
    const QJsonObject obj_album = json_obj["al"_L1].toObject();
    album_id = QString::number(obj_album.value("id"_L1).toInteger());
    album_name = obj_album.value("name"_L1).toString();
    if (obj_album.contains("picUrl"_L1))
      pic_url = QUrl(obj_album.value("picUrl"_L1).toString());
  }

  if (artist_id.isEmpty() || artist_name.isEmpty()) {
    artist_id = album_artist.artist_id;
    artist_name = album_artist.artist;
  }
  if (album_id.isEmpty() || album_name.isEmpty() || pic_url.isEmpty()) {
    album_id = album.album_id;
    album_name = album.album;
    pic_url = album.cover_url;
  }

  const QString song_id = QString::number(json_obj.value("id"_L1).toInteger());
  const QString title = json_obj.value("name"_L1).toString();
  const qint64 duration = json_obj.value("dt"_L1).toInteger() * kNsecPerMsec;

  const int track = 0;
  const int disc = 0;

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);
  song.set_source(Song::Source::Netease);
  song.set_song_id(song_id);
  song.set_album_id(album_id);
  song.set_artist_id(artist_id);
  song.set_album(album_name);
  song.set_artist(artist_name);
  song.set_title(title);
  song.set_track(track);
  song.set_disc(disc);
  song.set_url(url);
  song.set_length_nanosec(duration);
  song.set_art_automatic(pic_url);
  song.set_directory_id(0);
  song.set_filetype(Song::FileType::Stream);
  song.set_filesize(0);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_valid(true);

}

void NeteaseRequest::GetAlbumCoversCheck() {

  if (
      !finished_ &&
      // TODO: service_->download_album_covers() &&
      IsQuery() &&
      artists_requests_queue_.isEmpty() &&
      albums_requests_queue_.isEmpty() &&
      songs_requests_queue_.isEmpty() &&
      artist_albums_requests_queue_.isEmpty() &&
      album_songs_requests_queue_.isEmpty() &&
      album_cover_requests_queue_.isEmpty() &&
      artist_albums_requests_pending_.isEmpty() &&
      album_songs_requests_pending_.isEmpty() &&
      album_covers_requests_sent_.isEmpty() &&
      artists_requests_active_ <= 0 &&
      albums_requests_active_ <= 0 &&
      songs_requests_active_ <= 0 &&
      artist_albums_requests_active_ <= 0 &&
      album_songs_requests_active_ <= 0 &&
      album_covers_requests_active_ <= 0
  ) {
    GetAlbumCovers();
  }

}

void NeteaseRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    AddAlbumCoverRequest(song);
  }

  if (album_covers_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving album cover for %1 album...").arg(album_covers_requests_total_));
  else Q_EMIT UpdateStatus(query_id_, tr("Receiving album covers for %1 albums...").arg(album_covers_requests_total_));
  Q_EMIT UpdateProgress(query_id_, 0);

  StartRequests();

}

void NeteaseRequest::AddAlbumCoverRequest(const Song &song) {

  if (album_covers_requests_sent_.contains(song.album_id())) {
    album_covers_requests_sent_.insert(song.album_id(), song.song_id());
    return;
  }

  AlbumCoverRequest request;
  request.album_id = song.album_id();
  request.url = song.art_automatic();
  request.filename = CoverUtils::CoverFilePath(CoverOptions(), song.source(), song.effective_albumartist(), song.effective_album(), song.album_id(), QString(), request.url);
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insert(song.album_id(), song.song_id());
  ++album_covers_requests_total_;

  album_cover_requests_queue_.enqueue(request);

}

void NeteaseRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {
    const AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    QNetworkReply *reply = network_->get(QNetworkRequest(request.url));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request.album_id, request.url, request.filename); });
    ++album_covers_requests_active_;
  }

}

void NeteaseRequest::AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
  }
  else {
    AlbumCoverFinishCheck();
    return;
  }

  --album_covers_requests_active_;
  ++album_covers_requests_received_;

  if (finished_) return;

  const QScopeGuard finish_check = qScopeGuard([this]() { AlbumCoverFinishCheck(); });

  Q_EMIT UpdateProgress(query_id_, GetProgress(album_covers_requests_received_, album_covers_requests_total_));

  if (!album_covers_requests_sent_.contains(album_id)) {
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    album_covers_requests_sent_.remove(album_id);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  // image/jpg -> image/jpeg
  if (mimetype.compare(u"image/jpg", Qt::CaseInsensitive) == 0) {
    mimetype = u"image/jpeg"_s;
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QStringLiteral("Unsupported mimetype for image reader %1 for %2").arg(mimetype, url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Received empty image data for %1").arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    return;
  }

  QList<QByteArray> format_list = QImageReader::imageFormatsForMimeType(mimetype.toUtf8());
  char *format = nullptr;
  if (!format_list.isEmpty()) {
    format = format_list[0].data();
  }

  QImage image;
  if (image.loadFromData(data, format)) {
    if (image.save(filename, format)) {
      while (album_covers_requests_sent_.contains(album_id)) {
        const QString song_id = album_covers_requests_sent_.take(album_id);
        if (songs_.contains(song_id)) {
          songs_[song_id].set_art_automatic(QUrl::fromLocalFile(filename));
        }
      }
    }
    else {
      Error(QStringLiteral("Error saving image data to %1").arg(filename));
      if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    }
  }
  else {
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    Error(QStringLiteral("Error decoding image data from %1").arg(url.toString()));
  }

}

void NeteaseRequest::AlbumCoverFinishCheck() {

  FinishCheck();

}

void NeteaseRequest::FinishCheck() {

  if (
      !finished_ &&
      artists_requests_queue_.isEmpty() &&
      albums_requests_queue_.isEmpty() &&
      songs_requests_queue_.isEmpty() &&
      artist_albums_requests_queue_.isEmpty() &&
      album_songs_requests_queue_.isEmpty() &&
      album_cover_requests_queue_.isEmpty() &&
      artist_albums_requests_pending_.isEmpty() &&
      album_songs_requests_pending_.isEmpty() &&
      album_covers_requests_sent_.isEmpty() &&
      artists_requests_active_ <= 0 &&
      albums_requests_active_ <= 0 &&
      songs_requests_active_ <= 0 &&
      artist_albums_requests_active_ <= 0 &&
      album_songs_requests_active_ <= 0 &&
      album_covers_requests_active_ <= 0
  ) {
    if (timer_flush_requests_->isActive()) {
      timer_flush_requests_->stop();
    }
    finished_ = true;
    if (no_results_ && songs_.isEmpty()) {
      if (IsSearch()) {
        Q_EMIT Results(query_id_, SongMap(), tr("No match."));
      }
      else {
        Q_EMIT Results(query_id_, SongMap(), QString());
      }
    }
    else {
      if (songs_.isEmpty() && error_.isEmpty()) {
        Q_EMIT Results(query_id_, songs_, tr("Data missing error"));
      }
      else {
        Q_EMIT Results(query_id_, songs_, error_);
      }
    }
  }

}

int NeteaseRequest::GetProgress(const int count, const int total) {

  return static_cast<int>((static_cast<float>(count) / static_cast<float>(total)) * 100.0F);

}

void NeteaseRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Netease:" << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

  error_ = error_message;

}

void NeteaseRequest::Warn(const QString &error_message, const QVariant &debug) {

  qLog(Error) << "Netease:" << error_message;
  if (debug.isValid()) qLog(Debug) << debug;

}
