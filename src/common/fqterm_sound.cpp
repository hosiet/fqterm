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

#include <QSound>
#include <QFile>
#include <QMessageBox>
#include <stdlib.h>

#include "common.h"
#include "fqterm_sound.h"

/*
#ifndef _NO_ESD_COMPILED
#include <esd.h>
#endif

#ifndef _NO_ARTS_COMPILED
#include <soundserver.h>
using namespace Arts;
#endif
*/

namespace FQTerm {

FQTermSound::~FQTermSound(){}

void FQTermInternalSound::play() {
  if (QFile::exists(_soundfile)) {
    QSound::play(_soundfile);
  }
}

/*
  #ifndef _NO_ARTS_COMPILED
  void
  FQTermArtsSound::play()
  {
  Dispatcher dispatcher;
  SimpleSoundServer server;
  server = Arts::Reference("global:Arts_SimpleSoundServer");

  if (server.isNull()){
  FQ_TRACE("sound", 0) << "Can't connect to the sound server, "
  << "check if you do have a Arts system installed.";
  return;
  }

  if(QFile::exists(_soundfile))
  server.play(_soundfile.ascii());
  }
  #endif

  #ifndef _NO_ESD_COMPILED
  void
  FQTermEsdSound::play()
  {
  int fd = esd_open_sound(NULL);
  if (fd >= 0 && QFile::exists(_soundfile) ) {
  esd_play_file(NULL, _soundfile.ascii(), 0);
  esd_close(fd);
  }else
  FQ_TRACE("sound", 0) << "Can't open Esd driver, "
  << "Check if you do have a Esd system installed.";
  }
  #endif
*/
void FQTermExternalSound::setPlayer(const QString &playername) {
  playerName_ = playername;
}

void FQTermExternalSound::play() {
  if (QFile::exists(_soundfile)) {
    QString command = playerName_ + ' ' + _soundfile;
    runProgram(command);
  }
}

}  // namespace FQTerm
