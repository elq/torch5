// -*- C++ -*-

#ifndef QLUAEDITOR_H
#define QLUAEDITOR_H

#include "qtide.h"
#include "qluatextedit.h"
#include "qluamainwindow.h"

#include <QFile>
#include <QObject>
#include <QWidget>




// Text Editor

class QTIDE_API QLuaEditor : public QLuaMainWindow
{
  Q_OBJECT
  Q_PROPERTY(QString fileName READ fileName)

public:
  QLuaEditor(QWidget *parent=0);

  Q_INVOKABLE virtual bool readFile(QFile &file);
  Q_INVOKABLE virtual bool writeFile(QFile &file, bool rename=false);
  Q_INVOKABLE bool readFile(QString fname);
  Q_INVOKABLE bool writeFile(QString fname, bool rename=true);
  Q_INVOKABLE QLuaTextEdit *widget();
  QString fileName() const;
  
  virtual QToolBar *createToolBar();
  virtual QMenuBar  *createMenuBar();
  virtual QStatusBar *createStatusBar();
  virtual bool canClose();
  virtual void loadSettings();
  virtual void saveSettings();
  
public slots:
  virtual void doNew();
  virtual void doOpen();
  virtual void doOpenFile(QString fname);
  virtual void doSave();
  virtual void doSaveAs();
  virtual void doPrint();
  virtual void doSelectAll();
  virtual void doUndo();
  virtual void doRedo();
  virtual void doCut();
  virtual void doCopy();
  virtual void doPaste();
  virtual void doGoto();
  virtual void doFind();
  virtual void doReplace();
  virtual void doMode(QLuaTextEditModeFactory*);
  virtual void doLineWrap(bool);
  virtual void doLineNumbers(bool);
  virtual void doHighlight(bool);
  virtual void doAutoIndent(bool);
  virtual void doAutoMatch(bool);
  virtual void doCompletion(bool);
  virtual void doBalance();
  virtual void doLoad();
  virtual void doEval();
  virtual void doRestart();
  virtual void updateStatusBar();
  virtual void updateWindowTitle();
  virtual void updateActions();
  
public:
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

