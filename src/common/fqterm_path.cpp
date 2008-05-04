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

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#endif // WIN32

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QFont>

#include "fqterm_config.h"
#include "fqterm_trace.h"
#include "fqterm_param.h"
#include "fqterm_path.h"
#include "fqterm_font.h"

namespace FQTerm {

static QString getUserDataDir();
static QString getInstallPrefix();
static QString getResourceDir(const QString &prefix);

const QString &getPath(PathCategory category) {
  static QString null_dir = "";
  static QString user_config = getUserDataDir();
  static QString prefix = getInstallPrefix();
  static QString resource = getResourceDir(prefix);
  
  switch (category) {
    case RESOURCE:
      return resource;
      break;
    case USER_CONFIG:
      return user_config;
      break;
  }
  return null_dir;
}

void clearDir(const QString &path) {
  QDir dir(path);
  if (dir.exists()) {
    const QFileInfoList list = dir.entryInfoList();
    //QFileInfoListIterator it( *list );
    //QFileInfo *fi;
    foreach(QFileInfo fi, list) {
      if (fi.isFile()) {
        dir.remove(fi.fileName());
      }
    }
  }
}

bool checkPath(const QString &path) {
  QDir dir(path);
  if (!dir.exists()) {
    if (!dir.mkdir(path)) {
      FQ_TRACE("path", 0) << "Failed to create directory " << path;
      return false;
    }
  }
  return true;
}

bool checkFile(const QString &src, const QString &dst) {
  if (QFile(dst).exists()) {
    return true;
  }

  if (!QFile::copy(src, dst)) {
    FQ_TRACE("path", 0) << "Failed to copy a file from " << src
                        << " to " << dst;
    return false;
  } 

  FQ_TRACE("path", 5) << "A file copied from " << src
                      << " to " << dst;

  if (!QFile::setPermissions(dst, QFile::ReadOwner | QFile::WriteOwner)) {
    FQ_TRACE("path", 0) << "Failed to change access attributs of "<< dst
                        << " to make only the owner have rights to "
                        << "read/write it.";
    return false;
  }

  return true;
}

bool iniSettings() {
  if (!checkPath(getPath(USER_CONFIG))) {
    return false;
  }

  if (!checkFile(getPath(RESOURCE) + "userconf/fqterm.cfg.orig", 
                 getPath(USER_CONFIG) + "fqterm.cfg")) {
    return false;
  }

  if (!checkFile(getPath(RESOURCE) + "userconf/address.cfg.orig", 
                 getPath(USER_CONFIG) + "address.cfg")) {
    return false;
  }

  if (!checkFile(getPath(RESOURCE) + "userconf/language.cfg.orig", 
                 getPath(USER_CONFIG) + "language.cfg")) {
    return false;
  }

  //read settings from fqterm.cfg
  FQTermConfig *conf = new FQTermConfig(getPath(USER_CONFIG) + "fqterm.cfg");

  //set font
  QString family = (conf->getItemValue("global", "font"));

  QString pointsize = conf->getItemValue("global", "pointsize");
  QString pixelsize = conf->getItemValue("global", "pixelsize");
  if (!family.isEmpty()) {
    QFont font(family);
    if (pointsize.toInt() > 0) {
      font.setPointSize(pointsize.toInt());
    }
    if (pixelsize.toInt() > 0) {
      font.setPixelSize(pixelsize.toInt());
    }
    QString openAntiAlias_ = conf->getItemValue("global", "antialias");
    if (openAntiAlias_ != "0") {
      font.setStyleStrategy(QFont::PreferAntialias);
    }
    qApp->setFont(font);
  }

  // zmodem and pool directory
  QString pathZmodem = conf->getItemValue("preference", "zmodem");
  if (pathZmodem.isEmpty()) {
    pathZmodem = getPath(USER_CONFIG) + "zmodem";
  }

  if (!checkPath(pathZmodem)) {
    return false;
  }

  QString pathPool = conf->getItemValue("preference", "pool");

  if (pathPool.isEmpty()) {
    pathPool = getPath(USER_CONFIG) + "pool/";
  }

  if (pathPool.right(1) != "/") {
    pathPool.append('/');
  }

  QString pathCache = pathPool + "shadow-cache/";

  if (!checkPath(pathPool) || !checkPath(pathCache)) {
    return false;
  }

  delete conf;
  return true;
}

void loadNameList(FQTermConfig *pConf, QStringList &listName) {
  QString strTmp = pConf->getItemValue("bbs list", "num");

  QString strSection;

  for (int i = 0; i < strTmp.toInt(); i++) {
    strSection.sprintf("bbs %d", i);
    listName.append(pConf->getItemValue(strSection, "name"));
  }
}

bool loadAddress(FQTermConfig *pConf, int n, FQTermParam &param) {
  QString strTmp, strSection;
  if (n < 0) {
    strSection = "default";
  } else {
    strSection.sprintf("bbs %d", n);
  }

  if (!pConf->hasSection(strSection))
	  return false;

  // check if larger than existence
  strTmp = pConf->getItemValue("bbs list", "num");
  if (n >= strTmp.toInt()) {
    return false;
  }
  param.name_ = pConf->getItemValue(strSection, "name");
  param.hostAddress_ = pConf->getItemValue(strSection, "addr");
  strTmp = pConf->getItemValue(strSection, "port");
  param.port_ = strTmp.toUShort();
  strTmp = pConf->getItemValue(strSection, "hosttype");
  param.hostType_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "autologin");
  param.isAutoLogin_ = (strTmp != "0");
  param.preLoginCommand_ = pConf->getItemValue(strSection, "prelogin");
  param.userName_ = pConf->getItemValue(strSection, "user");
  param.password_ = pConf->getItemValue(strSection, "password");
  param.postLoginCommand_ = pConf->getItemValue(strSection, "postlogin");

  strTmp = pConf->getItemValue(strSection, "bbscode");
  param.serverEncodingID_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "displaycode");
  param.dispEncodingID_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "autofont");
  param.isFontAutoFit_ = (strTmp != "0");
  strTmp = pConf->getItemValue(strSection, "alwayshighlight");
  param.isAlwaysHighlight_ = (strTmp != "0");
  strTmp = pConf->getItemValue(strSection, "ansicolor");
  param.isAnsiColor_ = (strTmp != "0");
  QString language;
  QString font_name;
  QString font_size;
  language = FQTermParam::getLanguageName(true); 
  font_name = pConf->getItemValue(strSection, language + "fontname");
  font_size = pConf->getItemValue(strSection, language + "fontsize");
  if (!font_name.isEmpty()) {
    param.englishFontName_ = font_name;
  }
  if (!font_size.isEmpty()) {
    param.englishFontSize_ = font_size.toInt();
  }
  language = FQTermParam::getLanguageName(false); 
  font_name = pConf->getItemValue(strSection, language + "fontname");
  font_size = pConf->getItemValue(strSection, language + "fontsize");
  if (!font_name.isEmpty()) {
    param.nonEnglishFontName_ = font_name;
  }
  if (!font_size.isEmpty()) {
    param.nonEnglishFontSize_ = font_size.toInt();
  }
  param.foregroundColor_.setNamedColor(pConf->getItemValue(strSection, "fgcolor"));
  param.backgroundColor_.setNamedColor(pConf->getItemValue(strSection, "bgcolor"));
  param.schemaFileName_ = pConf->getItemValue(strSection, "schemafile");

  param.virtualTermType_ = pConf->getItemValue(strSection, "termtype");
  strTmp = pConf->getItemValue(strSection, "keytype");
  param.keyboardType_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "column");
  param.numColumns_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "row");
  param.numRows_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "scroll");
  param.numScrollLines_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "cursor");
  param.cursorType_ = strTmp.toInt();
  param.escapeString_ = pConf->getItemValue(strSection, "escape");

  strTmp = pConf->getItemValue(strSection, "proxytype");
  param.proxyType_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "proxyauth");
  param.isAuthentation_ = (strTmp != "0");
  param.proxyHostName_ = pConf->getItemValue(strSection, "proxyaddr");
  strTmp = pConf->getItemValue(strSection, "proxyport");
  param.proxyPort_ = strTmp.toInt();
  param.proxyUserName_ = pConf->getItemValue(strSection, "proxyuser");
  param.proxyPassword_ = pConf->getItemValue(strSection, "proxypassword");
  strTmp = pConf->getItemValue(strSection, "protocol");
  param.protocolType_ = strTmp.toInt();
  param.sshUserName_ = pConf->getItemValue(strSection, "sshuser");
  param.sshPassword_ = pConf->getItemValue(strSection, "sshpassword");

  strTmp = pConf->getItemValue(strSection, "maxidle");
  param.maxIdleSeconds_ = strTmp.toInt();
  param.replyKeyCombination_ = pConf->getItemValue(strSection, "replykey");
  if (param.replyKeyCombination_.isNull()) {
    Q_ASSERT(false);
  }

  param.antiIdleMessage_ = pConf->getItemValue(strSection, "antiidlestring");
  param.autoReplyMessage_ = pConf->getItemValue(strSection, "autoreply");
  strTmp = pConf->getItemValue(strSection, "bautoreply");
  param.isAutoReply_ = (strTmp != "0");

  strTmp = pConf->getItemValue(strSection, "reconnect");
  param.isAutoReconnect_ = (strTmp != "0");
  strTmp = pConf->getItemValue(strSection, "interval");
  param.reconnectInterval_ = strTmp.toInt();
  strTmp = pConf->getItemValue(strSection, "retrytimes");
  param.retryTimes_ = strTmp.toInt();

  strTmp = pConf->getItemValue(strSection, "loadscript");
  param.isAutoLoadScript_ = (strTmp != "0");
  param.autoLoadedScriptFileName_ = pConf->getItemValue(strSection, "scriptfile");

  strTmp = pConf->getItemValue(strSection, "menutype");
  param.menuType_ = strTmp.toInt();
  param.menuColor_.setNamedColor(pConf->getItemValue(strSection, "menucolor"));

  param.isColorCopy_ = (pConf->getItemValue(strSection, "colorcopy") != "0");
  param.isAutoCopy_ = (pConf->getItemValue(strSection, "autocopy") != "0");
  param.isRectSelect_ = (pConf->getItemValue(strSection, "rectselect") != "0");

  return true;
}

void saveAddress(FQTermConfig *pConf, int n, const FQTermParam &param) {
  QString strTmp, strSection;
  if (n < 0) {
    strSection = "default";
  } else {
    strSection.sprintf("bbs %d", n);
  }

  pConf->setItemValue(strSection, "name", param.name_);
  pConf->setItemValue(strSection, "addr", param.hostAddress_);
  strTmp.setNum(param.port_);
  pConf->setItemValue(strSection, "port", strTmp);
  strTmp.setNum(param.hostType_);
  pConf->setItemValue(strSection, "hosttype", strTmp);
  pConf->setItemValue(strSection, "autologin", param.isAutoLogin_ ? "1" : "0");
  pConf->setItemValue(strSection, "prelogin", param.preLoginCommand_);
  pConf->setItemValue(strSection, "user", param.userName_);
  pConf->setItemValue(strSection, "password", param.password_);
  pConf->setItemValue(strSection, "postlogin", param.postLoginCommand_);

  strTmp.setNum(param.serverEncodingID_);
  pConf->setItemValue(strSection, "bbscode", strTmp);
  strTmp.setNum(param.dispEncodingID_);
  pConf->setItemValue(strSection, "displaycode", strTmp);
  pConf->setItemValue(strSection, "autofont", param.isFontAutoFit_? "1" : "0");
  pConf->setItemValue(strSection, "alwayshighlight", param.isAlwaysHighlight_ ?
                      "1" : "0");
  pConf->setItemValue(strSection, "ansicolor", param.isAnsiColor_ ? "1" : "0");

  pConf->setItemValue(strSection, FQTermParam::getLanguageName(true) + "fontname", param.englishFontName_);
  strTmp.setNum(param.englishFontSize_);
  pConf->setItemValue(strSection, FQTermParam::getLanguageName(true) + "fontsize", strTmp);

  pConf->setItemValue(strSection, FQTermParam::getLanguageName(false) + "fontname", param.nonEnglishFontName_);
  strTmp.setNum(param.nonEnglishFontSize_);
  pConf->setItemValue(strSection, FQTermParam::getLanguageName(false) + "fontsize", strTmp);

  pConf->setItemValue(strSection, "bgcolor", param.backgroundColor_.name());
  pConf->setItemValue(strSection, "fgcolor", param.foregroundColor_.name());

  pConf->setItemValue(strSection, "schemafile", param.schemaFileName_);

  pConf->setItemValue(strSection, "termtype", param.virtualTermType_);
  strTmp.setNum(param.keyboardType_);
  pConf->setItemValue(strSection, "keytype", strTmp);
  strTmp.setNum(param.numColumns_);
  pConf->setItemValue(strSection, "column", strTmp);
  strTmp.setNum(param.numRows_);
  pConf->setItemValue(strSection, "row", strTmp);
  strTmp.setNum(param.numScrollLines_);
  pConf->setItemValue(strSection, "scroll", strTmp);
  strTmp.setNum(param.cursorType_);
  pConf->setItemValue(strSection, "cursor", strTmp);
  pConf->setItemValue(strSection, "escape", param.escapeString_);

  strTmp.setNum(param.proxyType_);
  pConf->setItemValue(strSection, "proxytype", strTmp);
  pConf->setItemValue(strSection, "proxyauth", param.isAuthentation_ ? "1" : "0");
  pConf->setItemValue(strSection, "proxyaddr", param.proxyHostName_);
  strTmp.setNum(param.proxyPort_);
  pConf->setItemValue(strSection, "proxyport", strTmp);
  pConf->setItemValue(strSection, "proxyuser", param.proxyUserName_);
  pConf->setItemValue(strSection, "proxypassword", param.proxyPassword_);
  strTmp.setNum(param.protocolType_);
  pConf->setItemValue(strSection, "protocol", strTmp);
  pConf->setItemValue(strSection, "sshuser", param.sshUserName_);
  pConf->setItemValue(strSection, "sshpassword", param.sshPassword_);

  strTmp.setNum(param.maxIdleSeconds_);
  pConf->setItemValue(strSection, "maxidle", strTmp);
  pConf->setItemValue(strSection, "replykey", param.replyKeyCombination_);
  pConf->setItemValue(strSection, "antiidlestring", param.antiIdleMessage_);
  pConf->setItemValue(strSection, "bautoreply", param.isAutoReply_ ? "1" : "0");
  pConf->setItemValue(strSection, "autoreply", param.autoReplyMessage_
                      );
  pConf->setItemValue(strSection, "reconnect", param.isAutoReconnect_? "1" : "0");
  strTmp.setNum(param.reconnectInterval_);
  pConf->setItemValue(strSection, "interval", strTmp);
  strTmp.setNum(param.retryTimes_);
  pConf->setItemValue(strSection, "retrytimes", strTmp);

  pConf->setItemValue(strSection, "loadscript", param.isAutoLoadScript_ ? "1" : "0");
  pConf->setItemValue(strSection, "scriptfile", param.autoLoadedScriptFileName_);

  strTmp.setNum(param.menuType_);
  pConf->setItemValue(strSection, "menutype", strTmp);
  pConf->setItemValue(strSection, "menucolor", param.menuColor_.name());

  pConf->setItemValue(strSection, "colorcopy", param.isColorCopy_? "1" : "0");
  pConf->setItemValue(strSection, "autocopy", param.isAutoCopy_? "1" : "0");
  pConf->setItemValue(strSection, "rectselect", param.isRectSelect_? "1" : "0");

}

static QString getResourceDir(const QString &prefix) {
  QString res;

  std::string fqterm_resource;

  char *p = NULL;
  if ((p = getenv("FQTERM_RESOURCE")) != NULL)
      fqterm_resource = p;

  if (fqterm_resource.size() == 0) {
#if defined(WIN32)
    res = prefix;
#elif defined(__APPLE__)
    res = prefix + "../Resources/";
#else
    res = prefix + "share/FQTerm/";  
#endif
  } else {
    res = QString::fromLocal8Bit(fqterm_resource.c_str());    
  }

  if (res[res.size() - 1] != '/')
      res += '/';
  
  return res;
}

static QString getInstallPrefix() {
  QString res;
  
  std::string fqterm_prefix;

  char *p = NULL;
  if ((p = getenv("FQTERM_PREFIX")) != NULL)
      fqterm_prefix = p;

  if (fqterm_prefix.size() == 0) {
    res = QCoreApplication::applicationDirPath() + "/";
  } else {
    res = QString::fromLocal8Bit(fqterm_prefix.c_str());
  }
  
  if (res[res.size() - 1] != '/')
      res += '/';

  return res;
}

static QString getUserDataDir() {
#ifdef WIN32
  char buffer[MAX_PATH];
  SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, buffer);
  std::string dir(buffer);

  if (dir[dir.size() - 1] != '\\') {
    dir += '\\';
  }
  return QString::fromLocal8Bit(dir.c_str()) + "FQTerm/";
#else
  char *p = NULL;

  std::string home;
  if ((p = getenv("HOME")) != NULL)
      home = p;
  if (home.size() == 0)
      home = "/root/";    

  if (home[home.size() - 1] != '/')
      home += '/';

  return QString::fromLocal8Bit(home.c_str()) + ".fqterm/";
#endif
}

}  // namespace FQTerm
