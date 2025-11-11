#ifndef NETEASESTREAMURLREQUEST_H
#define NETEASESTREAMURLREQUEST_H

#include <QVariant>
#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "neteasebaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class NeteaseService;

class NeteaseStreamURLRequest : public NeteaseBaseRequest {
  Q_OBJECT

 public:
  explicit NeteaseStreamURLRequest(NeteaseService *service, const SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent = nullptr);
  ~NeteaseStreamURLRequest() override;

  void GetStreamURL();
  void Process();
  void Cancel();

  // bool oauth() const;
  // NeteaseSettings::StreamUrlMethod stream_url_method() const;
  QUrl media_url() const;
  int song_id() const;

  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;

 Q_SIGNALS:
  void StreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void StreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate = -1, const int bit_depth = -1, const qint64 duration = -1);

 private Q_SLOTS:
  void StreamURLReceived();

// private:
  // static QList<QUrl> ParseUrls(const QJsonObject &json_object);

 private:
  NeteaseService *service_;
  QNetworkReply *reply_;
  QUrl media_url_;
  uint id_;
  qint64 song_id_;
};

using NeteaseStreamURLRequestPtr = QSharedPointer<NeteaseStreamURLRequest>;

#endif  // NETEASESTREAMURLREQUEST_H
