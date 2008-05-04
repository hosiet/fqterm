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

#include <QString>
#include <QLineEdit>

#include "fqterm_trace.h"
#include "sshlogindialog.h"

namespace FQTerm {

/* 
 *  Constructs a fSSHLogin which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
SSHLoginDialog::SSHLoginDialog(QString *username, QString *password,
                               QWidget *parent_, Qt::WFlags fl)
  : QDialog(parent_, fl) {
  ui_.setupUi(this);
  strUserName = username;
  strPassword = password;
  ui_.lePassword->setEchoMode(QLineEdit::Password);
  ui_.leUserName->setText(*username);
  ui_.lePassword->setText(*password);
  ui_.leUserName->setFocus();

  FQ_VERIFY(connect(ui_.bOK, SIGNAL(clicked()), this, SLOT(accept())));
  FQ_VERIFY(connect(ui_.bCancel, SIGNAL(clicked()), this, SLOT(reject())));
}

/*  
 *  Destroys the object and frees any allocated resources
 */
SSHLoginDialog::~SSHLoginDialog() {
	// no need to delete child widgets, Qt does it all for us
}

void SSHLoginDialog::accept() {
	*strUserName = ui_.leUserName->text();
	*strPassword = ui_.lePassword->text();
	QDialog::accept();
}

}  // namespace FQTerm

#include "sshlogindialog.moc"
