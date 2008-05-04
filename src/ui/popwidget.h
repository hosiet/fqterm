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

#ifndef FQTERM_POP_WIDGET_H
#define FQTERM_POP_WIDGET_H

#include <QWidget>
//Added by qt3to4:
#include <QMouseEvent>
#include <QLabel>

class QTimer;
class QLabel;

namespace FQTerm {

class FQTermWindow;

class popWidget: public QWidget {
  Q_OBJECT;
 public:
  //	#if (QT_VERSION>=310)
  //	popWidget(FQTermWindow * win, QWidget *parent = 0, const char *name=0, WFlags f=WStyle_Splash);
  //	#else
  popWidget(FQTermWindow *win, QWidget *parent_ = 0, const char *name = 0,
            Qt::WFlags f = Qt::WindowStaysOnTopHint
            | Qt::X11BypassWindowManagerHint
            | Qt::Tool);
  //	#endif
  ~popWidget();

  void popup();
  void setText(const QString &);

 public slots:
  void showTimer();

 protected:
  QTimer *pTimer;
  int stateID; // -1 hide, 0 popup, 1 wait, 2 hiding
  QPoint position_;
  QRect desktopRectangle_;
  int stepID_;
  int intervalID_;
  QLabel *label_;
  FQTermWindow *termWindow_;

  void mousePressEvent(QMouseEvent*);
};

}  // namespace FQTerm

#endif  // FQTERM_POP_WIDGET_H
