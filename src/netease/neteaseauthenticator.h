#ifndef NETEASEAUTHENTICATOR_H
#define NETEASEAUTHENTICATOR_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QSslError>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"

class QTimer;
class QNetworkReply;
class NetworkAccessManager;
class LocalRedirectServer;

class NeteaseAuthenticator: public QObject {
  Q_OBJECT

 public:
  explicit NeteaseAuthenticator(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~NeteaseAuthenticator() override;

  bool authenticated() const { return !cookies_.empty(); }

  QList<QNetworkCookie> cookies() const;

  void Authenticate();
  void ClearSession();
  void LoadSession();

 private:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  void StartCheckLoginTimer();
  // QNetworkReply *CreateAccessTokenRequest(const ParamList &params, const bool refresh_token);
  // void RequestAccessToken(const QString &code = QString(), const QUrl &redirect_url = QUrl());
  // void QrCodeUrlReceived(const QUrl &qrcode_url);

 Q_SIGNALS:
  void Error(const QString &error);
  void AuthenticationFinished(const bool success, const QString &error = QString());

 private Q_SLOTS:
  // void RedirectArrived();
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);
  void AnonimousRegisterFinished(QNetworkReply *reply);
  // void AccessTokenRequestFinished(QNetworkReply *reply, const bool refresh_token);

 private:
  const SharedPtr<NetworkAccessManager> network_;
  QTimer *timer_check_login_;

  QList<QNetworkCookie> cookies_;

  QList<QNetworkReply*> replies_;
};

#endif
