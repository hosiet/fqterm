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

#ifndef FQTERM_PATH_H
#define FQTERM_PATH_H

class QString;
class QStringList;

class FQTermConfig;
class FQTermParam;

namespace FQTerm {

enum PathCategory {RESOURCE, USER_CONFIG};

const QString &getPath(PathCategory category);

bool iniSettings();

void loadNameList(FQTermConfig *, QStringList &);
bool loadAddress(FQTermConfig *, int, FQTermParam &);
void saveAddress(FQTermConfig *, int, const FQTermParam &);

void clearDir(const QString &path);
}  // namespace FQTerm

#endif  // FQTERM_PATH_H
