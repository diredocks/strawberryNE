#ifndef NETEASECRYPTO_H
#define NETEASECRYPTO_H

#include <QString>
#include <QByteArray>
#include <QVariantMap>
#include <qstringliteral.h>

class NeteaseCrypto {
  public:
    static const QString iv;
    static const QString presetKey;
    static const QString linuxapiKey;
    static const QString base62;
    static const QString publicKey;
    static const QString eapiKey;

    static QByteArray aesEncrypt(const QByteArray& plainText, const QString mode, const QByteArray& key, const QByteArray& iv, QString format = QStringLiteral("base64"));
    static QByteArray aesDecrypt(const QByteArray& cipherText, const QString mode, const QByteArray& key, const QByteArray& iv, QString format = QStringLiteral("base64"));
    static QByteArray rsaEncrypt(QString plainText, const QString& strPubKey);
    static QVariantMap weapi(const QJsonDocument &object);
    static QVariantMap linuxapi(const QJsonDocument &object);
    static QVariantMap eapi(const QString &url, const QJsonDocument &object);
    static QVariantMap eapiResDecrypt(const QByteArray& encryptedParams);
    static QVariantMap eapiReqDecrypt(const QByteArray& encryptedParams);
    static QByteArray decrypt(QByteArray cipherBuffer);
};

#endif
