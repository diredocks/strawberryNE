#include <QApplication>
#include <QThread>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <qcontainerfwd.h>
#include <qtypes.h>

#include "includes/shared_ptr.h"
#include "netease/neteaseservice.h"
#include "utilities/neteaseprovider.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "neteaselyricsprovider.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;
// using namespace NeteaseLyricsProvider;

NeteaseLyricsProcvider::NeteaseLyricsProcvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(u"Netease"_s, true, false, network, parent) {}

void NeteaseLyricsProcvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  LyricsSearchContextPtr search = make_shared<LyricsSearchContext>();
  search->id = id;
  search->request = request;
  requests_search_.append(search);

  SendSearchRequest(search);

}

bool NeteaseLyricsProcvider::SendSearchRequest(LyricsSearchContextPtr search) {

  const QUrl url(QLatin1String(NeteaseService::kApiUrl) + "/api/cloudsearch/pc"_L1);
  QUrlQuery url_query;
  url_query.addQueryItem(u"type"_s, u"1"_s);
  url_query.addQueryItem(u"s"_s, QString::fromLatin1(QUrl::toPercentEncoding(search->request.title + u" "_s + search->request.artist)));
  QNetworkReply *reply = CreateGetRequest(url, url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search]() { HandleSearchReply(reply, search); });

  qLog(Debug) << "NeteaseLyrics: Sending request for" << url;

  return true;

}

NeteaseLyricsProcvider::JsonObjectResult NeteaseLyricsProcvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return JsonObjectResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains(u"msg"_s) && json_object.contains("code"_L1)) {
        const int code = json_object["code"_L1].toInt();
        const QString message = json_object[u"msg"_s].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(code);
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  return result;

}

void NeteaseLyricsProcvider::HandleSearchReply(QNetworkReply *reply, LyricsSearchContextPtr search) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  // const QScopeGuard end_search = qScopeGuard([this, search]() { EndSearch(search); });

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  QJsonValue value_type;
  if (json_object.contains("result"_L1)) {
    value_type = json_object["result"_L1];
  } else {
    Error(u"Json reply is missing result"_s, json_object);
    return;
  }

  if (!value_type.isObject()) {
    Error(u"Json result is not a object."_s, value_type);
    return;
  }
  const QJsonObject object_type = value_type.toObject();

  if (!object_type.contains("songs"_L1)) {
    Error(u"Json result object does not contain songs."_s, object_type);
    return;
  }
  const QJsonValue value_items = object_type["songs"_L1];

  if (!value_items.isArray()) {
    Error(u"Json result object songs is not a array."_s, value_items);
    return;
  }
  const QJsonArray array_items = value_items.toArray();

  for (int i = 0; const QJsonValue &value_track: array_items) {
    if (!value_track.isObject()) {
      continue;
    }
    QJsonObject object_track = value_track.toObject();

    if (!object_track.contains("id"_L1)) {
      Error(u"Invalid Json reply, song is missing id."_s, object_track);
      continue;
    }

    const qint64 track_id = object_track["id"_L1].toInteger();
    if (search->active_tracks_.contains(track_id)) continue;

    LyricsSearchResult res;

    const QJsonValue value_title = object_track["name"_L1];
    if (!value_title.isString()) {
      Error("Invalid Json reply, name is not a string."_L1, value_title);
      continue;
    }
    res.title = object_track["name"_L1].toString();

    const QJsonArray array_artists = object_track["ar"_L1].toArray();
    if (!array_artists.isEmpty()) {
      const QJsonValue first_artist = array_artists.first();
      if (!first_artist.isObject() || !first_artist.toObject().contains("name"_L1)) {
        Error("Invalid Json reply, artist entry missing name."_L1, first_artist);
        continue;
      }
      res.artist = first_artist.toObject()["name"_L1].toString();
    }

    const QJsonValue value_album = object_track["al"_L1];
    if (!value_album.isObject()) {
      Error("Invalid Json reply, album is not a object."_L1, value_album);
      continue;
    }
    const QJsonObject object_album = value_album.toObject();

    if (!object_album.contains("name"_L1) || !object_album.contains("picUrl"_L1)) {
      Error("Invalid Json reply, album missing name or picUrl."_L1, object_album);
      continue;
    }
    res.album = object_album["name"_L1].toString();

    res.score += 0.1 * (array_items.size() - i);
    i++;

    search->active_tracks_.insert(track_id, res);
  }

  for (auto it = search->active_tracks_.cbegin(); it != search->active_tracks_.cend(); ++it) {
    SendLyricsRequest(search, it.key());
  }

}

bool NeteaseLyricsProcvider::SendLyricsRequest(LyricsSearchContextPtr search, const qint64 track_id) {

  const QUrl url(QLatin1String(NeteaseService::kApiUrl) + "/api/song/lyric"_L1);
  
  ParamList params = ParamList() << Param("id"_L1, QString::number(track_id))
                                 << Param("tv"_L1, "-1"_L1)
                                 << Param("lv"_L1, "-1"_L1)
                                 << Param("rv"_L1, "-1"_L1)
                                 << Param("kv"_L1, "-1"_L1)
                                 << Param("_nmclfl"_L1, "-1"_L1);
  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)),QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QNetworkReply *reply = CreateGetRequest(url, url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search, track_id]() { HandleLyricsReply(reply, search, track_id); });

  qLog(Debug) << "NeteaseLyrics: Sending request for id" << track_id;

  return true;

}

void NeteaseLyricsProcvider::HandleLyricsReply(QNetworkReply *reply, LyricsSearchContextPtr search, const qint64 track_id) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QScopeGuard end_search = qScopeGuard([this, search, track_id]() { EndSearch(search, track_id); });


  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  QJsonValue value_lrc;
  if (json_object.contains("lrc"_L1)) {
    value_lrc = json_object["lrc"_L1];
  } else {
    Error("Json reply is missing lrc"_L1, json_object);
    return;
  }

  if (!value_lrc.isObject()) {
    Error("Json lrc is not a object."_L1, value_lrc);
    return;
  }
  const QJsonObject object_lrc = value_lrc.toObject();

  if (!object_lrc.contains("lyric"_L1)) {
    Error("Json lrc object does not contain lyric."_L1, object_lrc);
    return;
  }
  const QJsonValue value_lyric = object_lrc["lyric"_L1];

  if (!value_lyric.isString()) {
    Error("Json lyric value is not string."_L1, value_lyric);
  }

  LyricsSearchResult result = search->active_tracks_.take(track_id);
  result.lyrics = object_lrc["lyric"_L1].toString();
  if (!result.lyrics.isEmpty()) {
    result.lyrics = NeteaseProvider::LrcToString(result.lyrics);
    search->results.append(result);
  }

}

void NeteaseLyricsProcvider::EndSearch(LyricsSearchContextPtr search, const qint64 track_id) {

  search->active_tracks_.remove(track_id);

  if (search->active_tracks_.isEmpty()) {
    requests_search_.removeAll(search);
    if (search->results.isEmpty()) {
      qLog(Debug) << "NeteaseLyrics: No lyrics for" << search->request.artist << search->request.title;
    } else {
      qLog(Debug) << "NeteaseLyrics: Got lyrics for" << search->request.artist << search->request.title;
    }
    Q_EMIT SearchFinished(search->id, search->results);
  }

}
