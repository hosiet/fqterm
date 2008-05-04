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

#ifndef FQTERM_SCHEMADIALOG_H
#define FQTERM_SCHEMADIALOG_H

#include "ui_schemadialog.h"

namespace FQTerm {

class schemaDialog: public QDialog {
  Q_OBJECT;
 public:
  schemaDialog(QWidget *parent = 0, Qt::WFlags fl = 0);
  ~schemaDialog();

  void setSchema(const QString &);
  QString getSchema();

 protected:
  QColor colors[16];
  QPushButton * colorButtons[16];
  QColor fade_;
  float alpha_;
  QString backgroundFileName_;
  int backgroundType_;
  QString title_;

  QStringList fileList_;

  QString currentSchemaFileName_;

  bool isModified_;
  int lastItemID_;
 private:
  Ui::schemaDialog ui_;
  QButtonGroup buttonGroup_;

 protected:
  void connectSlots();

  void loadList();
  void loadSchema(const QString &schemaFileName);
  void saveNumSchema(int n = -1);

  void updateView();
  void updateBgPreview();

  QImage &fadeColor(QImage &, float, const QColor &);

 protected slots:
  void buttonClicked();
  void nameChanged(int);
  void bgType(int);
  void imageType(int);
  void chooseImage();
  void fadeClicked();
  void alphaChanged(int);
  void saveSchema();
  void removeSchema();
  void onOK();
  void onCancel();
  void textChanged(const QString &);
};

}  // namespace FQTerm

#endif  // FQTERM_SCHEMADIALOG_H
