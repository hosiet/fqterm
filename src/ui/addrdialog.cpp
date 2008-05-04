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

#include <QPixmap>
#include <QMessageBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QPainter>
#include <QFileDialog>
#include <QPalette>
#include <QMenu>

#include "fqterm_config.h"
#include "fqterm_param.h"
#include "fqterm_path.h"
#include "fqterm_trace.h"

#include "addrdialog.h"
#include "schemadialog.h"

namespace FQTerm {

const int addrDialog::ports[3] = {23, 22, 22};

/*
 *  Constructs a addrDialog which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
addrDialog::addrDialog(QWidget *parent_, const FQTermParam& parameter, Qt::WFlags fl)
  : QDialog(parent_, fl),
    menuButtonGroup_(this),
    param_(parameter) {
  ui_.setupUi(this);
  menuButtonGroup_.addButton(ui_.radioButton1, 0);
  menuButtonGroup_.addButton(ui_.radioButton2, 1);
  menuButtonGroup_.addButton(ui_.radioButton3, 2);
  QMenu *fontMenu = new QMenu(this);
  for (int i = 0; i < 2; ++i) {
    QAction *act = fontMenu->addAction(FQTermParam::getLanguageName(bool(i)) + " Font", this, SLOT(onFont()));
    act->setData(i);
  }
  ui_.fontToolButton->setMenu(fontMenu);
  setUIFromParam();
  connector();
}

void addrDialog::setParamFromUI() {
  param_.name_ = ui_.nameLineEdit->text();
  param_.hostAddress_ = ui_.addrLineEdit->text();
  param_.port_ = ui_.portLineEdit->text().toUShort();
  param_.hostType_ = ui_.hostTypeComboBox->currentIndex();
  param_.preLoginCommand_ = ui_.preloginLineEdit->text();
  param_.userName_ = ui_.userLineEdit->text();
  param_.password_ = ui_.passwdLineEdit->text();
  param_.postLoginCommand_ = ui_.postloginLineEdit->text();
  param_.serverEncodingID_ = ui_.bbscodeComboBox->currentIndex();
  param_.dispEncodingID_ = ui_.displaycodeComboBox->currentIndex();
  param_.isFontAutoFit_ = ui_.autofontCheckBox->isChecked();
  param_.isAlwaysHighlight_ = ui_.highlightCheckBox->isChecked();
  param_.isAnsiColor_ = ui_.ansicolorCheckBox->isChecked();
  param_.backgroundColor_ = ui_.fontPreviewer->palette().color(QPalette::Background);
  param_.foregroundColor_ = ui_.fontPreviewer->palette().color(QPalette::Foreground);
  param_.schemaFileName_ = schemaFileName_;
  param_.virtualTermType_ = ui_.termtypeLineEdit->text();
  param_.keyboardType_ = ui_.keytypeComboBox->currentIndex();
  param_.numColumns_ = ui_.columnLineEdit->text().toInt();
  param_.numRows_ = ui_.rowLineEdit->text().toInt();
  param_.numScrollLines_ = ui_.scrollLineEdit->text().toInt();
  param_.cursorType_ = ui_.cursorTypeComboBox->currentIndex();
  param_.escapeString_ = ui_.escapeLineEdit->text();
  param_.proxyType_ = ui_.proxytypeComboBox->currentIndex();
  param_.proxyHostName_ = ui_.proxyaddrLineEdit->text();
  param_.proxyPort_ = ui_.proxyportLineEdit->text().toUShort();
  param_.isAuthentation_ = ui_.authentationGroup->isChecked();
  param_.proxyUserName_ = ui_.proxyuserLineEdit->text();
  param_.proxyPassword_ = ui_.proxypasswdLineEdit->text();
  param_.protocolType_ = ui_.protocolComboBox->currentIndex();
  if (param_.port_ != ports[param_.protocolType_]) {
    ui_.portCheckBox->setChecked(true);
  }
  switch(param_.protocolType_)
  {
  case 0:
    param_.isAutoLogin_ = ui_.telnetAutoLoginGroup->isChecked();
  	break;
  case 1:
  case 2:
    param_.isAutoLogin_ = ui_.sshAutoLoginGroup->isChecked();
    break;
  }
  param_.sshUserName_ = ui_.sshuserLineEdit->text();
  param_.sshPassword_ = ui_.sshpasswdLineEdit->text();
  param_.maxIdleSeconds_ = ui_.idletimeLineEdit->text().toInt();
  param_.replyKeyCombination_ = ui_.replykeyLineEdit->text();
  if (param_.replyKeyCombination_.isNull()) {
    FQ_TRACE("addrdialog", 0) << "Key combination for reply is NULL.";
  }
  param_.antiIdleMessage_ = ui_.antiLineEdit->text();
  param_.isAutoReply_ = ui_.replyCheckBox->isChecked();
  param_.autoReplyMessage_ = ui_.replyLineEdit->text();
  param_.isAutoReconnect_ = ui_.reconnectCheckBox->isChecked();
  param_.reconnectInterval_ = ui_.reconnectLineEdit->text().toInt();
  param_.retryTimes_ = ui_.retryLineEdit->text().toInt();
  param_.isAutoLoadScript_ = ui_.scriptCheckBox->isChecked();
  param_.autoLoadedScriptFileName_ = ui_.scriptLineEdit->text();
  param_.menuType_ = menuButtonGroup_.checkedId();
  param_.menuColor_ = ui_.menuLabel->palette().color(QPalette::Background);
}

void addrDialog::setUIFromParam() {
  schemaFileName_ = param_.schemaFileName_;
  QString strTmp;
  ui_.nameLineEdit->setText(param_.name_);
  ui_.addrLineEdit->setText(param_.hostAddress_);
  strTmp.setNum(param_.port_);
  ui_.portLineEdit->setText(strTmp);
  ui_.hostTypeComboBox->setCurrentIndex(param_.hostType_);
//  ui_.autoLoginCheckBox->setChecked(param_.isAutoLogin_);
  ui_.preloginLineEdit->setText(param_.preLoginCommand_);
  ui_.userLineEdit->setText(param_.userName_);
  ui_.passwdLineEdit->setText(param_.password_);
  ui_.postloginLineEdit->setText(param_.postLoginCommand_);
  ui_.bbscodeComboBox->setCurrentIndex(param_.serverEncodingID_);
  ui_.displaycodeComboBox->setCurrentIndex(param_.dispEncodingID_);
  ui_.autofontCheckBox->setChecked(param_.isFontAutoFit_);
  ui_.highlightCheckBox->setChecked(param_.isAlwaysHighlight_);
  ui_.ansicolorCheckBox->setChecked(param_.isAnsiColor_);
  ui_.termtypeLineEdit->setText(param_.virtualTermType_);
  ui_.keytypeComboBox->setCurrentIndex(param_.keyboardType_);
  strTmp.setNum(param_.numColumns_);
  ui_.columnLineEdit->setText(strTmp);
  strTmp.setNum(param_.numRows_);
  ui_.rowLineEdit->setText(strTmp);
  strTmp.setNum(param_.numScrollLines_);
  ui_.scrollLineEdit->setText(strTmp);
  ui_.cursorTypeComboBox->setCurrentIndex(param_.cursorType_);
  ui_.escapeLineEdit->setText(param_.escapeString_);
  ui_.proxytypeComboBox->setCurrentIndex(param_.proxyType_);
  ui_.proxyaddrLineEdit->setText(param_.proxyHostName_);
  strTmp.setNum(param_.proxyPort_);
  ui_.proxyportLineEdit->setText(strTmp);
  ui_.authentationGroup->setChecked(param_.isAuthentation_);
  ui_.proxyuserLineEdit->setText(param_.proxyUserName_);
  ui_.proxypasswdLineEdit->setText(param_.proxyPassword_);
  ui_.protocolComboBox->setCurrentIndex(param_.protocolType_);
  onProtocol(param_.protocolType_);
  ui_.sshuserLineEdit->setText(param_.sshUserName_);
  ui_.sshpasswdLineEdit->setText(param_.sshPassword_);
  strTmp.setNum(param_.maxIdleSeconds_);
  ui_.idletimeLineEdit->setText(strTmp);
  ui_.replykeyLineEdit->setText(param_.replyKeyCombination_);
  ui_.antiLineEdit->setText(param_.antiIdleMessage_);
  ui_.replyCheckBox->setChecked(param_.isAutoReply_);
  ui_.replyLineEdit->setEnabled(param_.isAutoReply_);
  ui_.replyLineEdit->setText(param_.autoReplyMessage_);
  ui_.reconnectCheckBox->setChecked(param_.isAutoReconnect_);
  ui_.reconnectLineEdit->setEnabled(param_.isAutoReconnect_);
  ui_.retryLineEdit->setEnabled(param_.isAutoReconnect_);
  strTmp.setNum(param_.reconnectInterval_);
  ui_.reconnectLineEdit->setText(strTmp);
  strTmp.setNum(param_.retryTimes_);
  ui_.retryLineEdit->setText(strTmp);
  ui_.scriptCheckBox->setChecked(param_.isAutoLoadScript_);
  ui_.scriptLineEdit->setText(param_.autoLoadedScriptFileName_);
  //ui.menuGroup->setButton(param_.m_nMenuType);
  QRadioButton *rbMenu = qobject_cast < QRadioButton * > (menuButtonGroup_.button
    (param_.menuType_));
  rbMenu->setChecked(true);
  previewFont();
  previewMenu();
}


/*
 *  Destroys the object and frees any allocated resources
 */
addrDialog::~addrDialog() {
}


void addrDialog::onOK() {
  setParamFromUI();
  done(1);
}

void addrDialog::onCancel() {
  done(0);
}



void addrDialog::onFgcolor() {
  QColor color = QColorDialog::getColor(param_.foregroundColor_);
  if (color.isValid() == TRUE) {
    param_.foregroundColor_ = color;
    previewFont();
  }
}

void addrDialog::onBgcolor() {
  QColor color = QColorDialog::getColor(param_.backgroundColor_);
  if (color.isValid() == TRUE) {
    param_.backgroundColor_ = color;
    previewFont();
  }
}

void addrDialog::onSchema() {
  schemaDialog schema(this);

  schema.setSchema(schemaFileName_);

  if (schema.exec() == 1) {
    schemaFileName_ = schema.getSchema();
    if (schemaFileName_.isEmpty()) {
      schemaFileName_ = "";
    }
  }
}



void addrDialog::onProtocol(int n) {
  ui_.portCheckBox->setChecked(false);
  ui_.portLineEdit->setText(QString("%1").arg(ports[n]));
  switch(n)
  {
  case 0: //telnet
    ui_.telnetAutoLoginGroup->setEnabled(true);
    ui_.telnetAutoLoginGroup->setChecked(param_.isAutoLogin_);
    ui_.sshAutoLoginGroup->setEnabled(false);
    ui_.sshAutoLoginGroup->setChecked(false);
    break;
  case 1:
#if defined(_NO_SSH_COMPILED)
    QMessageBox::warning(this, "sorry",
      "SSH support is not compiled, check your OpenSSL and try to recompile FQTerm");
    ui_.protocolComboBox->setCurrentItem(0);
#else
    ui_.telnetAutoLoginGroup->setEnabled(false);
    ui_.telnetAutoLoginGroup->setChecked(false);
    ui_.sshAutoLoginGroup->setEnabled(true);
    ui_.sshAutoLoginGroup->setChecked(param_.isAutoLogin_);
#endif
    break;
  case 2:
#if defined(_NO_SSH_COMPILED)
    QMessageBox::warning(this, "sorry",
      "SSH support is not compiled, check your OpenSSL and try to recompile FQTerm");
    ui_.protocolComboBox->setCurrentItem(0);
#else
    ui_.telnetAutoLoginGroup->setEnabled(false);
    ui_.telnetAutoLoginGroup->setChecked(false);
    ui_.sshAutoLoginGroup->setEnabled(true);
    ui_.sshAutoLoginGroup->setChecked(param_.isAutoLogin_);
#endif
    break;
  }
}

void addrDialog::onChooseScript() {
  QString path = getPath(USER_CONFIG) + "script";

  QString strFile = QFileDialog::getOpenFileName(this, "choose a script file",
                                                 path, "JavaScript File (*.js)");

  if (strFile.isNull()) {
    return ;
  }

  ui_.scriptLineEdit->setText(strFile);
}

void addrDialog::onMenuColor() {
  QColor color = QColorDialog::getColor(param_.menuColor_);
  if (color.isValid() == TRUE) {
    param_.menuColor_ = color;
    previewMenu();
  }
}

void addrDialog::previewFont() {
  //issue 98 preview both en and zh

  QPalette palette;
  palette.setColor(QPalette::Window, param_.backgroundColor_);
  palette.setColor(QPalette::WindowText, param_.foregroundColor_);
  ui_.fontPreviewer->setPalette(palette);
  QString sample("<html><body style=\" font-family:'" + param_.englishFontName_ 
    + "'; font-size:" + QString().setNum(param_.englishFontSize_) 
    + "pt;\"><BR>AaBbCc</body></html>");
  sample += QString("<html><body style=\" font-family:'" 
    + param_.nonEnglishFontName_ + "'; font-size:" 
    + QString().setNum(param_.nonEnglishFontSize_) + "pt;\">" 
    + param_.nonEnglishFontName_ + "<BR></body></html>");
  ui_.fontPreviewer->setText(sample);
}


void addrDialog::connector() {
  FQ_VERIFY(connect(ui_.applyPushButton, SIGNAL(clicked()), this, SLOT(onOK())));
  FQ_VERIFY(connect(ui_.closePushButton, SIGNAL(clicked()), this, SLOT(onCancel())));


  FQ_VERIFY(connect(ui_.fgcolorPushButton, SIGNAL(clicked()), this, SLOT(onFgcolor())));
  FQ_VERIFY(connect(ui_.bgcolorPushButton, SIGNAL(clicked()), this, SLOT(onBgcolor())));
  FQ_VERIFY(connect(ui_.schemaPushButton, SIGNAL(clicked()), this, SLOT(onSchema())));

  FQ_VERIFY(connect(ui_.protocolComboBox, SIGNAL(activated(int)), this, SLOT(onProtocol(int))));

  FQ_VERIFY(connect(ui_.scriptPushButton, SIGNAL(clicked()), this, SLOT(onChooseScript())));

  FQ_VERIFY(connect(ui_.menuColorButton, SIGNAL(clicked()), this, SLOT(onMenuColor())));
}

void addrDialog::previewMenu() {
  QPalette palette;
  palette.setColor(QPalette::Window, param_.menuColor_);
  ui_.menuLabel->setPalette(palette);
}

void addrDialog::onFont() {
  bool isEnglish = ((QAction*)(sender()))->data().toBool();
  bool ok;
  QString& fontName = isEnglish?param_.englishFontName_:param_.nonEnglishFontName_;
  int& fontSize = isEnglish?param_.englishFontSize_:param_.nonEnglishFontSize_;
  QFont now(fontName, fontSize);

  QFont font = QFontDialog::getFont(&ok, now);
  if (ok == true) {
    fontName = font.family();
    fontSize = font.pointSize();
    previewFont();
  }
}

}  // namespace FQTerm

#include "addrdialog.moc"
