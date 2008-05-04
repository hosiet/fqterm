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

#ifndef FQTERM_OSD_MESSAGE_H
#define FQTERM_OSD_MESSAGE_H

#include <QPixmap>
#include <QWidget>

namespace FQTerm {

/**
 * A widget that displays messages in the top-left corner.
 */
class PageViewMessage: public QWidget {
  Q_OBJECT;
 public:
  PageViewMessage(QWidget *parent_);

  enum Alignment {
    TopLeft, TopRight, BottomLeft, BottomRight
  };
  enum Icon {
    None, Info, Warning, Error, Find
  };
  void display(const QString &message, Icon icon = Info, int durationMs = 4000,
               QPoint pos = QPoint(10, 10), Alignment ali = TopLeft);
  QRect displayCheck(const QString &message, Icon icon = Info);
 public slots:
  void showText(const QString &);

 protected:
  void paintEvent(QPaintEvent *e);
  void mousePressEvent(QMouseEvent *e);
  QRect displayImpl(const QString &message, Icon icon = Info,
                    bool check = false, int durationMs = 4000,
                    QPoint pos = QPoint(10, 10), Alignment ali = TopLeft);

 private:
  QPixmap pixmap_;
  QTimer *timer_;
  QString message_;
};

}  // namespace FQTerm

#endif  // FQTERM_OSD_MESSAGE_H
