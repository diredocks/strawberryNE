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

#include "includes/scoped_ptr.h"
#include "core/jsonbaserequest.h"
#include "neteaseservice.h"

class QNetworkAccessManager;
class QNetworkReply;

class NeteaseBaseRequest : public QObject {
  Q_OBJECT

 public:
  explicit NeteaseBaseRequest(NeteaseService *service, QObject *parent = nullptr);

  using JsonObjectResult = JsonBaseRequest::JsonObjectResult;
  using ErrorCode = JsonBaseRequest::ErrorCode;

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

 public:
  // static QUrl CreateUrl(const QUrl &server_url, const SubsonicSettings::AuthMethod auth_method, const QString &username, const QString &password, const QString &ressource_name, const ParamList &params_provided);

 protected:
  QNetworkReply *CreatePostRequest(const QString &ressource_name, const ParamList &params_provided) const;
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

  // virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

 private Q_SLOTS:
  // void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 private:
  NeteaseService *service_;
  ScopedPtr<QNetworkAccessManager> network_;
};

#endif
