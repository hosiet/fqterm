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

#ifndef FQTERM_HTTP_H
#define FQTERM_HTTP_H

#include <QObject>
#include <QHttp>
#include <QMutex>
#include <QMap>

namespace FQTerm {

class FQTermConfig;
class FQTermFileDialog;

class FQTermHttp: public QObject {
  Q_OBJECT;
 public:
  FQTermHttp(FQTermConfig *, QWidget*, const QString &poolDir);
  ~FQTermHttp();

  void getLink(const QString &, bool);

 signals:
  void done(QObject*);
  void message(const QString &);
  void percent(int);
  void headerReceived(FQTermHttp *, const QString &filename);
  void previewImage(const QString &filename, bool raiseViewer);

 public slots:
  void cancel();

 protected slots:
  void httpDone(bool);
  void httpRead(int, int);
  void httpResponse(const QHttpResponseHeader &);

 protected:
  QHttp http_;
  QString cacheFileName_;
  bool previewEmitted;
  bool isPreview_;
  bool isExisting_;
  int lastPercent_;

  QString poolDir_;

  static QMap<QString, int> downloadMap_;
  static QMutex mutex_;

  FQTermConfig *config_;
};

}  // namespace FQTerm

#endif  // FQTERM_HTTP_H
