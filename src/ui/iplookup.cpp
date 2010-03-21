/***************************************************************************
*   fqterm, a terminal emulator for both BBS and *nix.                    *
*   Copyright (C) 2008 fqterm development group.                          *
*                                                                         *
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
*   51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.               *
***************************************************************************/

#include "iplookup.h"
#include "fqterm_ip_location.h"
#include "fqterm.h"
namespace FQTerm {



IPLookupDialog::~IPLookupDialog(){

}

IPLookupDialog::IPLookupDialog( QWidget *parent_ /*= 0*/, Qt::WFlags fl /*= 0*/ )  : QDialog(parent_, fl) {
  ui_.setupUi(this);
  setWindowTitle(tr("IP Lookup"));
  FQ_VERIFY(connect(ui_.lookupPushButton, SIGNAL(clicked()), this, SLOT(onLookup())));
  FQ_VERIFY(connect(ui_.finishedPushButton, SIGNAL(clicked()), this, SLOT(onFinished())));
}

void IPLookupDialog::onLookup() {
  QString country, city;
  QString ip = ui_.ipLineEdit->text();
  bool res = FQTermIPLocation::getInstance()->getLocation(ip, country, city);
  if (res) {
    ui_.addressLineEdit->setText(encoding2unicode((country + city).toLatin1(), FQTERM_ENCODING_GBK));
  } else {
    ui_.addressLineEdit->setText(tr("Invalid"));
  }
}

void IPLookupDialog::onFinished() {
  done(0);
}
} //namespace FQTerm

#include "iplookup.moc"