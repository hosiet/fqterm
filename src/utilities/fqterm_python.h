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

#ifndef FQTERM_PYTHON_H
#define FQTERM_PYTHON_H

#include "fqterm.h"

#ifdef HAVE_PYTHON
#include <Python.h>

namespace FQTerm {

class FQTermWindow;
extern QString getException();
extern QString getErrOutputFile(FQTermWindow*);
extern PyMethodDef fqterm_methods[];
QByteArray stripWhitespace(const QByteArray &cstr);

}  // namespace FQTerm

#endif  // HAVE_PYTHON

#endif  // FQTERM_PYTHON_H

