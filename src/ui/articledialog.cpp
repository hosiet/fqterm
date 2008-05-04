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

#include "articledialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include "fqterm_trace.h"

/* 
 *  Constructs a articleDialog as a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
articleDialog::articleDialog(QWidget *parent, Qt::WFlags fl)
  : QDialog(parent, fl) {
  ui_.setupUi(this);
  connectSlots();
}

/*
 *  Destroys the object and frees any allocated resources
 */
articleDialog::~articleDialog() {
  // no need to delete child widgets, Qt does it all for us
}


void articleDialog::connectSlots() {
  FQ_VERIFY(connect(ui_.selectButton, SIGNAL(clicked()), this, SLOT(onSelect())));
  FQ_VERIFY(connect(ui_.copyButton, SIGNAL(clicked()), this, SLOT(onCopy())));
  FQ_VERIFY(connect(ui_.saveButton, SIGNAL(clicked()), this, SLOT(onSave())));
  FQ_VERIFY(connect(ui_.closeButton, SIGNAL(clicked()), this, SLOT(onClose())));
}

void articleDialog::onSelect() {
  ui_.textBrowser->selectAll();
}

void articleDialog::onCopy() {
  ui_.textBrowser->copy();
  // 	QString strText = textBrowser->selectedText();
  // 
  // 	QClipboard *clipboard = QApplication::clipboard();
  // 	
  // 	#if (QT_VERSION>=0x030100)
  // 		clipboard->setText(strText, QClipboard::Selection );
  // 		clipboard->setText(strText, QClipboard::Clipboard );
  // 	#else
  // 		clipboard->setText(strText);
  // 	#endif
}

void articleDialog::onSave() {
  QFileDialog fileDlg;
  QString filename = fileDlg.getSaveFileName();
  
  if (!filename.isNull()) {
    QFile f(filename);
    if ((f.open(QIODevice::WriteOnly))) {
      f.write(articleText_.toLocal8Bit());
      f.close();
    } else {
      QMessageBox mb("Access file error:", filename, QMessageBox::Warning, 0, 0, 0,
                     this, 0);
      mb.exec();
    }
  }
}

void articleDialog::onClose() {
  done(0);
}


#include "articledialog.moc"
