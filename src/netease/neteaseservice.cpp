#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QTimer>

#include "constants/neteasesettings.h"
#include "neteaseservice.h"
#include "neteaseauthenticator.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/database.h"
#include "core/networkaccessmanager.h"
#include "core/oauthenticator.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"

using namespace Qt::Literals::StringLiterals;

const Song::Source NeteaseService::kSource = Song::Source::Netease;
const char NeteaseService::kApiUrl[] = "https://interface.music.163.com";
const char NeteaseService::kWebApiUrl[] = "https://music.163.com";

NeteaseService::NeteaseService(const SharedPtr<TaskManager> task_manager,
                               const SharedPtr<Database> database,
                               const SharedPtr<NetworkAccessManager> network,
                               const SharedPtr<AlbumCoverLoader> albumcover_loader,
                               QObject *parent)
    : StreamingService(Song::Source::Netease, u"Netease"_s, u"netease"_s, QLatin1String(NeteaseSettings::kSettingsGroup), parent),
      network_(network),
      auth_(new NeteaseAuthenticator(this, network, this)),
      enabled_(false) {

  QObject::connect(auth_.get(), &NeteaseAuthenticator::AuthenticationFinished, this, &NeteaseService::AuthFinished);

  auth_->LoadSession();
  Authenticate();

}

NeteaseService::~NeteaseService() { }

void NeteaseService::Exit() {

  Q_EMIT ExitFinished(); // Trigger this to indicate exit finished

}

bool NeteaseService::authenticated() const { return auth_->authenticated(); }

QList<QNetworkCookie> NeteaseService::cookies() const { return auth_->cookies(); }

void NeteaseService::Authenticate() { auth_->Authenticate(); }

void NeteaseService::ClearSession() { auth_->ClearSession(); }

void NeteaseService::AuthFinished(const bool success, const QString &error) {

  qLog(Debug) << success << error;

}
