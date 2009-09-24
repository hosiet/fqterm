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
#include <map>
#include <utility>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTabBar>
//class QTab;
class QIcon;
class QSize;
class FQTermConfig;
namespace FQTerm {

class FQTermWindow;
class FQTermFrame;

class FQTermWndMgr: public QMdiArea {
  Q_OBJECT;
 public:
  FQTermWndMgr(QWidget *parent = 0, const char *name = 0);
  ~FQTermWndMgr();

  QTabBar* tabBar() {return tabBar_;}
  
  FQTermWindow* newWindow(const FQTermParam &param, FQTermConfig* config, QIcon* icon, int index = -1);
  bool closeWindow(FQTermWindow *mw);
  bool closeAllWindow();



  FQTermWindow *activeWindow();
  FQTermWindow *nthWindow(int n);
  int count();





  //sub-window position & size

  bool getSubWindowMax() const { return subWindowMax_; }
  void setSubWindowMax(bool val) { subWindowMax_ = val; }
  QSize getSubWindowSize() const { return subWindowSize_; }
  void setSubWindowSize(QSize val) { subWindowSize_ = val; }

 public slots:
   void onSubWindowClosed(QObject* obj);
   //record subwindows' size changes
   void subWindowResized(FQTermWindow *);
   void activateTheWindow(int n);
   void activateNextWindow();
   void activatePrevWindow();
   void refreshAllExcept(FQTermWindow *mw);
   void blinkTheTab(FQTermWindow *mw, bool bVisible);
   void cascade();
   void tile();

 protected:
  QList<QIcon *> icons_;
  FQTermFrame *termFrame_;
  bool isAfterRemoved_;
  QSize subWindowSize_;
  bool subWindowMax_;
 QTabBar* tabBar_;

 typedef std::pair<QMdiSubWindow*, int> mdi_info_t;
 typedef std::map<FQTermWindow*, mdi_info_t> window_map_t;
 typedef std::map<FQTermWindow*, mdi_info_t>::iterator window_map_iterator_t;
 window_map_t windowMap_;
 FQTermWindow* MDIToFQ(QMdiSubWindow* subWindow)
 {
   for (window_map_iterator_t it = windowMap_.begin(); it != windowMap_.end(); ++it)
   {
     if (it->second.first == subWindow)
       return it->first;
   }
   return NULL;
 }
 QMdiSubWindow* FQToMDI(FQTermWindow* window)
 {
   window_map_iterator_t it = windowMap_.find(window);
   if (it != windowMap_.end())
     return it->second.first;
   return NULL;
 }
 int FQToIndex(FQTermWindow* window)
 {
   window_map_iterator_t it = windowMap_.find(window);
   if (it != windowMap_.end())
     return it->second.second;
   return -1;
 }
 bool afterRemove();
 protected slots:
   void onSubWindowActivated(QMdiSubWindow * subWindow);
};

}  // namespace FQTerm

#endif  // FQTERM_WND_MGR_H
