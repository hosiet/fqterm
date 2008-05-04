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

//WARNING this is not meant for use outside this unit!

#ifndef FQTERM_PROGRESSBAR_H
#define FQTERM_PROGRESSBAR_H

#include <QProgressBar>
#include <QLabel>

class QLabel;
class QPushButton;

namespace FQTerm {
/**
 * @class KDE::ProgressBar
 * @short ProgressBar class with some useful additions
 */
class ProgressBar: public QProgressBar {
  friend class StatusBar;

 public:
  /** @param text a 1-6 word description of the progress operation */
  ProgressBar &setDescription(const QString &text);

  /** @param text eg. Scanning, Reading. The state of the operation */
  ProgressBar &setStatus(const QString &text);

  /** set the recipient slot for the abort button */
  ProgressBar &setAbortSlot(QObject *receiver, const char *slot);

  void setDone();

  QString description()const {
    return description_;
  }

 protected:
  ProgressBar(QWidget *parent, QLabel *label);
  ~ProgressBar();

  virtual void hide();

  QLabel *label_;
  QString description_;
  bool isFinished_;

  QPushButton *abortButton_;
};

}  // namespace FQTerm

#endif  // FQTERM_PROGRESSBAR_H
