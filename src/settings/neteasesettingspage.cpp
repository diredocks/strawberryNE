#include <QObject>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QSettings>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QMessageBox>
#include <QEvent>

#include "settingsdialog.h"
#include "neteasesettingspage.h"
#include "ui_neteasesettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "netease/neteaseservice.h"
#include "constants/neteasesettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace NeteaseSettings;

NeteaseSettingsPage::NeteaseSettingsPage(SettingsDialog *dialog, const SharedPtr<NeteaseService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::NeteaseSettingsPage),
      service_(service) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"netease"_s));

  // dialog->installEventFilter(this);

}

NeteaseSettingsPage::~NeteaseSettingsPage() { delete ui_; }

void NeteaseSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, false).toBool());

  ui_->searchdelay->setValue(s.value(kSearchDelay, 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value(kArtistsSearchLimit, 4).toInt());
  ui_->albumssearchlimit->setValue(s.value(kAlbumsSearchLimit, 10).toInt());
  ui_->songssearchlimit->setValue(s.value(kSongsSearchLimit, 10).toInt());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, true).toBool());

  s.endGroup();

  Init(ui_->layout_neteasesettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void NeteaseSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enable->isChecked());
  s.setValue(kSearchDelay, ui_->searchdelay->value());
  s.setValue(kArtistsSearchLimit, ui_->artistssearchlimit->value());
  s.setValue(kAlbumsSearchLimit, ui_->albumssearchlimit->value());
  s.setValue(kSongsSearchLimit, ui_->songssearchlimit->value());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
  s.endGroup();

}
