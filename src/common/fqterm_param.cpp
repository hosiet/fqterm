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

#include "fqterm_param.h"
#include "fqterm_font.h"

namespace FQTerm {

FQTermParam::FQTermParam() {
  name_ = "NEW SMTH";
  hostAddress_ = "newsmth.net";
  port_ = 23;
  hostType_ = 0; // 0--BBS 1--*NIX
  isAutoLogin_ = false;
  preLoginCommand_ = "";
  userName_ = "";
  password_ = "";
  postLoginCommand_ = "";
  // Display
  serverEncodingID_ = 0;
  dispEncodingID_ = 0;
  isFontAutoFit_ = 1;
  isAlwaysHighlight_ = 0;
  isAnsiColor_ = 1;

  englishFontName_ = getDefaultFontFamilyForLanguage(true);
  englishFontSize_ = 14;
  nonEnglishFontName_ = getDefaultFontFamilyForLanguage(false);
  nonEnglishFontSize_ = 14;

  backgroundColor_ = QColor(0, 0, 0);
  foregroundColor_ = QColor(198, 195, 198);
  schemaFileName_ = "";
  // Terminal
  virtualTermType_ = "vt102";
  keyboardType_ = 0;
  numColumns_ = 80;
  numRows_ = 24;
  numScrollLines_ = 240;
  cursorType_ = 1; // 0--Block 1--Underline 2--I Type
  escapeString_ = "^[^[[";
  // Connection
  proxyType_ = 0; // 0--None 1--Wingate 2--SOCKS4 3--SOCKS5 4--HTTP
  proxyHostName_ = "";
  proxyPort_ = 0;
  isAuthentation_ = false;
  proxyUserName_ = "";
  proxyPassword_ = "";
  protocolType_ = 0; // 0--Telnet 1--SSH1 2--SSH2
  sshUserName_ = "";
  sshPassword_ = "";
  // Misc
  maxIdleSeconds_ = 120;
  replyKeyCombination_ = "^Z";
  antiIdleMessage_ = "^@";
  isAutoReply_ = false;
  autoReplyMessage_ = "(FQTerm) Sorry, I am not around";
  isAutoReconnect_ = false;
  reconnectInterval_ = 3;
  retryTimes_ = 0;
  isAutoLoadScript_ = false;
  autoLoadedScriptFileName_ = "";
  // Mouse
  menuType_ = 2;
  menuColor_ = QColor(0, 65, 132);

  isColorCopy_ = false;
  isRectSelect_ = false;
  isAutoCopy_ = true;
}

FQTermParam::FQTermParam(const FQTermParam &param) {
  copy(param);
}

FQTermParam::~FQTermParam(){}

FQTermParam &FQTermParam::operator = (const FQTermParam &param) {
  copy(param);
  return  *this;
}

void FQTermParam::copy(const FQTermParam& param) {
  name_ = param.name_;
  hostAddress_ = param.hostAddress_;
  port_ = param.port_;
  hostType_ = param.hostType_; // 0--BBS 1--*NIX
  isAutoLogin_ = param.isAutoLogin_;
  preLoginCommand_ = param.preLoginCommand_;
  userName_ = param.userName_;
  password_ = param.password_;
  postLoginCommand_ = param.postLoginCommand_;
  // Display
  serverEncodingID_ = param.serverEncodingID_;
  dispEncodingID_ = param.dispEncodingID_;
  isFontAutoFit_ = param.isFontAutoFit_;
  isAlwaysHighlight_ = param.isAlwaysHighlight_;
  isAnsiColor_ = param.isAnsiColor_;
  englishFontName_ = param.englishFontName_;
  englishFontSize_ = param.englishFontSize_;
  nonEnglishFontName_ = param.nonEnglishFontName_;
  nonEnglishFontSize_ = param.nonEnglishFontSize_;
  backgroundColor_ = param.backgroundColor_;
  foregroundColor_ = param.foregroundColor_;
  schemaFileName_ = param.schemaFileName_;
  // Terminal
  virtualTermType_ = param.virtualTermType_;
  keyboardType_ = param.keyboardType_;
  numColumns_ = param.numColumns_;
  numRows_ = param.numRows_;
  numScrollLines_ = param.numScrollLines_;
  cursorType_ = param.cursorType_; // 0--Block 1--Underline 2--I Type
  escapeString_ = param.escapeString_; // 0--ESC ESC 1--Ctrl+u
  // Connection
  proxyType_ = param.proxyType_;
  // 0--None 1--Wingate 2--SOCKS4 3--SOCKS5 4--HTTP
  proxyHostName_ = param.proxyHostName_;
  proxyPort_ = param.proxyPort_;
  isAuthentation_ = param.isAuthentation_;
  proxyUserName_ = param.proxyUserName_;
  proxyPassword_ = param.proxyPassword_;
  protocolType_ = param.protocolType_; // 0--Telnet 1--SSH1 2--SSH2
  sshUserName_ = param.sshUserName_;
  sshPassword_ = param.sshPassword_;
  // Misc
  maxIdleSeconds_ = param.maxIdleSeconds_;
  replyKeyCombination_ = param.replyKeyCombination_;
  antiIdleMessage_ = param.antiIdleMessage_;
  isAutoReply_ = param.isAutoReply_;
  autoReplyMessage_ = param.autoReplyMessage_;
  isAutoReconnect_ = param.isAutoReconnect_;
  reconnectInterval_ = param.reconnectInterval_;
  retryTimes_ = param.retryTimes_;
  isAutoLoadScript_ = param.isAutoLoadScript_;
  autoLoadedScriptFileName_ = param.autoLoadedScriptFileName_;
  // Mouse
  menuType_ = param.menuType_;
  menuColor_ = param.menuColor_;

  isColorCopy_ = param.isColorCopy_;
  isRectSelect_ = param.isRectSelect_;
  isAutoCopy_ = param.isAutoCopy_;
}

bool FQTermParam::operator==(const FQTermParam& param)
{
  if (name_ != param.name_) return false;
  if (hostAddress_ != param.hostAddress_) return false;
  if (port_ != param.port_) return false;
  if (hostType_ != param.hostType_) return false;
  if (isAutoLogin_ != param.isAutoLogin_) return false;
  if (preLoginCommand_ != param.preLoginCommand_) return false;
  if (userName_ != param.userName_) return false;
  if (password_ != param.password_) return false;
  if (postLoginCommand_ != param.postLoginCommand_) return false;
  if (serverEncodingID_ != param.serverEncodingID_) return false;
  if (dispEncodingID_ != param.dispEncodingID_) return false;
  if (isFontAutoFit_ != param.isFontAutoFit_) return false;
  if (isAlwaysHighlight_ != param.isAlwaysHighlight_) return false;
  if (isAnsiColor_ != param.isAnsiColor_) return false;
  if (englishFontName_ != param.englishFontName_) return false;
  if (englishFontSize_ != param.englishFontSize_) return false;
  if (nonEnglishFontName_ != param.nonEnglishFontName_) return false;
  if (nonEnglishFontSize_ != param.nonEnglishFontSize_) return false;
  
  if (backgroundColor_ != param.backgroundColor_) return false;
  if (foregroundColor_ != param.foregroundColor_) return false;
  if (schemaFileName_ != param.schemaFileName_) return false;
  if (virtualTermType_ != param.virtualTermType_) return false;
  if (keyboardType_ != param.keyboardType_) return false;
  if (numColumns_ != param.numColumns_) return false;
  if (numRows_ != param.numRows_) return false;
  if (numScrollLines_ != param.numScrollLines_) return false;
  if (cursorType_ != param.cursorType_) return false;
  if (escapeString_ != param.escapeString_) return false;
  if (proxyType_ != param.proxyType_) return false;
  if (proxyHostName_ != param.proxyHostName_) return false;
  if (proxyPort_ != param.proxyPort_) return false;
  if (isAuthentation_ != param.isAuthentation_) return false;
  if (proxyUserName_ != param.proxyUserName_) return false;
  if (proxyPassword_ != param.proxyPassword_) return false;
  if (protocolType_ != param.protocolType_) return false;
  if (sshUserName_ != param.sshUserName_) return false;
  if (sshPassword_ != param.sshPassword_) return false;
  if (maxIdleSeconds_ != param.maxIdleSeconds_) return false;
  if (replyKeyCombination_ != param.replyKeyCombination_) return false;
  if (antiIdleMessage_ != param.antiIdleMessage_) return false;
  if (isAutoReply_ != param.isAutoReply_) return false;
  if (autoReplyMessage_ != param.autoReplyMessage_) return false;
  if (isAutoReconnect_ != param.isAutoReconnect_) return false;
  if (reconnectInterval_ != param.reconnectInterval_) return false;
  if (retryTimes_ != param.retryTimes_) return false;
  if (isAutoLoadScript_ != param.isAutoLoadScript_) return false;
  if (autoLoadedScriptFileName_ != param.autoLoadedScriptFileName_) return false;
  if (menuType_ != param.menuType_) return false;
  if (isColorCopy_ != param.isColorCopy_) return false;
  if (isRectSelect_ != param.isRectSelect_) return false;
  if (isAutoCopy_ != param.isAutoCopy_) return false;
  return true;
}

QString FQTermParam::getLanguageName(bool isEnglish)
{
  if (isEnglish) {
    return QString(QObject::tr("English"));
  }
  return QString(QObject::tr("Non-English"));
}

}  // namespace FQTerm
