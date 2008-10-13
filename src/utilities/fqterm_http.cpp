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

#include <QString>
#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDataStream>
#include <QUrl>
#include <QRegExp>

#include "fqterm.h"
#include "fqterm_path.h"
#include "fqterm_http.h"
#include "fqterm_config.h"
#include "fqterm_filedialog.h"

namespace FQTerm {

QMap<QString, int> FQTermHttp::downloadMap_;
QMutex FQTermHttp::mutex_;

FQTermHttp::FQTermHttp(FQTermConfig *config, QWidget *p, const QString &poolDir)
  : poolDir_(poolDir) {
  // 	m_pDialog = NULL;
  config_ = config;

  FQ_VERIFY(connect(&http_, SIGNAL(done(bool)), this, SLOT(httpDone(bool))));
  FQ_VERIFY(connect(&http_, SIGNAL(dataReadProgress(int, int)),
                  this, SLOT(httpRead(int, int))));
  FQ_VERIFY(connect(&http_, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)),
                  this, SLOT(httpResponse(const QHttpResponseHeader &))));
}

FQTermHttp::~FQTermHttp() {
  // 	if(m_pDialog!=NULL)
  // 		delete m_pDialog;
}

void FQTermHttp::cancel() {
  http_.abort();

  // remove unsuccessful download
  if (QFile::exists(cacheFileName_)) {
    QFile::remove(cacheFileName_);
  }

  emit done(this);
}

void FQTermHttp::getLink(const QString &url, bool preview) {
  isExisting_ = false;
  isPreview_ = preview;
  previewEmitted = false;
  lastPercent_ = 0;
  QUrl u(url);
  if (u.isRelative() || u.scheme() == "file") {
    emit previewImage(cacheFileName_, false);
    emit done(this);
    return ;
  }

  //	QString path = url.mid(url.find(u.host(),false) + u.host().length());
  //	QString path = url.mid(url.find('/',
  //							url.find(u.host(),false),
  //							false));

  if (QFile::exists(getPath(USER_CONFIG) + "hosts.cfg")) {
    config_ = new FQTermConfig(getPath(USER_CONFIG) + "hosts.cfg");
    QString strTmp = config_->getItemValue("hosts", u.host().toLocal8Bit());
    if (!strTmp.isEmpty()) {
      QString strUrl = url;
      strUrl.replace(QRegExp(u.host(), Qt::CaseInsensitive), strTmp);
      u = strUrl;
    }
  }
  cacheFileName_ = QFileInfo(u.path()).fileName();
  http_.setHost(u.host(), u.port(80));
  http_.get(u.path() + "?" + u.encodedQuery());
}
/*
static void getSaveFileName(const QString &filename, QWidget *widget, QString &fileSave) {

  QString strPrevSave, strSave;
  QString userConfig = getPath(USER_CONFIG) + "fqterm.cfg";
  config_ = new FQTermConfig(userConfig);

  if (QFile::exists(userConfig)) {
	strPrevSave = config_->getItemValue("global", "previous");
  }

  if (strPrevSave.isEmpty()) {
	strSave = QFileDialog::getSaveFileName(widget,
										"Choose a directory to save under",
										getPath(USER_CONFIG) + "/" + filename, "*");
  } else {
	strSave = QFileDialog::getSaveFileName(widget,
										"Choose a directory to save under",
										strPrevSave + "/" + filename, "*");
  }

  QFileInfo fi(strSave);

  while (fi.exists()) {
    int yn = QMessageBox::warning(widget, "FQTerm", "File exists. Overwrite?",
                                  "Yes", "No");
    if (yn == 0) {
      break;
    }

    strSave = QFileDialog::getSaveFileName(widget,
                                           "Choose a directory to save under",
                                           fi.absolutePath() + "/" + filename, "*");
    if (strSave.isEmpty()) {
      break;
    }
  }

  if (!strSave.isEmpty()) {
	if (QFile::exists(userConfig)) {
	  config_->setItemValue("global", "previous", fi.absolutePath());
	  config_->save(userConfig);
	}
  }

  fileSave = strSave;
}
*/

void FQTermHttp::httpResponse(const QHttpResponseHeader &hrh) {
  if (hrh.statusCode() == 302) {
    //FIXME: according to RC, this code still could not work
    //if the server send the relative location.
//    QString realLocation = '/' + hrh.value("Location");
	QString realLocation = '/' + hrh.value("Location");
    http_.close();
    http_.get(realLocation);
    return;
  }
  if (hrh.statusCode() != 200) {
    return;
  }
  QString ValueString;
  QString filename;

  ValueString = hrh.value("Content-Length");
  int FileLength = ValueString.toInt();

  ValueString = hrh.value("Content-Disposition");
  //	ValueString = ValueString.mid(ValueString.find(';') + 1).stripWhiteSpace();
  //	if(ValueString.lower().find("filename") == 0)
  //	m_strHttpFile = ValueString.mid(ValueString.find('=') + 1).stripWhiteSpace();
  if (ValueString.right(1) != ";") {
    ValueString += ";";
  }
  QRegExp re("filename=.*;", Qt::CaseInsensitive);
  re.setMinimal(true);
  //Dont FIXME:this will also split filenames with ';' inside, does anyone really do this?
  int pos = re.indexIn(ValueString);
  if (pos != -1) {
    cacheFileName_ = ValueString.mid(pos + 9, re.matchedLength() - 10);
  }
  filename = cacheFileName_ = G2U(cacheFileName_.toLatin1());

  if (isPreview_) {
    cacheFileName_ = poolDir_ + cacheFileName_;

    QFileInfo fi(cacheFileName_);

    int i = 1;
    QFileInfo fi2 = fi;

    mutex_.lock();
    if (downloadMap_.find(cacheFileName_) == downloadMap_.end() && !fi2.exists()) { 
      downloadMap_[cacheFileName_] = FileLength;
    }
    while (fi2.exists()) {

      QMap<QString, int>::iterator ii;
      if ((ii = downloadMap_.find(cacheFileName_)) != downloadMap_.end()) { 
        if (ii.value() == FileLength) {
          http_.abort();
          isExisting_ = true;
          emit done(this);
          mutex_.unlock();
          return;
        }
      }

      if (fi2.size() == FileLength) {
        isExisting_ = true;
        http_.abort();
        break;
      } else {

		cacheFileName_ = QString("%1/%2(%3).%4").arg(fi.path())
								.arg(fi.completeBaseName()).arg(i).arg(fi.suffix());
        fi2.setFile(cacheFileName_);
        if (!fi2.exists()) {
          downloadMap_[cacheFileName_] = FileLength;
          break;
        }
        
        i++;
      }
    }
    mutex_.unlock();

    fi.setFile(cacheFileName_);

    QString strExt = fi.suffix().toLower();
    if (strExt == "jpg" || strExt == "jpeg" || strExt == "gif" || strExt == "mng"
        || strExt == "png" || strExt == "bmp") {
      isPreview_ = true;
    } else {
      isPreview_ = false;
    }
  } else {
//    getSaveFileName(cacheFileName_, NULL, strSave);
	mutex_.lock();

	FQTermFileDialog fileDialog(config_);
    QString strSave = fileDialog.getSaveName(cacheFileName_, "*");
	mutex_.unlock();
    // no filename specified which means the user canceled this download
    if (strSave.isEmpty()) {
      http_.abort();
      emit done(this);
      return ;
    }
    cacheFileName_ = strSave;
  }

  emit headerReceived(this, filename);
}


void FQTermHttp::httpRead(int done, int total) {
  QByteArray ba = http_.readAll();
  QFile file(cacheFileName_);
  if (file.open(QIODevice::ReadWrite | QIODevice::Append)) {
    QDataStream ds(&file);
    ds.writeRawData(ba, ba.size());
    file.close();
  }
  if (total != 0) {
	//m_pDialog->setProgress(done,total);
    int p = done *100 / total;
    if (p - lastPercent_ >= 10 && isPreview_ && QFileInfo(cacheFileName_).suffix().toLower() == "jpg") {
      if (!previewEmitted) {
        emit previewImage(cacheFileName_,true);
        previewEmitted = true;
      } else {
        emit previewImage(cacheFileName_,false);
      }
      lastPercent_ = p;
    }
    emit percent(p);
  }
}

void FQTermHttp::httpDone(bool err) {
  if (err) {
    switch (http_.error()) {
      case QHttp::Aborted: if (isExisting_) {
        break;
      } else {
        emit done(this);
        return ;
      }
      default:
        QMessageBox::critical(NULL, tr("Download Error"), tr(
                                  "Failed to download file"));
        deleteLater();
        return ;
    }
  } else {
    mutex_.lock();
    downloadMap_.remove(cacheFileName_);
    mutex_.unlock();
  }

  if (isPreview_) {
    emit previewImage(cacheFileName_, true);
  } else {
    emit message("Download one file successfully");
  }

  emit done(this);
}

}  // namespace FQTerm

#include "fqterm_http.moc"
