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

#ifndef FQTERM_SSH2_PACKET_H
#define FQTERM_SSH2_PACKET_H

#include "fqterm_ssh_packet.h"

namespace FQTerm {

class FQTermSSH2PacketSender: public FQTermSSHPacketSender {
 protected:
  virtual void makePacket();

 public:
  virtual void setEncryptionType(int cipherType);
};

class FQTermSSH2PacketReceiver: public FQTermSSHPacketReceiver {
 private:
  // greater than 0 if last time an incomplete ssh2 packet received.
  int last_expected_input_length_;
 public:
  FQTermSSH2PacketReceiver()
      : last_expected_input_length_(0) {
  }

  virtual void parseData(FQTermSSHBuffer *input);
  virtual void setEncryptionType(int cipherType);
};

}  // namespace FQTerm

#endif  // FQTERM_SSH2_PACKET
