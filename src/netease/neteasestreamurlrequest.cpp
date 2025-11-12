#include <QObject>
#include <QMimeDatabase>
#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "neteaseservice.h"
#include "neteasebaserequest.h"
#include "neteasestreamurlrequest.h"

using namespace Qt::Literals::StringLiterals;

NeteaseStreamURLRequest::NeteaseStreamURLRequest(NeteaseService *service, const SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent)
    : NeteaseBaseRequest(service, network, parent),
      service_(service),
      reply_(nullptr),
      media_url_(media_url),
      id_(id),
      song_id_(media_url.path().toLongLong()) {}

NeteaseStreamURLRequest::~NeteaseStreamURLRequest() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

}

QUrl NeteaseStreamURLRequest::media_url() const {

  return media_url_;

}

int NeteaseStreamURLRequest::song_id() const {

  return song_id_;

}

void NeteaseStreamURLRequest::Process() {

  if (!service_->authenticated()) {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Not authenticated with Netease."));
    return;
  }

  GetStreamURL();

}

void NeteaseStreamURLRequest::Cancel() {

  if (reply_ && reply_->isRunning()) {
    reply_->abort();
  }
  else {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Cancelled."));
  }

}

void NeteaseStreamURLRequest::GetStreamURL() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  ParamList params = ParamList() << Param("ids"_L1, QStringLiteral("[%1]").arg(song_id_))
                                 << Param("level"_L1, "exhigh"_L1)
                                 << Param("encodeType"_L1, "flac"_L1);
  reply_ = CreatePostRequest("/weapi/song/enhance/player/url/v1"_L1, params);
  QObject::connect(reply_, &QNetworkReply::finished, this, &NeteaseStreamURLRequest::StreamURLReceived);

  // switch (stream_url_method()) {
  //   case NeteaseSettings::StreamUrlMethod::StreamUrl:
  //     TODO: params << Param(u"soundQuality"_s, service_->quality());
  //
  // }

}

void NeteaseStreamURLRequest::StreamURLReceived() {

  if (!reply_) return;

  Q_ASSERT(replies_.contains(reply_));
  replies_.removeAll(reply_);

  const JsonObjectResult json_object_result = ParseJsonObject(reply_);

  QObject::disconnect(reply_, nullptr, this, nullptr);
  reply_->deleteLater();
  reply_ = nullptr;

  if (!json_object_result.success()) {
    Q_EMIT StreamURLFailure(id_, media_url_, json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;

  if (json_object["code"_L1].toInt() != 200) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid response code."_s);
    return;
  }

  const QJsonValue data_val = json_object["data"_L1];
  if (!data_val.isArray() || data_val.toArray().isEmpty()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Missing stream data array."_s);
    return;
  }

  const QJsonObject data_obj = data_val.toArray().first().toObject();

  if (!data_obj.contains("url"_L1)) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Missing stream url."_s);
    return;
  }

  const QUrl url(data_obj["url"_L1].toString());
  if (!url.isValid()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid URL."_s);
    return;
  }

  int samplerate = data_obj["sr"_L1].toInt(-1);

  QString type_str;
  if (data_obj.contains("type"_L1))
    type_str = data_obj["type"_L1].toString().toLower();
  else if (data_obj.contains("encodeType"_L1))
    type_str = data_obj["encodeType"_L1].toString().toLower();

  Song::FileType filetype = Song::FiletypeByExtension(type_str);
  if (filetype == Song::FileType::Unknown) {
    filetype = Song::FiletypeByExtension(QFileInfo(url.path()).suffix());
    if (filetype == Song::FileType::Unknown)
      filetype = Song::FileType::Stream;
  }

  const int track_id = data_obj["id"_L1].toInt();
  if (track_id != song_id_) {
    qLog(Debug) << "Netease returned track ID" << track_id << "for" << media_url_;
  }

  QList<QUrl> urls;
  urls << url;

  Q_EMIT StreamURLSuccess(id_, media_url_, urls.first(), filetype, samplerate);

}

void NeteaseStreamURLRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Netease:" << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

}
