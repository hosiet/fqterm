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

#ifndef FQTERM_SOUND_CONF_H
#define FQTERM_SOUND_CONF_H

#include "ui_soundconf.h"

namespace FQTerm {

class FQTermSound;
class FQTermConfig;

class soundConf: public QDialog {
  Q_OBJECT;
 public:
  soundConf(FQTermConfig *, QWidget *parent_ = 0, Qt::WFlags fl = 0);
  ~soundConf();
  void loadSetting();
  void saveSetting();

 public slots:
  void onSelectFile();
  void onSelectProg();
  void onPlayMethod(int id);
  void onTestPlay();
 protected slots:
  void accept();

 private:
  FQTermSound *sound_;
  Ui::soundConf ui_;
  QButtonGroup buttonGroup_;
  FQTermConfig * config_;
};

}  // namespace FQTerm

#endif  // FQTERM_SOUND_CONF_H
