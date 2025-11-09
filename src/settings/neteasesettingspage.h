#ifndef NETEASESETTINGSPAGE_H
#define NETEASESETTINGSPAGE_H

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"
#include "settings/settingspage.h"

class QEvent;
class QShowEvent;
class NeteaseService;
class SettingsDialog;
class Ui_NeteaseSettingsPage;

class NeteaseSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit NeteaseSettingsPage(SettingsDialog *dialog, const SharedPtr<NeteaseService> service, QWidget *parent = nullptr);
  ~NeteaseSettingsPage() override;

  void Load() override;
  void Save() override;

 //  bool eventFilter(QObject *object, QEvent *event) override;
 //
 // protected:
 //  void showEvent(QShowEvent *e) override;

 Q_SIGNALS:
  void Authorize();

 // private Q_SLOTS:
 //  void LoginClicked();
 //  void LogoutClicked();
 //  void LoginSuccess();
 //  void LoginFailure(const QString &failure_reason);

 private:
  Ui_NeteaseSettingsPage *ui_;
  const SharedPtr<NeteaseService> service_;
};

#endif
