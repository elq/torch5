/* -*- C++ -*- */

#include <QtGlobal>
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QPageSetupDialog>
#include <QPainter>
#include <QPointer>
#include <QRegExp>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include "qluaide.h"
#include "qluatextedit.h"
#include "qluaeditor.h"
#include "qluasdimain.h"
#include "qluamdimain.h"
#include "qluaapplication.h"




// ========================================
// PRIVATE



class QLuaIde::Private : public QObject
{
  Q_OBJECT
public:
  QLuaIde *q;
  int      uid;
  bool     editOnError;
  bool     windowsChangedScheduled;
  QObjectList windows;
  QStringList recentFiles;
  QPointer<QLuaSdiMain> sdiMain;
  QPointer<QLuaMdiMain> mdiMain;
  QPointer<QWidget> returnTo;
  QSet<QWidget*> closeSet;
  bool closingDown;

public slots:
  void destroyed(QObject*);
  void newEngine();
  void errorMessage(QByteArray);
  void windowShown(QWidget *w);
  void scheduleWindowsChanged();
  void emitWindowsChanged();
public:
  Private(QLuaIde *q);
  QLuaEditor *findEditor(QString fname);
};


QLuaIde::Private::Private(QLuaIde *q)
  : QObject(q), 
    q(q), 
    uid(1),
    editOnError(false),
    windowsChangedScheduled(false),
    closingDown(false)
{
  QLuaApplication *app = QLuaApplication::instance();
  connect(app, SIGNAL(newEngine()), this, SLOT(newEngine()));
  newEngine();
}


QLuaEditor *
QLuaIde::Private::findEditor(QString fname)
{
  QLuaEditor *e;
  QString cname = QFileInfo(fname).canonicalFilePath();
  if (! cname.isEmpty())
    foreach(QObject *o, windows)
      if ((e = qobject_cast<QLuaEditor*>(o)))
        if (e->fileName() == cname)
          if (! e->isWindowModified())
            return e;
  if (! cname.isEmpty())
    foreach(QObject *o, windows)
      if ((e = qobject_cast<QLuaEditor*>(o)))
        if (e->fileName() == cname)
          return e;
  return 0;
}


void 
QLuaIde::Private::scheduleWindowsChanged()
{
  if (!windowsChangedScheduled)
    QTimer::singleShot(0, this, SLOT(emitWindowsChanged()));
  windowsChangedScheduled = true;
}


void 
QLuaIde::Private::emitWindowsChanged()
{
  windowsChangedScheduled = false;
  emit q->windowsChanged();
}


static QString
incrementName(QString s)
{
  int l = s.size();
  while (l>0 && s[l-1].isDigit())
    l = l - 1;
  int n = s.mid(l).toInt();
  return s.left(l) + QString::number(n+1);
}


void
QLuaIde::Private::windowShown(QWidget *w)
{
  if (! windows.contains(w))
    {
      // find unique name
      QString name = w->objectName();
      if (name.isEmpty())
        name = "window1";
      QStringList list = q->windowNames();
      while (list.contains(name))
        name = incrementName(name);
      w->setObjectName(name);
      // append window
      connect(w, SIGNAL(destroyed(QObject*)),
              this, SLOT(destroyed(QObject*)) );
      windows.append(w);
      // placement policy
      if (!mdiMain || !mdiMain->adopt(w))
        q->loadWindowGeometry(w);
      // name
      QtLuaEngine *engine = QLuaApplication::engine();
      if (engine)
        engine->nameObject(w);
      // advertise changed to windows list
      scheduleWindowsChanged();
    }
}


void
QLuaIde::Private::destroyed(QObject *o)
{
  windows.removeAll(o);
  scheduleWindowsChanged();
}


void
QLuaIde::Private::newEngine()
{
  QtLuaEngine *engine = QLuaApplication::engine();
  if (engine)
    connect(engine, SIGNAL(errorMessage(QByteArray)),
            this, SLOT(errorMessage(QByteArray)) );
}


static bool 
errorMessageFname(QString fname, int lineno, QString msg, int level)
{
  QFileInfo info(fname);
  if (info.exists())
    {
      QLuaEditor *e = QLuaIde::instance()->editor(fname);
      e->widget()->showLine(lineno);
      if (level)
        msg = QLuaIde::Private::tr("Error: called from here.");
      else 
        msg = QLuaIde::Private::tr("Error: ") + msg;
      e->showStatusMessage(msg);
      return true;
    }
  return false;
}  


static bool 
errorMessageEname(QString ename, int lineno, QString msg, int level)
{
  QtLuaEngine *engine = QLuaApplication::engine();
  QObject *o = engine->namedObject(ename);
  QLuaEditor *e = qobject_cast<QLuaEditor*>(o);
  if (e)
    {
      e->show();
      e->raise();
      e->activateWindow(); 
      e->widget()->showLine(lineno);
      if (level) 
        msg = QLuaIde::Private::tr("Error: called from here.");
      else 
        msg = QLuaIde::Private::tr("Error: ") + msg;
      e->showStatusMessage(msg);
      return true;
    }
  return false;
}


