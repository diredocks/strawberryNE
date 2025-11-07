#include "neteasecrypto.h"

#include <QByteArray>
#include <QObject>
#include <QVariantMap>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <qstringliteral.h>

extern "C" {
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
}

const QString NeteaseCrypto::iv = QStringLiteral("0102030405060708");
const QString NeteaseCrypto::presetKey = QStringLiteral("0CoJUm6Qyw8W8jud");
const QString NeteaseCrypto::linuxapiKey = QStringLiteral("rFgB&h#%2?^eDg:Q");
const QString NeteaseCrypto::base62 = QStringLiteral("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
const QString NeteaseCrypto::publicKey = QStringLiteral(
    "-----BEGIN PUBLIC KEY-----\n"
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDgtQn2JZ34ZC28NWYpAUd98iZ37BUrX/aKzmFbt7clFSs6sXqHauqKWqdtLkF2KexO40H1YTX8z2lSgBBOAxLsvaklV8k4cBFK9snQXE9/DDaFt6Rr7iVZMldczhC0JNgTz+SHXT6CBHuX3e9SdB1Ua44oncaTWz7OBGLbCiK45wIDAQAB"
    "\n-----END PUBLIC KEY-----"
);

const QString NeteaseCrypto::eapiKey = QStringLiteral("e82ckenh8dichen8");
/**
 * @brief 使用AES算法加密数据的函数
 * @param plainText 明文数据
 * @param mode 加密算法
 * @param key 加密密钥
 * @param iv 偏移量
 * @param format 输出格式
 * @return QString 密文数据，如果加密失败则为空字符串
 */
QByteArray NeteaseCrypto::aesEncrypt(const QByteArray& plainText, const QString mode, const QByteArray& key, const QByteArray& iv, QString format) {
    auto cipher = (mode == QStringLiteral("cbc")) ? EVP_aes_128_cbc : /*ecb*/ EVP_aes_128_ecb;
    EVP_CIPHER_CTX* ctx;
    int len;
    unsigned char* ciphertext = new unsigned char[plainText.size() * 10];
    int ciphertext_len;
    if (!(ctx = EVP_CIPHER_CTX_new())) ERR_print_errors_fp(stderr);
    if (1 != EVP_EncryptInit_ex(ctx, cipher(), NULL,
        (unsigned char*)key.constData(),
        (unsigned char*)iv.constData()))
        ERR_print_errors_fp(stderr);
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, (unsigned char*)plainText.constData(), plainText.size()))
        ERR_print_errors_fp(stderr);
    ciphertext_len = len;
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) ERR_print_errors_fp(stderr);
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    auto result = QByteArray((char*)ciphertext, ciphertext_len);
    delete[] ciphertext;
    if (format == QStringLiteral("base64")) {
        return result.toBase64();
    }
    return result.toHex().toUpper();
}

/**
 * @brief 使用AES算法解密数据的函数
 * @param cipherText 密文数据
 * @param mode 解密算法
 * @param key 解密密钥
 * @param iv 偏移量
 * @return QString 明文数据，如果解密失败则为空字符串
 */
QByteArray NeteaseCrypto::aesDecrypt(const QByteArray& cipherText, const QString mode, const QByteArray& key, const QByteArray& iv, QString format)
{
    auto cipher = (mode == QStringLiteral("cbc")) ? EVP_aes_128_cbc : /*ecb*/ EVP_aes_128_ecb;
    EVP_CIPHER_CTX* ctx;

    int len;
    auto cipherText_p = format == QStringLiteral("base64") ? QByteArray::fromBase64(cipherText) : QByteArray::fromHex(cipherText);
    unsigned char* plainText = new unsigned char[cipherText_p.size()];
    int plaintext_len;

    if (!(ctx = EVP_CIPHER_CTX_new())) ERR_print_errors_fp(stderr);

    if (1 != EVP_DecryptInit_ex(ctx, cipher(), NULL,
        (unsigned char*)key.constData(),
        (unsigned char*)iv.constData()))
        ERR_print_errors_fp(stderr);

    if (1 != EVP_DecryptUpdate(ctx, plainText, &len, (unsigned char*)cipherText_p.constData(), cipherText_p.size()))
        ERR_print_errors_fp(stderr);
    plaintext_len = len;

    if (1 != EVP_DecryptFinal_ex(ctx, plainText + len, &len)) ERR_print_errors_fp(stderr);
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    auto result = QByteArray((char*)plainText, plaintext_len);
    delete[] plainText;
    return result;
}

/**
 * @brief rsaEncrypt 公钥加密
 * @param plainText 明文
 * @param strPubKey 公钥
 * @return 加密后数据(Hex格式)
 */
