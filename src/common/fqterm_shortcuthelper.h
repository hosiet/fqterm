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

#ifndef __FQTERM_SHORTCUTHELPER__
#define __FQTERM_SHORTCUTHELPER__

#include "fqterm.h"
#include <QObject>
#include <QString>
#include <QMap>
namespace FQTerm
{
class FQTermConfig;
  
class FQTermShortcutHelper : public QObject
{
  Q_OBJECT;
public:
  enum FQTERM_SHORTCUT
  {
    FQTERM_SHORTCUT_ZERO_GUARD = 0,
    CONNECT,
    DISCONNECT,
    ADDRESSBOOK,  //F2
    QUICKLOGIN,   //F3
    COPY,         //Ctrl+Ins under lin & win, Ctrl+C under mac
    PASTE,        //Shift+Insert under lin & win, Ctrl+V under mac
    COPYWITHCOLOR, 
    RECTANGLESELECTION,
    AUTOCOPYSELECTION,
    PASTEWORDWRAP,
    ENGLISHFONT,
    NONENGLISHFONT,
    COLORSETTING,
    ANSICOLOR,
    REFRESHSCREEN,  //F5
    UIFONT,
    FULLSCREEN,   //F6
    BOSSCOLOR,    //F12
    STATUSBAR,
    SWITCHBAR,
    GOOGLEIT,       //Ctrl+Alt+G
    EXTERNALEDITOR,
    CURRENTSETTING,
    DEFAULTSETTING,
    PREFERENCE,
    SHORTCUTSETTING,
    COPYARTICLE,  //F9
    ANTIIDLE, 
    AUTOREPLY,
    VIEWMESSAGE,  //F10
    BEEP,
    MOUSESUPPORT,
    IMAGEVIEWER,
    RUNSCRIPT,    //F7
    STOPSCRIPT,   //F8
    RUNPYTHONSCRIPT, //Ctrl+F1
    ABOUT,        //F1
    HOMEPAGE,
    FQTERM_SHORTCUT_MAX_GUARD
  };


public:
  FQTermShortcutHelper(FQTermConfig* config);
  ~FQTermShortcutHelper();
  
private:
  struct ShortcutDescriptionEntry
  {
    ShortcutDescriptionEntry(const QString& key = QString(""), const QString& defaultshortcuttext = QString(""), const QString& description = QString("")) : 
      key_(key),
      defaultshortcuttext_(defaultshortcuttext),
      description_(description)
    {}
    QString key_;
    QString defaultshortcuttext_;
    QString description_;
  };
  void initShortcutDescriptionTable();
  FQTermConfig* config_;
  QMap<int, ShortcutDescriptionEntry> shortcutDescriptionTable_;
  void initShortcutDescriptionTableEntry(int index, const QString& key, const QString& defaultshortcuttext, const QString& description) {
    shortcutDescriptionTable_[index] = ShortcutDescriptionEntry(key, defaultshortcuttext, description);
  }
  
  
public:

  QString getShortcutText(int shortcut) ;
  QString getShortcutDescription(int shortcut) {
    return shortcutDescriptionTable_[shortcut].description_;
  }
  QString getShortcutDefaultText(int shortcut) {
    return shortcutDescriptionTable_[shortcut].defaultshortcuttext_;
  }
  void setShortcutText(int shortcut, const QString& text);
  void resetShortcutText(int shortcut);
  void resetAllShortcutText();
  
private:
  //These functions are used to save shortcut
  QString getShortcutSection() {
    return QString("shortcutsettings");
  }
  QString getShortcutKey(int shortcut) {
    return shortcutDescriptionTable_[shortcut].key_;
  }
  
  QString getShortcutConfig(int shortcut);
  void setShortcutConfig(int shortcut, const QString& text);
};
  
  
}//namespace FQTerm


#endif 
