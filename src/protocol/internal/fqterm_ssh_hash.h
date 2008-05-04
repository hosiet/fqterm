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

#ifndef FQTERM_SSH_HASH_H
#define FQTERM_SSH_HASH_H

#include "fqterm_ssh_types.h"

namespace FQTerm {

class FQTermSSHHash {
protected:
  int d_digestLength;
  int digestLength()const {
    return d_digestLength;
  }

public:
  FQTermSSHHash() {
    d_digestLength = 0;
  }
  virtual ~FQTermSSHHash() {}  
  
  virtual void update(u_char *data, int len) = 0;
  virtual void final(u_char *data) = 0;
};

}  // namespace FQTerm

#endif //FQTERM_SSH_HASH_H
