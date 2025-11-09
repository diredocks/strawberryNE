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

#include "neteaseservice.h"

class QNetworkAccessManager;
class QNetworkReply;

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

  using JsonObjectResult = JsonBaseRequest::JsonObjectResult;
  using ErrorCode = JsonBaseRequest::ErrorCode;

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

 protected:
  QNetworkReply *CreatePostRequest(const QString &ressource_name, const ParamList &params_provided) const;
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 private:
  const NeteaseService *service_;
  const SharedPtr<QNetworkAccessManager> network_;
};

#endif
