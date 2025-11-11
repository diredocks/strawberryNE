#include <QUrl>
#include <QTimer>
#include <QString>
#include <QByteArray>

#include "constants/neteasesettings.h"
#include "neteaseservice.h"
#include "neteasebaserequest.h"
#include "neteaserequest.h"
#include "neteaseauthenticator.h"
#include "core/settings.h"
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
      timer_search_delay_(new QTimer(this)),
      enabled_(false),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      download_album_covers_(true),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_type_(SearchType::Artists),
      search_id_(0) {

  QObject::connect(auth_.get(), &NeteaseAuthenticator::AuthenticationFinished, this, &NeteaseService::AuthFinished);

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &NeteaseService::StartSearch);

  NeteaseService::ReloadSettings();
  auth_->LoadSession();
  Authenticate();

}

NeteaseService::~NeteaseService() { }

void NeteaseService::Exit() {

  Q_EMIT ExitFinished(); // Trigger this to indicate exit finished

}

bool NeteaseService::authenticated() const {

  return auth_->authenticated();

}

QList<QNetworkCookie> NeteaseService::cookies() const {

  return auth_->cookies();

}

void NeteaseService::ReloadSettings() {

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);

  enabled_ = s.value(NeteaseSettings::kEnabled, false).toBool();

  quint64 search_delay = std::max(s.value(NeteaseSettings::kSearchDelay, 1500).toULongLong(), 500ULL);
  artistssearchlimit_ = s.value(NeteaseSettings::kArtistsSearchLimit, 4).toInt();
  albumssearchlimit_ = s.value(NeteaseSettings::kAlbumsSearchLimit, 10).toInt();
  songssearchlimit_ = s.value(NeteaseSettings::kSongsSearchLimit, 10).toInt();
  fetchalbums_ = s.value(NeteaseSettings::kFetchAlbums, false).toBool();
  download_album_covers_ = s.value(NeteaseSettings::kDownloadAlbumCovers, true).toBool();
  // remove_remastered_ = s.value(NeteaseSettings::kRemoveRemastered, true).toBool();

  s.endGroup();

  timer_search_delay_->setInterval(static_cast<int>(search_delay));

};

void NeteaseService::Authenticate() {

  auth_->Authenticate();

}

void NeteaseService::ClearSession() {

  auth_->ClearSession();

}

void NeteaseService::AuthFinished(const bool success, const QString &error) {

  qLog(Debug) << success << error;

  if (success) {
    Q_EMIT LoginSuccess();
    // Q_EMIT UpdateSpotifyAccessToken(oauth_->access_token());
  }
  else {
    Q_EMIT LoginFailure(error);
  }

  Q_EMIT LoginFinished(success);

}

void NeteaseService::GetArtists() {

  if (!authenticated()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  artists_request_.reset(new NeteaseRequest(this, network_, NeteaseBaseRequest::Type::FavouriteArtists, this));
  QObject::connect(&*artists_request_, &NeteaseRequest::Results, this, &NeteaseService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::ArtistsProgressSetMaximumReceived);
  QObject::connect(&*artists_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

}

void NeteaseService::GetAlbums() {

  if (!authenticated()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  albums_request_.reset(new NeteaseRequest(this, network_, NeteaseBaseRequest::Type::FavouriteAlbums, this));
  QObject::connect(&*albums_request_, &NeteaseRequest::Results, this, &NeteaseService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::AlbumsProgressSetMaximumReceived);
  QObject::connect(&*albums_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

}

void NeteaseService::GetSongs() {

  if (!authenticated()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  songs_request_.reset(new NeteaseRequest(this, network_, NeteaseBaseRequest::Type::FavouriteSongs, this));
  QObject::connect(&*songs_request_, &NeteaseRequest::Results, this, &NeteaseService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::SongsProgressSetMaximumReceived);
  QObject::connect(&*songs_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::SongsUpdateProgressReceived);

  songs_request_->Process();

}

void NeteaseService::ResetArtistsRequest() {

  albums_request_.reset();

}

void NeteaseService::ResetAlbumsRequest() {

  albums_request_.reset();

}
void NeteaseService::ResetSongsRequest() {

  songs_request_.reset();

}

int NeteaseService::Search(const QString &text, const SearchType type) {

  pending_search_id_ = next_pending_search_id_;
  pending_search_text_ = text;
  pending_search_type_ = type;

  next_pending_search_id_++;

  if (text.isEmpty()) {
    timer_search_delay_->stop();
    return pending_search_id_;
  }
  timer_search_delay_->start();

  return pending_search_id_;

}

void NeteaseService::StartSearch() {

  if (!authenticated()) {
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void NeteaseService::CancelSearch() {}

void NeteaseService::SendSearch() {

  NeteaseBaseRequest::Type type = NeteaseBaseRequest::Type::None;

  switch (pending_search_type_) {
    case SearchType::Artists:
      type = NeteaseBaseRequest::Type::SearchArtists;
      break;
    case SearchType::Albums:
      type = NeteaseBaseRequest::Type::SearchAlbums;
      break;
    case SearchType::Songs:
      type = NeteaseBaseRequest::Type::SearchSongs;
      break;
    default:
      //Error("Invalid search type.");
      return;
  }

  search_request_.reset(new NeteaseRequest(this, network_, type, this));
  QObject::connect(&*search_request_, &NeteaseRequest::Results, this, &NeteaseService::SearchResultsReceived);
  QObject::connect(&*search_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::SearchProgressSetMaximum);
  QObject::connect(&*search_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::SearchUpdateProgress);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void NeteaseService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}

void NeteaseService::ExitReceived() {}

void NeteaseService::ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error) {
  Q_UNUSED(id);
  Q_EMIT ArtistsResults(songs, error);
  ResetArtistsRequest();
}

void NeteaseService::AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error) {
  Q_UNUSED(id);
  Q_EMIT AlbumsResults(songs, error);
  ResetAlbumsRequest();
}

void NeteaseService::SongsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT SongsResults(songs, error);
  ResetSongsRequest();

}

void NeteaseService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateStatus(text);
}

void NeteaseService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateStatus(text);
}

void NeteaseService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateStatus(text);
}

void NeteaseService::ArtistsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT ArtistsProgressSetMaximum(max);
}

void NeteaseService::AlbumsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT AlbumsProgressSetMaximum(max);
}

void NeteaseService::SongsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT SongsProgressSetMaximum(max);
}

void NeteaseService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateProgress(progress);
}

void NeteaseService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateProgress(progress);
}

void NeteaseService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateProgress(progress);
}
