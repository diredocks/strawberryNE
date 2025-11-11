#ifndef NETEASEBASEREQUEST_H
#define NETEASEBASEREQUEST_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QPair>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSslError>
#include <QJsonObject>

#include "core/jsonbaserequest.h"
#include "core/networkaccessmanager.h"

class QNetworkReply;
class QNetworkAccessManager;
class NeteaseService;

class NeteaseBaseRequest : public QObject {
  Q_OBJECT

 public:
  explicit NeteaseBaseRequest(const NeteaseService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  enum class Type {
    None,
    FavouriteArtists,
    FavouriteAlbums,
    FavouriteSongs,
    SearchArtists,
    SearchAlbums,
    SearchSongs,
    StreamURL,
  };

  using JsonValueResult  = JsonBaseRequest::JsonValueResult;
  using JsonObjectResult = JsonBaseRequest::JsonObjectResult;
  using JsonArrayResult = JsonBaseRequest::JsonArrayResult;
  using ErrorCode = JsonBaseRequest::ErrorCode;

  static JsonObjectResult ParseJsonObject(QNetworkReply *reply);

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

 protected:
  inline JsonObjectResult GetJsonObject(const QByteArray &data) { return JsonBaseRequest::GetJsonObject(data); }
  inline JsonValueResult GetJsonValue(const QJsonObject &json_object, const QString &name) { return JsonBaseRequest::GetJsonValue(json_object, name); }
  inline JsonObjectResult GetJsonObject(const QJsonObject &json_object, const QString &name) { return JsonBaseRequest::GetJsonObject(json_object, name); }
  inline JsonArrayResult GetJsonArray(const QJsonObject &json_object, const QString &name) { return JsonBaseRequest::GetJsonArray(json_object, name); }

  QNetworkReply *CreatePostRequest(const QString &ressource_name, const ParamList &params_provided);

  const NeteaseService *service_;
  QList<QNetworkReply*> replies_;

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 private:
  const SharedPtr<QNetworkAccessManager> network_;
};

#endif
