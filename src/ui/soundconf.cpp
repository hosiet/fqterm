/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.              *
 ***************************************************************************/

#include <QMessageBox>
#include <QFileDialog>

#include "fqterm.h"
#include "fqterm_config.h"
#include "fqterm_path.h"
#include "fqterm_sound.h"

#include "soundconf.h"

namespace FQTerm {
/*
 *  Constructs a fSoundConf which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */

soundConf::soundConf(FQTermConfig * config, QWidget *parent, Qt::WFlags fl)
    : QDialog(parent, fl),
    buttonGroup_(this),
    config_(config){
  ui_.setupUi(this);
  buttonGroup_.addButton(ui_.radioButton1, 0);
  buttonGroup_.addButton(ui_.radioButton2, 1);
  buttonGroup_.addButton(ui_.radioButton3, 2);
  buttonGroup_.addButton(ui_.radioButton4, 3);
  sound_ = NULL;
  loadSetting();
}

/*
 *  Destroys the object and frees any allocated resources
 */
soundConf::~soundConf() {
  // no need to delete child widgets, Qt does it all for us
  delete sound_;
}

/*
 * public slot
 */
void soundConf::onSelectFile() {
  QString soundfile = QFileDialog::getOpenFileName(
      this, "Choose a file", QDir::currentPath(), "*");
  if (!soundfile.isNull()) {
    ui_.leFile->setText(soundfile);
  }
}

/*
 * public slot
 */
void soundConf::onSelectProg() {
  QString progfile = QFileDialog::getOpenFileName(
      this, "Choose a program", QDir::currentPath(), "*");
  if (!progfile.isEmpty()) {
    ui_.leFile->setText(progfile);
  }
}

/*
 * public slot
 */
void soundConf::onPlayMethod(int id) {
#ifdef _NO_ARTS_COMPILED
  if (id == 1) {
    QMessageBox::critical(
        this, tr("No such output driver"),
        tr("ARTS is not supported by this instance of FQTerm,\n"
           "Check whether your ARTS support is enabled in compile time."),
        tr("&OK"));
    buttonGroup_.button(3)->setChecked(true);
    // 	QRadioButton * tmp = static_cast<QRadioButton *>(bgMethod->find(3));
    // 	tmp->setChecked(true);
  }
#endif
#ifdef _NO_ESD_COMPILED
  if (id == 2) {
    QMessageBox::critical(
        this, tr("No such output driver"),
        tr("ESD is not supported by this instance of FQTerm,\n"
           "Check whether your ESD support is enabled in compile time"),
        tr("&OK"));
    // 	QRadioButton * tmp = static_cast<QRadioButton *>(bgMethod->find(3));
    // 	tmp->setChecked(true);
    buttonGroup_.button(3)->setChecked(true);
  }
#endif
  if (id == 3 || buttonGroup_.checkedId() == 3) {
    ui_.leProg->setEnabled(true);
    ui_.bpSelect->setEnabled(true);
  } else if (id == 0 || id == 1 || id == 2) {
    ui_.leProg->setEnabled(false);
    ui_.bpSelect->setEnabled(false);
  }
}

void soundConf::onTestPlay() {
  if (ui_.leFile->text().isEmpty()) {
    QMessageBox::critical(
        this, tr("No sound file"),
        tr("You have to select a file to test the sound"), tr("&Ok"));
  }

  switch (buttonGroup_.checkedId()) {
    case 0:
      sound_ = new FQTermInternalSound(ui_.leFile->text());
      break;
    case 1:
      /*
        #ifndef _NO_ARTS_COMPILED
        m_pSound = new FQTermArtsSound(leFile->text());
        #endif
        break;
        case 2:
        #ifndef _NO_ESD_COMPILED
        m_pSound = new FQTermEsdSound(leFile->text());
        #endif*/
      break;
    case 3:
      if (ui_.leProg->text().isEmpty()) {
        QMessageBox::critical(
            this, tr("No player"),
            tr("You have to specify an external player"), tr("&Ok"));
      } else {
        sound_ = new FQTermExternalSound(ui_.leProg->text(), ui_.leFile->text());
      }
      break;
    default:
      sound_ = NULL;
  }
  if (sound_) {
    sound_->play();
  }
  delete sound_;
  sound_ = NULL;
}

void soundConf::loadSetting() {

  QString strTmp;

  strTmp = config_->getItemValue("preference", "wavefile");
  if (!strTmp.isEmpty()) {
    ui_.leFile->setText(strTmp);
  }

  strTmp = config_->getItemValue("preference", "playmethod");
  if (!strTmp.isEmpty()) {
    buttonGroup_.button(strTmp.toInt())->setChecked(true);
    if (strTmp.toInt() != 3) {
      ui_.leProg->setEnabled(false);
      ui_.bpSelect->setEnabled(false);
    } else {
      strTmp = config_->getItemValue("preference", "externalplayer");
      if (!strTmp.isEmpty()) {
        ui_.leProg->setText(strTmp);
      }
    }
  } else {
    ui_.leProg->setEnabled(false);
    ui_.bpSelect->setEnabled(false);
  }
}

void soundConf::saveSetting() {

  QString strTmp;

  config_->setItemValue("preference", "beep", "2");

  config_->setItemValue("preference", "wavefile", ui_.leFile->text());

  strTmp.setNum(buttonGroup_.checkedId());
  config_->setItemValue("preference", "playmethod", strTmp);

  if (strTmp == "3") {
    config_->setItemValue("preference", "externalplayer", ui_.leProg->text());
  }

  config_->save(getPath(USER_CONFIG) + "fqterm.cfg");
}

void soundConf::accept() {
  saveSetting();
  QDialog::accept();
}

}  // namespace FQTerm

#include "soundconf.moc"
