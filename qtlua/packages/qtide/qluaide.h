// -*- C++ -*-

#ifndef QLUAIDE_H
#define QLUAIDE_H

#include "qtide.h"

#include <QFile>
#include <QList>
#include <QObject>
#include <QObjectList>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QWidget>

class QLuaMainWindow;
class QLuaEditor;
class QLuaInspector;
class QLuaSdiMain;
class QLuaMdiMain;
class QLuaBrowser;


// Text editor widget


class QTIDE_API QLuaIde : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool editOnError READ editOnError WRITE setEditOnError)
  Q_PROPERTY(bool mdiDefault READ mdiDefault)

public:
  static QLuaIde *instance();
  bool editOnError() const;
  bool mdiDefault() const;

  Q_INVOKABLE QObjectList windows() const;
  Q_INVOKABLE QStringList windowNames() const;
  Q_INVOKABLE QStringList recentFiles() const;
  Q_INVOKABLE QWidget* previousWindow() const;
  Q_INVOKABLE QLuaSdiMain *sdiMain() const;
  Q_INVOKABLE QLuaMdiMain *mdiMain() const;

public slots:
  void setEditOnError(bool b);
  void doPreferences(QLuaMainWindow *w);
  void doHelp(QLuaMainWindow *w);
  void doClose();
  bool close(QWidget *r);
  void addRecentFile(QString fname);
  void clearRecentFiles();
  void loadRecentFiles();
  void saveRecentFiles();
  void activateWidget(QWidget *w);
  void activateConsole(QWidget *returnTo=0);
  void returnToPreviousWindow();
  void loadWindowGeometry(QWidget *w);
  void saveWindowGeometry(QWidget *w);
  QLuaEditor    *editor(QString fname = QString());
  QLuaBrowser   *browser(QUrl url);
  QLuaBrowser   *browser(QString url);
  QLuaInspector *inspector(); 
  QLuaSdiMain   *createSdiMain();
  QLuaMdiMain   *createMdiMain();

signals:
  void windowsChanged();
  void prefsRequested(QLuaMainWindow *window);
  void helpRequested(QLuaMainWindow *window);

public:
  class Private;
  class Initializer;
protected:
  QLuaIde();
private:
  Private *d;
};



#endif


/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */

