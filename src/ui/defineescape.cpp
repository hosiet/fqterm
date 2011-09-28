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

#include "defineescape.h"
#include "fqterm.h"

namespace FQTerm {

DefineEscapeDialog::~DefineEscapeDialog()
{
}

DefineEscapeDialog::DefineEscapeDialog(QString& strEsc, QWidget *parent_ /*= 0*/, Qt::WFlags fl /*= 0*/)
: strEsc_(strEsc), QDialog(parent_, fl)
{
  ui_.setupUi(this);
  ui_.edtEscape->setText(strEsc_);
  FQ_VERIFY(connect(ui_.btnOK, SIGNAL(clicked()), this, SLOT(onOK())));
  FQ_VERIFY(connect(ui_.btnCancel, SIGNAL(clicked()), this, SLOT(onCancel())));
}

void DefineEscapeDialog::onOK()
{
  strEsc_ = ui_.edtEscape->text();
  done(1);
}

void DefineEscapeDialog::onCancel()
{
  done(0);
}
} //namespace FQTerm

#include "defineescape.moc"