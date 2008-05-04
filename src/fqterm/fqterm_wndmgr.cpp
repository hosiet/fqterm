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

#include <stdio.h>

#include <QApplication>
#include <QIcon>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTabBar>

#include "fqterm_frame.h"
#include "fqterm_path.h"
#include "fqterm_screen.h"
#include "fqterm_trace.h"
#include "fqterm_window.h"
#include "fqterm_wndmgr.h"

namespace FQTerm {

//constructor
FQTermWndMgr::FQTermWndMgr(QObject *parent, const char *name)
    : QObject(parent) {
  setObjectName(name);
  termFrame_ = (FQTermFrame*)parent;
  activeWindowIndex_ = -1;
  isAfterRemoved_ = false;
}

//destructor
FQTermWndMgr::~FQTermWndMgr(){}

//add window-tab-iconset
int FQTermWndMgr::addWindow(FQTermWindow *mw, const QString &qtab, QIcon *icon) {
  FQ_FUNC_TRACE("wndmgr", 3);

  Q_ASSERT(mw != NULL);
  termWindows_.append(mw);
  Q_ASSERT(qtab != QString::QString());
  tabCaptions_.append(qtab);
  icons_.append(icon);

  if (termWindows_.count() == 1) {
    termFrame_->enableMenuToolBar(true);
  }

  return tabCaptions_.count();
}

//remove window-tab-iconset
void FQTermWndMgr::removeWindow(FQTermWindow *mw) {
  FQ_FUNC_TRACE("wndmgr", 3);
  //find where it is
  int n = termWindows_.indexOf(mw);
  if (n == -1) {
    return;
  }

  //remove them from list
  tabCaptions_.removeAt(n);
  termWindows_.removeAt(n);
  icons_.removeAt(n);

  if (termWindows_.count() == 0) {
    activeWindowIndex_ = -1;
    termFrame_->enableMenuToolBar(false);
  }

  isAfterRemoved_ = true;
  //remove from the Tabbar
  termFrame_->tabBar_->removeTab(n);
  termFrame_->mdiArea_->subWindowList().at(n)->close();
}

//avtive the tab when switch the window
void FQTermWndMgr::activateTheTab(FQTermWindow *mw) {
  FQ_FUNC_TRACE("wndmgr", 3);
  //find where it is
  int n = termWindows_.indexOf(mw);

  if (n == activeWindowIndex_) {
    return ;
  }

  activeWindowIndex_ = n;

  // QString qtab = tabCaptions_.at(n);
  //set it seleted

  termFrame_->tabBar_->setCurrentIndex(n);

  termFrame_->updateMenuToolBar();
}

//active the window when switch the tab
void FQTermWndMgr::activateTheWindow(const QString &qtab, int n) {
  FQ_FUNC_TRACE("wndmgr", 3);

  if (n == activeWindowIndex_) {
    return ;
  }

  activeWindowIndex_ = n;

  QMdiSubWindow *subWindow = termFrame_->mdiArea_->subWindowList().at(n);

  // Fix the refresh bug by send PaintEvent to session screen.
  termFrame_->mdiArea_->setActiveSubWindow(subWindow);
  subWindow->setFocus();

  termFrame_->updateMenuToolBar();
}

//blink the tab when message come in
void FQTermWndMgr::blinkTheTab(FQTermWindow *mw, bool bVisible) {
  FQ_FUNC_TRACE("wndmgr", 10);
  static QIcon a_icon(getPath(RESOURCE) + "pic/tabpad.png");
  static QIcon b_icon(getPath(RESOURCE) + "pic/transp.png");

  //find where it is
  int n = termWindows_.indexOf(mw);
  QIcon *icon = icons_.at(n);
  //FIXME: QIcon::Automatic
  if (bVisible) {
    icon->addFile(getPath(RESOURCE) + "pic/tabpad.png");

    termFrame_->tabBar_->setTabIcon(n, a_icon);
  } else {
    //,QIcon::Automatic);
    icon->addFile(getPath(RESOURCE) + "pic/transp.png");
    termFrame_->tabBar_->setTabIcon(n, b_icon);
  }
  //,QIcon::Automatic);


  //termFrame_->tabBar->setTabIcon(n, *icon);

  termFrame_->tabBar_->update();
}

//return the number of connected window
int FQTermWndMgr::count() {
  return termWindows_.count();
}

FQTermWindow *FQTermWndMgr::activeWindow() {
  if (activeWindowIndex_ == -1) {
    return NULL;
  } else {
    return termWindows_.at(activeWindowIndex_);
  }
}

void FQTermWndMgr::activeNextPrev(bool next) {
  int n = activeWindowIndex_;

  if (n == -1) {
    return ;
  }
  if (next) {
    n = (n == termWindows_.count() - 1) ? 0 : n + 1;
  } else {
    n = (n == 0) ? termWindows_.count() - 1: n - 1;
  }

  activeWindowIndex_ = n;

  FQTermWindow *mw = termWindows_.at(n);
  ((QWidget*)termFrame_->mdiArea_)->setFocus();
  mw->showNormal();

  // set it seleted
  termFrame_->tabBar_->setCurrentIndex(n);
  termFrame_->updateMenuToolBar();
}

bool FQTermWndMgr::afterRemove() {
  if (isAfterRemoved_) {
    isAfterRemoved_ = false;
    return true;
  } else {
    return false;
  }
}

void FQTermWndMgr::refreshAllExcept(FQTermWindow *termWindow) {
  for (QList<FQTermWindow *>::iterator it = termWindows_.begin();
       it != termWindows_.end(); ++it ) {
    if (*it != termWindow) {
      (*it)->repaintScreen();
    }
  }
}

const QList<FQTermWindow *>& FQTermWndMgr::termWindowList() {
  return termWindows_;
}

}  // namespace FQTerm

#include "fqterm_wndmgr.moc"