void
QLuaIde::Private::errorMessage(QByteArray m)
{
  QtLuaEngine *engine = QLuaApplication::engine();
  if (editOnError && engine)
    {
      QString message = QString::fromLocal8Bit(m.constData());
      if (message.contains('\n'))
        message.truncate(message.indexOf('\n'));
      QStringList location = engine->lastErrorLocation();
      for (int i = location.size()-1; i>=0; --i)
        {
          QString loc = location.at(i);
          QRegExp re3("^@(.+):([0-9]+)$");
          re3.setMinimal(true);
          if (re3.indexIn(loc) >= 0)
            if (errorMessageFname(re3.cap(1), re3.cap(2).toInt(), message, i))
              continue;
          QRegExp re4("^qt\\.(.+):([0-9]+)$");
          re4.setMinimal(true);
          if (re4.indexIn(loc) >= 0)
            if (errorMessageEname(re4.cap(1), re4.cap(2).toInt(), message, i))
              continue;
        }
    }
}





// ========================================
// QLUAIDE



QPointer<QLuaIde> qLuaIde;


QLuaIde *
QLuaIde::instance()
{
  if (! qLuaIde)
    qLuaIde = new QLuaIde();
  return qLuaIde;
}


QLuaIde::QLuaIde()
  : QObject(QCoreApplication::instance()),
    d(new Private(this))
{
  loadRecentFiles();
  setObjectName("qLuaIde");
  connect(QLuaApplication::instance(), SIGNAL(windowShown(QWidget*)),
          d, SLOT(windowShown(QWidget*)) );
  QtLuaEngine *engine = QLuaApplication::engine();
  if (engine)
    engine->nameObject(this);
  // pickup existing visible windows.
  foreach(QWidget *w, QApplication::topLevelWidgets())
    if (w->windowType() == Qt::Window && w->isVisible() 
        && ! w->testAttribute(Qt::WA_DontShowOnScreen) )
      d->windowShown(w);
}


bool 
QLuaIde::editOnError() const
{
  return d->editOnError;
}


bool 
QLuaIde::mdiDefault() const
{
#ifdef Q_WS_WIN
  return true;
#else
  return false;
#endif
}


QLuaSdiMain *
QLuaIde::sdiMain() const
{
  return d->sdiMain;
}


QLuaMdiMain *
QLuaIde::mdiMain() const
{
  return d->mdiMain;
}


QObjectList 
QLuaIde::windows() const
{
  return d->windows;
}


QStringList 
QLuaIde::windowNames() const
{
  QStringList s;
  foreach(QObject *o, d->windows)
    if (! o->objectName().isEmpty())
      s += o->objectName();
  return s;
}


QStringList 
QLuaIde::recentFiles() const
{
  return d->recentFiles;
}


void 
QLuaIde::setEditOnError(bool b)
{
  d->editOnError = b;
}


void 
QLuaIde::doPreferences(QLuaMainWindow *w)
{
  emit prefsRequested(w);
}


void 
QLuaIde::doHelp(QLuaMainWindow *w)
{
  emit helpRequested(w);
}


void
QLuaIde::doClose()
{
  QWidget *r = 0;
  for (QObject *s = sender(); s; s = s->parent())
    if ((r = qobject_cast<QWidget*>(s)))
      break;
  close(r);
}


