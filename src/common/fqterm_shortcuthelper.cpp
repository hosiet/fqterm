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
#include "fqterm_shortcuthelper.h"
#include "fqterm_config.h"

namespace FQTerm
{

FQTermShortcutHelper::FQTermShortcutHelper(FQTermConfig* config) :
  config_(config)
{
  initShortcutDescriptionTable();
}

QString FQTermShortcutHelper::getShortcutText(int shortcut)
{
  if (!config_)
    return "";
  QString result = getShortcutConfig(shortcut);
  if (result == "")
    result = getShortcutDefaultText(shortcut);
  else if (result == "Undefined")
    result = "";
  return result;
}

void FQTermShortcutHelper::initShortcutDescriptionTable()
{
  initShortcutDescriptionTableEntry(CONNECT, "connect", tr(""), tr("Connect Host"));
  initShortcutDescriptionTableEntry(DISCONNECT, "disconnect", tr(""), tr("Disconnect Host"));
  initShortcutDescriptionTableEntry(ADDRESSBOOK, "addressbook", tr("F2"), tr("Address Book"));
  initShortcutDescriptionTableEntry(QUICKLOGIN, "quicklogin", tr("F3"), tr("Quick Login"));
#if defined(__APPLE__)
  initShortcutDescriptionTableEntry(COPY, "copy", tr("Ctrl+C"), tr("Copy"));
  initShortcutDescriptionTableEntry(PASTE, "paste", tr("Ctrl+V"), tr("Paste"));
#else
  initShortcutDescriptionTableEntry(COPY, "copy", tr("Ctrl+Ins"), tr("Copy"));
  initShortcutDescriptionTableEntry(PASTE, "paste", tr("Shift+Insert"), tr("Paste"));
#endif 
  initShortcutDescriptionTableEntry(COPYWITHCOLOR, "copywithcolor", tr(""), tr("Copy With Color"));  
  initShortcutDescriptionTableEntry(RECTANGLESELECTION, "rectangleselection", tr(""), tr("Rectangle Selection"));
  initShortcutDescriptionTableEntry(AUTOCOPYSELECTION, "autocopyselection", tr(""), tr("Auto Reply"));
  initShortcutDescriptionTableEntry(PASTEWORDWRAP, "pastewordwrap", tr(""), tr("Paste With Word Wrap"));  
  initShortcutDescriptionTableEntry(ENGLISHFONT, "englishfont", tr(""), tr("Set English Font"));
  initShortcutDescriptionTableEntry(NONENGLISHFONT, "nonenglishfont", tr(""), tr("Set non-English Font"));
  initShortcutDescriptionTableEntry(COLORSETTING, "colorsetting", tr(""), tr("Color Setting"));
  initShortcutDescriptionTableEntry(REFRESHSCREEN, "refreshscreen", tr(""), tr("Refresh Screen"));
  initShortcutDescriptionTableEntry(ANSICOLOR, "ansicolor", tr(""), tr("Toggle Ansi Color"));  
  initShortcutDescriptionTableEntry(UIFONT, "uifont", tr(""), tr("Set UI Font"));  
  initShortcutDescriptionTableEntry(FULLSCREEN, "fullscreen", tr("F6"), tr("Toggle Full Screen"));  
  initShortcutDescriptionTableEntry(BOSSCOLOR, "bosscolor", tr("F12"), tr("Toggle Boss Color"));  
  initShortcutDescriptionTableEntry(STATUSBAR, "statusbar", tr(""), tr("Toggle Status Bar"));  
  initShortcutDescriptionTableEntry(SWITCHBAR, "switchbar", tr(""), tr("Toggle Switch Bar"));  
  initShortcutDescriptionTableEntry(CURRENTSETTING, "currentsetting", tr(""), tr("Current Session Setting"));  
  initShortcutDescriptionTableEntry(GOOGLEIT, "googleit", tr("Ctrl+Alt+G"), tr("Google selected words"));
  initShortcutDescriptionTableEntry(EXTERNALEDITOR, "externaleditor", tr("Ctrl+Alt+E"), tr("Invoke external editor"));
  initShortcutDescriptionTableEntry(DEFAULTSETTING, "defaultsetting", tr(""), tr("Default Setting"));  
  initShortcutDescriptionTableEntry(PREFERENCE, "preference", tr(""), tr("Preference"));  
  initShortcutDescriptionTableEntry(SHORTCUTSETTING, "shortcut", tr(""), tr("Shorcut Setting"));
  initShortcutDescriptionTableEntry(COPYARTICLE, "copyarticle", tr("F9"), tr("Copy Article"));  
  initShortcutDescriptionTableEntry(ANTIIDLE, "antiidle", tr(""), tr("Toggle Anti Idle"));  
  initShortcutDescriptionTableEntry(AUTOREPLY, "autoreply", tr(""), tr("Toggle Auto Reply"));  
  initShortcutDescriptionTableEntry(VIEWMESSAGE, "viewmessage", tr("F10"), tr("View Messages"));  
  initShortcutDescriptionTableEntry(BEEP, "beep", tr(""), tr("Toggle Beep"));  
  initShortcutDescriptionTableEntry(MOUSESUPPORT, "mousesupport", tr(""), tr("Toggle Mouse Support"));  
  initShortcutDescriptionTableEntry(IMAGEVIEWER, "imageviewer", tr(""), tr("Image Viewer"));  
  initShortcutDescriptionTableEntry(RUNSCRIPT, "runscript", tr("F7"), tr("Run Script"));  
  initShortcutDescriptionTableEntry(STOPSCRIPT, "stop", tr("F8"), tr("Stop Script"));  
  initShortcutDescriptionTableEntry(RUNPYTHONSCRIPT, "runpythonscript", tr("Ctrl+F1"), tr("Run Python Script")); 
  initShortcutDescriptionTableEntry(ABOUT, "about", tr("F1"), tr("About"));  
  initShortcutDescriptionTableEntry(HOMEPAGE, "homepage", tr(""), tr("Homepage"));  
  //index, key, default shortcut, descritption
}

void FQTermShortcutHelper::resetAllShortcutText()
{
  for (int i = FQTERM_SHORTCUT_ZERO_GUARD + 1; i < FQTERM_SHORTCUT_MAX_GUARD; ++i)
  {
    resetShortcutText(i);
  }
}

void FQTermShortcutHelper::resetShortcutText(int shortcut)
{
  if (!config_)
    return;
  setShortcutText(shortcut, getShortcutDefaultText(shortcut));
}

void FQTermShortcutHelper::setShortcutText(int shortcut, const QString& text)
{
  if (!config_)
    return;
  QString val = text.trimmed();
  if (val == "")
    val = "Undefined";
  setShortcutConfig(shortcut, val);
}

FQTermShortcutHelper::~FQTermShortcutHelper()
{
}

QString FQTermShortcutHelper::getShortcutConfig(int shortcut)
{
  return config_->getItemValue(getShortcutSection(), getShortcutKey(shortcut));
}

void FQTermShortcutHelper::setShortcutConfig(int shortcut, const QString& text)
{
  config_->setItemValue(getShortcutSection(), getShortcutKey(shortcut), text);
}

}//namespace FQTerm

#include "fqterm_shortcuthelper.moc"