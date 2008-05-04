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

#ifndef FQTERM_KEY_DIALOG_H
#define FQTERM_KEY_DIALOG_H

#include "ui_keydialog.h"

namespace FQTerm {

class FQTermConfig;

class keyDialog: public QDialog {
  Q_OBJECT;
 public:
  keyDialog(FQTermConfig *, QWidget *parent_ = 0, Qt::WFlags fl = 0);
  ~keyDialog();

 protected:
  void connectSlots();
  void loadKey(int);
  void loadName();

 protected slots:
  void onNamechange(int);
  void onClose();
  void onAdd();
  void onDelete();
  void onUpdate();
  void onChooseScript();
  void onProgram();
  void onUp();
  void onDown();
  void onLeft();
  void onRight();
  void onEnter();
  // 	void onSelect(int);
 private:
  Ui::keyDialog ui_;
  QButtonGroup keyButtonGroup_;
  FQTermConfig * config_;
};

}  // namespace FQTerm

#endif  // FQTERM_KEY_DIALOG_H