bool
QLuaIde::close(QWidget *r)
{
  bool okay = true;
  if (! d->closingDown)
    {
      // confirm
      QString appName = QCoreApplication::applicationName();
      if ( ! QLuaApplication::instance()->isClosingDown())
        if (QMessageBox::question((r) ? r : d->sdiMain, 
                                  tr("Really Quit?"), 
                                  tr("Really quit %0?").arg(appName),
                                  QMessageBox::Ok|QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Ok )
          return false;
      // mdi settings
      if (d->mdiMain)
        d->mdiMain->saveMdiSettings();
      // close all windows but sdimain and mdimain
      d->closingDown = true;
      d->closeSet.clear();
      d->closeSet += 0;
      d->closeSet += d->sdiMain;
      d->closeSet += d->mdiMain;
      QObjectList wl = d->windows;
      while (okay && wl.size())
        {
          QWidget *w = qobject_cast<QWidget*>(wl.takeFirst());
          if (d->closeSet.contains(w))
            continue;
          d->closeSet += w;
          okay = w->close();
          wl = d->windows;
        }
      if (okay && d->sdiMain)
        okay = d->sdiMain->close();
      if (okay && d->mdiMain)
        okay = d->mdiMain->close();
      if (okay)
        okay = QLuaApplication::instance()->close();
      d->closingDown = okay;
    }
  return okay;
}


void 
QLuaIde::addRecentFile(QString fname)
{
  d->recentFiles.removeAll(fname);
  d->recentFiles.prepend(fname);
  while(d->recentFiles.size() > 8)
    d->recentFiles.removeLast();
}


void 
QLuaIde::clearRecentFiles()
{
  QSettings s;
  s.remove("editor/recentFiles");
  d->recentFiles.clear();
}


void 
QLuaIde::loadRecentFiles()
{
  QSettings s;
  d->recentFiles = s.value("editor/recentFiles").toStringList();
}


void 
QLuaIde::saveRecentFiles()
{
  QSettings s;
  s.setValue("editor/recentFiles", d->recentFiles);
}


void 
QLuaIde::activateWidget(QWidget *w)
{
  if (w)
    {
      if (d->mdiMain && d->mdiMain->activate(w))
        return;
      w = w->window();
      w->show();
      w->raise();
      w->activateWindow();
    }
}


void 
QLuaIde::activateConsole(QWidget *returnTo)
{
  if (returnTo && returnTo != d->sdiMain)
    {
      d->returnTo = returnTo;
      d->scheduleWindowsChanged();
    }
  activateWidget(d->sdiMain);
}


void 
QLuaIde::returnToPreviousWindow()
{
  QWidget *w = d->returnTo;
  activateWidget(d->returnTo);
}


void 
QLuaIde::loadWindowGeometry(QWidget *w)
{
  QString name = w->objectName();
  if (d->windows.contains(w) && !name.isEmpty())
    {
      // sdi or mdi?
      QDockWidget *dw = 0;
      QMdiSubWindow *sw = 0;
      QMainWindow *mw = qobject_cast<QMainWindow*>(w);
      while (w && w->windowType() != Qt::Window && w->parentWidget() 
             && ! (sw = qobject_cast<QMdiSubWindow*>(w)) 
             && ! (dw = qobject_cast<QDockWidget*>(w)) )
        w = w->parentWidget();
      // proceed
      if (mw && !name.isEmpty())
        {
          QSettings s;
          s.beginGroup(sw ? "mdi" : "sdi");
          s.beginGroup(name);
          if (! dw)
            w->restoreGeometry(s.value("geometry").toByteArray());
          mw->restoreState(s.value("state").toByteArray());
        }
    }
}


void 
QLuaIde::saveWindowGeometry(QWidget *w)
{
  QString name = w->objectName();
  if (d->windows.contains(w) && !name.isEmpty())
    {
      // sdi or mdi?
      QDockWidget *dw = 0;
      QMdiSubWindow *sw = 0;
      QMainWindow *mw = qobject_cast<QMainWindow*>(w);
      while (w && w->windowType() != Qt::Window && w->parentWidget() 
             && ! (sw = qobject_cast<QMdiSubWindow*>(w)) 
             && ! (dw = qobject_cast<QDockWidget*>(w)) )
        w = w->parentWidget();
      // proceed
      if (mw && !name.isEmpty())
        {
          QSettings s;
          s.beginGroup(sw ? "mdi" : "sdi");
          s.beginGroup(name);
          s.setValue("state", mw->saveState());
          if (! dw)
            s.setValue("geometry", w->saveGeometry());
        }
    }
}


QWidget *
QLuaIde::previousWindow() const
{
  return d->returnTo;
}


QLuaEditor *
QLuaIde::editor(QString fname)
{
  // find existing
  QLuaEditor *e = d->findEditor(fname);
  if (e)
    {
      activateWidget(e);
    }
  else
    {
      // create
      e = new QLuaEditor();
      e->setAttribute(Qt::WA_DeleteOnClose);
      // load
      if (! fname.isEmpty())
        if (! e->readFile(fname))
          {
            delete e;
            return 0;
          }
      // show
      e->show();
    }
  return e;
}


QLuaInspector *
QLuaIde::inspector()
{
  // TODO
  return 0;
}


QLuaBrowser *
QLuaIde::browser(QString s)
{
  if (QFileInfo(s).exists())
    return browser(QUrl::fromLocalFile(s));
  return browser(QUrl(s));
}


QLuaBrowser *
QLuaIde::browser(QUrl url)
{
  // FOR NOW
  QDesktopServices::openUrl(url);
  return 0;
}


QLuaSdiMain *
QLuaIde::createSdiMain()
{
  QLuaSdiMain *e = d->sdiMain;
  QtLuaEngine *engine = QLuaApplication::engine();
  if (e)
    {
      activateWidget(e);
    }
  else
    {
      // create
      e = new QLuaSdiMain();
      e->setAttribute(Qt::WA_DeleteOnClose);
      if (engine)
        engine->nameObject(e);
      // show
      d->sdiMain = e;
      e->show();
    }
  return e;
}


QLuaMdiMain *
QLuaIde::createMdiMain()
{
  QLuaMdiMain *m = d->mdiMain;
  QtLuaEngine *engine = QLuaApplication::engine();
  if (m)
    {
      activateWidget(m);
    }
  else 
    {
      // create
      m = new QLuaMdiMain();
      if (engine)
        engine->nameObject(m);
      // we do not want that:
      // if it contains the console, we'll exit anyway.
      // if it does not contain the console, close hides and adopt shows.
      m->setAttribute(Qt::WA_DeleteOnClose,false);
      // show
      d->mdiMain = m;
    }
  return m;
}




// ========================================
// MOC


#include "qluaide.moc"





/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */
