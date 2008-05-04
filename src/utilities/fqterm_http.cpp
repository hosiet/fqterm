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

namespace FQTerm {

FQTermHttp::FQTermHttp(QWidget *p, const QString &poolDir)
  : poolDir_(poolDir) {
  // 	m_pDialog = NULL;
  parent_ = p;
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
    FQTermConfig conf(getPath(USER_CONFIG) + "hosts.cfg");
    QString strTmp = conf.getItemValue("hosts", u.host().toLocal8Bit());
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

static void getSaveFileName(const QString &filename, QWidget *widget, QString &fileSave) {
  //TODO: load previous path
  QString strSave = QFileDialog::getSaveFileName(widget,
                                                 "Choose a file to save under",
                                                 getPath(USER_CONFIG) + "/" + filename, "*");

  QFileInfo fi(strSave);

  while (fi.exists()) {
    int yn = QMessageBox::warning(widget, "FQTerm", "File exists. Overwrite?",
                                  "Yes", "No");
    if (yn == 0) {
      break;
    }

    strSave = QFileDialog::getSaveFileName(widget,
                                           "Choose a file to save under",
                                           getPath(USER_CONFIG) + "/" + filename, "*");
    if (strSave.isEmpty()) {
      break;
    }
  }

  if (!strSave.isEmpty()) {
    // TODO: save the path
  }

  fileSave = strSave;
}


void FQTermHttp::httpResponse(const QHttpResponseHeader &hrh) {
  if (hrh.statusCode() == 302) {
    //FIXME: according to RC, this code still could not work
    //if the server send the relative location.
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
    while (fi2.exists()) {
      // all the same
      if (fi2.size() == FileLength) {
        isExisting_ = true;
        http_.abort();
        break;
      } else {
        cacheFileName_ = QString("%1/%2(%3).%4").arg(fi.path()).arg
                         (fi.completeBaseName()).arg(i).arg(fi.suffix());
        fi2.setFile(cacheFileName_);
        i++;
      }
    }
    fi.setFile(cacheFileName_);

    QString strExt = fi.suffix().toLower();
    if (strExt == "jpg" || strExt == "jpeg" || strExt == "gif" || strExt == "mng"
        || strExt == "png" || strExt == "bmp") {
      isPreview_ = true;
    } else {
      isPreview_ = false;
    }
  } else {
    QString strSave;
    getSaveFileName(cacheFileName_, NULL, strSave);
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
