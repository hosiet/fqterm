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

#ifndef FQTERM_HTTP_H
#define FQTERM_HTTP_H

#include <QObject>
#include <QHttp>

namespace FQTerm {
class FQTermHttp: public QObject {
  Q_OBJECT;
 public:
  FQTermHttp(QWidget*, const QString &poolDir);
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

  QWidget *parent_;
};

}  // namespace FQTerm

#endif  // FQTERM_HTTP_H
