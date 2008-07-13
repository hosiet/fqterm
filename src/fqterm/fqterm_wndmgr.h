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

#ifndef FQTERM_WND_MGR_H
#define FQTERM_WND_MGR_H

#include <QString>
#include <QList>

//class QTab;
class QIcon;

namespace FQTerm {

class FQTermWindow;
class FQTermFrame;

class FQTermWndMgr: public QObject {
  Q_OBJECT;
 public:
  FQTermWndMgr(QObject *parent = 0, const char *name = 0);
  ~FQTermWndMgr();

  int addWindow(FQTermWindow *mw, const QString &qtab, QIcon *icon);

  void removeWindow(FQTermWindow *mw);

  void activateTheTab(FQTermWindow *mw);

  void activateTheWindow(const QString &qtab, int n);
  void blinkTheTab(FQTermWindow *mw, bool bVisible);

  int count();

  bool afterRemove();

  FQTermWindow *activeWindow();

  void activeNextPrev(bool);

  void refreshAllExcept(FQTermWindow *mw);

  const QList<FQTermWindow *>& termWindowList();

 protected:
  QList<QString> tabCaptions_;
  QList<QIcon *> icons_;
  QList<FQTermWindow *> termWindows_;

  FQTermFrame *termFrame_;

  int activeWindowIndex_;
  bool isAfterRemoved_;
};

}  // namespace FQTerm

#endif  // FQTERM_WND_MGR_H
