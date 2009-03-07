// -*- C++ -*-

#ifndef QLUAMAINWINDOW_H
#define QLUAMAINWINDOW_H

#include "qtide.h"


#include <QAction>
#include <QActionGroup>
#include <QFile>
#include <QIcon>
#include <QKeySequence>
#include <QMainWindow>
#include <QMessageBox>
#include <QObject>
#include <QWidget>


class QLuaEditor;
class QLuaTextEdit;
class QLuaTextEditModeFactory;
class QMenu;
class QMenuBar;
class QStatusBar;
class QToolBar;


// Helpers to create actions

namespace QLuaActionHelpers {
  
  struct QTIDE_API Connection { 
    QObject *o; 
    const char *s; 
    Connection(QObject *o, const char *s) : o(o),s(s) {} 
  };
  
  QTIDE_API QAction* operator<<(QAction *action, Connection c);
  QTIDE_API QAction* operator<<(QAction *action, QIcon icon);
  QTIDE_API QAction* operator<<(QAction *action, QActionGroup &group);
  QTIDE_API QAction* operator<<(QAction *action, QKeySequence key);
  QTIDE_API QAction* operator<<(QAction *action, QString s);
  QTIDE_API QAction* operator<<(QAction *action, QVariant v);
  QTIDE_API QAction* operator<<(QAction *action, QAction::MenuRole);
  
}




// Text Editor

class QTIDE_API QLuaMainWindow : public QMainWindow
{
  Q_OBJECT

public:

  enum StdAction { 
    ActionFileNew,
    ActionFileNewLua,
    ActionFileOpen,
    ActionFileOpenRecent,
    ActionFileSave,
    ActionFileSaveAs,
    ActionFilePrint,
    ActionFileClose,
    ActionFileQuit,
    ActionEditSelectAll,
    ActionEditUndo,
    ActionEditRedo,
    ActionEditCut,
    ActionEditCopy,
    ActionEditPaste,
    ActionEditGoto,
    ActionEditFind,
    ActionEditReplace,
    ActionHistoryUp,
    ActionHistoryDown,
    ActionHistorySearch,
    ActionPreferences,
    ActionLineNumbers,
    ActionLineWrap,
    ActionModeSelect,
    ActionModeComplete,
    ActionModeAutoIndent,
    ActionModeAutoMatch,
    ActionModeAutoHighlight,
    ActionModeBalance,
    ActionConsoleClear,
    ActionLuaEval,
    ActionLuaLoad,
    ActionLuaRestart,
    ActionLuaPause,
    ActionLuaStop,
    ActionWhatsThis,
    ActionHelp,
    ActionAbout,
    ActionAboutQt,
  };
  
  ~QLuaMainWindow();
  QLuaMainWindow(QString objName, QWidget *parent=0);

  QAction  *newAction(QString title);
  QAction  *newAction(QString title, bool flag);
  bool      hasAction(StdAction what);
  QAction  *stdAction(StdAction what, bool create=true);
  QAction  *addStdActionToMenu(StdAction what, QMenu *menu);
  QAction  *addStdWindowMenu(QMenuBar *menubar);
  QAction  *addStdHelpMenu(QMenuBar *menubar);
  
  virtual QMenuBar *menuBar();
  virtual QToolBar *toolBar();
  virtual QStatusBar *statusBar();
  virtual QToolBar *createToolBar()     { return 0; }
  virtual QMenuBar  *createMenuBar()    { return 0; }
  virtual QStatusBar *createStatusBar() { return 0; }
  virtual bool canClose()               { return true; }
  virtual QPrinter *loadPageSetup();
  virtual void savePageSetup();
  virtual void loadSettings();
  virtual void saveSettings();

  static QString fileDialogFilters();
  static QString allFilesFilter();

public slots:
  virtual void updateMode(QLuaTextEditModeFactory*);
  virtual void updateActions();
  virtual void updateActionsLater();
  virtual void clearStatusMessage();
  virtual void showStatusMessage(const QString & message, int timeout=0);

  int messageBox(QString t, QString m, 
                 QMessageBox::StandardButtons buttons,
                 QMessageBox::StandardButton def = QMessageBox::NoButton,
                 QMessageBox::Icon icon = QMessageBox::Warning);
  
  QByteArray messageBox(QString t, QString m, 
                        QByteArray buttons = "Ok",
                        QByteArray def = "NoButton",
                        QByteArray icon = "Warning");
  
  virtual void doNew();
  virtual void doOpen();
  virtual void doOpenFile(QString fname);
  virtual void doSave() {}
  virtual void doSaveAs() {}
  virtual void doPrint() {}
  virtual void doSelectAll() {}
  virtual void doUndo() {}
  virtual void doRedo() {}
  virtual void doCut() {}
  virtual void doCopy() {}
  virtual void doPaste() {}
  virtual void doGoto() {}
  virtual void doFind() {}
  virtual void doReplace() {}
  virtual void doPreferences();
  virtual void doMode(QLuaTextEditModeFactory*) {}
  virtual void doLineWrap(bool) {}
  virtual void doLineNumbers(bool) {}
  virtual void doHighlight(bool) {}
  virtual void doCompletion(bool) {}
  virtual void doAutoIndent(bool) {}
  virtual void doAutoMatch(bool) {}
  virtual void doBalance() {}
  virtual void doClear() {}
  virtual void doLoad() {}
  virtual void doEval() {}
  virtual void doRestart() {}
  virtual void doPause() {}
  virtual void doStop() {}
  virtual void doHelp();
  
protected:
  virtual void closeEvent(QCloseEvent *e);
  
public:
  class Global;
  class Private;
private:
  Private *d;
};





#endif


/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */

