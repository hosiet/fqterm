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

#ifndef FQTERM_AUTO_UPDATE_H
#define FQTERM_AUTO_UPDATE_H

#include <QObject>

class QHttp;
class QFile;
class QString;

namespace FQTerm {

class FQTermConfig;

class FQTermAutoUpdater : public QObject{

  Q_OBJECT;

public:
  FQTermAutoUpdater(QObject* parent, FQTermConfig* config, QString zmodemPoolDir);
  ~FQTermAutoUpdater();

  void checkUpdate();

protected slots:
  void checkRequestFinished(int requestId, bool error);
  void httpDone(bool err);

private:
  QString getNewestVersion();
  void promptUpdate();

  QString zmodemPoolDir_;
  FQTermConfig* config_;
  QHttp * updateChecker_;
  QFile * versionInfoFile_;
  int checkRequestId_;
  bool checkDone_;
};

} //namespace FQTerm

#endif