QByteArray NeteaseCrypto::rsaEncrypt(QString plainText, const QString& strPubKey)
{
    QByteArray encryptData;
    QByteArray pubKeyArry = strPubKey.toUtf8();
    uchar* pPubKey = (uchar*)pubKeyArry.data();
    BIO* pKeyBio = BIO_new_mem_buf(pPubKey, pubKeyArry.length());
    if (pKeyBio == NULL) {
        return "";
    }
    RSA* pRsa = RSA_new();
    if (strPubKey.contains(QStringLiteral("BEGIN RSA PUBLIC KEY"))) {
        pRsa = PEM_read_bio_RSAPublicKey(pKeyBio, &pRsa, NULL, NULL);
    }
    else {
        pRsa = PEM_read_bio_RSA_PUBKEY(pKeyBio, &pRsa, NULL, NULL);
    }
    if (pRsa == NULL) {
        BIO_free_all(pKeyBio);
        return "";
    }

    int nLen = RSA_size(pRsa);
    char* pEncryptBuf = new char[nLen];


    if (plainText.length() < 128) {
        // 如果小于128，就用0填充空位，直到长度为128
        plainText.prepend(QString().fill(QChar(), 128 - plainText.length()));
    }
    QByteArray plainData = plainText.toUtf8();
    int nClearDataLen = plainData.length();
    uchar* pClearData = (uchar*)plainData.data();

    int nSize = RSA_public_encrypt(nClearDataLen,
        pClearData,
        (unsigned char*)pEncryptBuf,
        pRsa,
        RSA_NO_PADDING);

    if (nSize >= 0) {
        QByteArray arry((char*)pEncryptBuf, nSize);
        encryptData.append(arry);
    }

    // 释放内存
    delete[] pEncryptBuf;
    BIO_free_all(pKeyBio);
    RSA_free(pRsa);
    return encryptData;
}


QVariantMap NeteaseCrypto::weapi(const QJsonDocument &object) {
    const QByteArray text = object.toJson(QJsonDocument::Compact);  // QByteArray 更合适

    // 创建一个长度为16的字节数组
    QByteArray secretKey;
    secretKey.resize(16);

    for (int i = 0; i < secretKey.size(); i++) {
        quint8 byte = QRandomGenerator::global()->generate() & 0xFF;
        int index = byte % base62.length();
        secretKey[i] = base62.at(index).toLatin1();
    }

    QByteArray presetKeyBytes = presetKey.toUtf8();
    QByteArray ivBytes = iv.toUtf8();

    QByteArray firstEncrypt = aesEncrypt(
        text,
        QStringLiteral("cbc"),
        presetKeyBytes.constData(),
        ivBytes.constData(),
        QStringLiteral("base64")
    );

    QByteArray params = aesEncrypt(
        firstEncrypt,
        QStringLiteral("cbc"),
        secretKey.constData(),
        ivBytes.constData(),
        QStringLiteral("base64")
    );

    std::reverse(secretKey.begin(), secretKey.end());
    QByteArray encSecKey = rsaEncrypt(QString::fromUtf8(secretKey), publicKey).toHex();

    return QVariantMap{
        { QStringLiteral("params"), QString::fromUtf8(params) },
        { QStringLiteral("encSecKey"), QString::fromUtf8(encSecKey) }
    };
}

QVariantMap NeteaseCrypto::linuxapi(const QJsonDocument &object) {
    const QByteArray text = object.toJson(QJsonDocument::Indented);
    QByteArray keyBytes = linuxapiKey.toUtf8();
    QByteArray emptyIv;

    QByteArray encrypted = aesEncrypt(
        text,
        QStringLiteral("ecb"),
        keyBytes.constData(),
        emptyIv.constData(),
        QStringLiteral("hex")
    );

    return QVariantMap{
        { QStringLiteral("eparams"), QString::fromUtf8(encrypted) }
    };
}

QVariantMap NeteaseCrypto::eapi(const QString &url, const QJsonDocument &object) {
    const QByteArray text = object.toJson(QJsonDocument::Indented);

    const QString message = QStringLiteral("nobody")
        + url
        + QStringLiteral("use")
        + QString::fromUtf8(text)
        + QStringLiteral("md5forencrypt");

    const QByteArray digest = QCryptographicHash::hash(message.toUtf8(), QCryptographicHash::Md5).toHex();

    const QString data = url
        + QStringLiteral("-36cd479b6b5-")
        + QString::fromUtf8(text)
        + QStringLiteral("-36cd479b6b5-")
        + QString::fromUtf8(digest);

    QByteArray keyBytes = eapiKey.toUtf8();
    QByteArray emptyIv;

    QByteArray encrypted = aesEncrypt(
        data.toUtf8(),
        QStringLiteral("ecb"),
        keyBytes.constData(),
        emptyIv.constData(),
        QStringLiteral("hex")
    );

    return QVariantMap{
        { QStringLiteral("params"), QString::fromUtf8(encrypted) }
    };
}

QVariantMap NeteaseCrypto::eapiResDecrypt(const QByteArray &encryptedParams) {
    QByteArray keyBytes = eapiKey.toUtf8();
    QByteArray emptyIv;

    QByteArray decryptedData = aesDecrypt(
        encryptedParams,
        QStringLiteral("ecb"),
        keyBytes,
        emptyIv,
        QStringLiteral("hex")
    );

    QJsonDocument doc = QJsonDocument::fromJson(decryptedData);
    return doc.toVariant().toMap();
}

QVariantMap NeteaseCrypto::eapiReqDecrypt(const QByteArray& encryptedParams) {
    // 使用aesDecrypt解密参数
    auto decryptedData = aesDecrypt(encryptedParams, QStringLiteral("ecb"), eapiKey.toUtf8(), "", QStringLiteral("hex"));
    // 使用正则表达式解析出URL和数据
    QRegularExpression regex(QStringLiteral("(.*?)-36cd479b6b5-(.*?)-36cd479b6b5-(.*)"));
    QRegularExpressionMatch match = regex.match(QString::fromUtf8(decryptedData));
    if (match.hasMatch()) {
        const auto url = match.captured(1);
        const auto data = QJsonDocument::fromJson(match.captured(2).toUtf8()).toVariant().toMap();
        return {
            { QStringLiteral("url"), url },
            { QStringLiteral("data"), data }
        };
    }

    // 如果没有匹配到，返回null
    return {};
}
