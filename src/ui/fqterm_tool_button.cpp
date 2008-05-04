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

#include "fqterm_tool_button.h"
#include "fqterm_trace.h"

namespace FQTerm {

FQTermToolButton::FQTermToolButton(QWidget *parent_, int id, QString name)
    : QToolButton(parent_) {
  setObjectName(name);
  this->id_ = id;
  FQ_VERIFY(connect(this, SIGNAL(clicked()), this, SLOT(slotClicked())));
}

FQTermToolButton::~FQTermToolButton(){}

void FQTermToolButton::slotClicked() {
  emit(buttonClicked(id_));
}

}  // namespace FQTerm

#include "fqterm_tool_button.moc"
