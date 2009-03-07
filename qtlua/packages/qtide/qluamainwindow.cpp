/* -*- C++ -*- */

#include <QtGlobal>
#include <QApplication>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenu>
#include <QMetaEnum>
#include <QMetaObject>
#include <QMenuBar>
#include <QPointer>
#include <QPrinter>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QVariant>
#include <QWhatsThis>



#include "qluaapplication.h"
#include "qluasdimain.h"
#include "qluamdimain.h"
#include "qluamainwindow.h"
#include "qluatextedit.h"
#include "qluaeditor.h"
#include "qluaide.h"



// ========================================
// QLUAACTIONHELPERS


namespace QLuaActionHelpers {

  QAction * 
  operator<<(QAction *action, QIcon icon)
  {
    action->setIcon(icon);
    return action;
  }
  
  QAction * 
  operator<<(QAction *action, QActionGroup &group)
  {
    action->setActionGroup(&group);
    return action;
  }
  
  QAction * 
  operator<<(QAction *action, QKeySequence shortcut)
  {
    QList<QKeySequence> shortcuts = action->shortcuts();
    shortcuts.prepend(shortcut);
    action->setShortcuts(shortcuts);
    return action;
  }
  
  QAction * 
  operator<<(QAction *action, QString string)
  {
    if (action->text().isEmpty())
      action->setText(string);
    else if (action->statusTip().isEmpty())
      action->setStatusTip(string);
    else if (action->whatsThis().isEmpty())
      action->setWhatsThis(string);
    return action;
  }
  
  QAction *
  operator<<(QAction *action, Connection c)
  {
    QObject::connect(action, SIGNAL(triggered(bool)), c.o, c.s);
    return action;
  }
  
  QAction *
  operator<<(QAction *action, QVariant variant)
  {
    action->setData(variant);
    return action;
  }
  
  QAction *
  operator<<(QAction *action, QAction::MenuRole role)
  {
    action->setMenuRole(role);
    return action;
  }

}



// ========================================
// QLUAMAINWINDOW


using namespace QLuaActionHelpers;


class QLuaMainWindow::Private : public QObject
{
  Q_OBJECT
public:
  QLuaMainWindow *q;
  QPointer<QMenuBar> menuBar;
  QPointer<QToolBar> toolBar;
  QPointer<QStatusBar> statusBar;
  bool updateActionsScheduled;
  QMap<StdAction,QAction*> actionMap;
  QPointer<QMenu> recentMenu;
  QPointer<QMenu> windowMenu;
  QPointer<QMenu> modeMenu;
  QPointer<QMenu> helpMenu;
  QActionGroup *modeGroup;
  QString statusMessage;
  QPrinter *printer;
  
public:
  ~Private();
  Private(QLuaMainWindow *parent);
public slots:
  void fillRecentMenu();
  void doRecentFile();
  void fillWindowMenu();
  void aboutToShowWindowMenu();
  void doWindow();
  void doConsole();
  void fillModeMenu();
  void doMode(QAction *action);
  void messageChanged(QString);
};


QLuaMainWindow::Private::~Private()
{
  delete printer;
}


QLuaMainWindow::Private::Private(QLuaMainWindow *q)
  : QObject(q), q(q), 
    updateActionsScheduled(false),
    modeGroup(0),
    printer(0)
{
}


void 
QLuaMainWindow::Private::fillRecentMenu()
{
  QLuaIde *ide = QLuaIde::instance();
  if (! recentMenu || !ide)
    return;
  recentMenu->clear();
  foreach (QString fname, QLuaIde::instance()->recentFiles())
    {
      QFileInfo fi(fname);
      QString n = fi.fileName();
      QAction *action = recentMenu->addAction(n);
      QFontMetrics fm(action->font());
      QString p = fm.elidedText(fi.filePath(), Qt::ElideLeft, 300);
      action->setText(QString("%1 [%2]").arg(fi.fileName()).arg(p));
      action->setData(fname);
      action->setToolTip(tr("Open the named file."));
      connect(action, SIGNAL(triggered()), this, SLOT(doRecentFile()));
    }
  recentMenu->addSeparator();
  QAction *action = recentMenu->addAction(tr("&Clear"));
  action->setToolTip(tr("Clear the history of recent files."));
  connect(action, SIGNAL(triggered()), 
          QLuaIde::instance(), SLOT(clearRecentFiles()));
}


void 
QLuaMainWindow::Private::doRecentFile()
{
  QAction *action = qobject_cast<QAction*>(sender());
  if (action)
    {
      QString fname = action->data().toString();
      if (! fname.isEmpty())
        q->doOpenFile(fname);
    }
}


void 
QLuaMainWindow::Private::fillWindowMenu()
{
  QLuaIde *ide = QLuaIde::instance();
  QMenu *menu = windowMenu;
  if (! menu || !ide)
    return;
  menu->clear();
  // return to previous window action
  if (q != ide->sdiMain())
    {
      menu->addAction(tr("&Return to Console")) 
        << QKeySequence(tr("F6","windows|returnto"))
        << Connection(this, SLOT(doConsole()))
        << tr("Return to console.");
    }
  else if (ide->previousWindow())
    {
      QString s = tr("&Return to Previous","windows|returnto");
      QLuaEditor *e = qobject_cast<QLuaEditor*>(ide->previousWindow());
      if (e && !e->fileName().isEmpty())
        s = tr("&Return to %1","windows|returnto")
          .arg(QFileInfo(e->fileName()).fileName());
      menu->addAction(s)
        << QKeySequence(tr("F6","windows|returnto"))
        << Connection(ide, SLOT(returnToPreviousWindow()))
        << tr("Return to previously active window.");
    }
  if (! menu->actions().isEmpty() && ide)
    menu->addSeparator();
  // mdi action
  QLuaMdiMain *mdiMain = ide->mdiMain();
  if (mdiMain)
    {
      QObject *parent = q->parent();
      while (parent && parent != mdiMain)
        parent = parent->parent();
      if (! parent)
        mdiMain = 0;
    }
  if (mdiMain)
    {
      menu->addAction(mdiMain->dockAction());
      menu->addAction(mdiMain->cascadeAction());
      menu->addAction(mdiMain->tileAction());
      menu->addSeparator();
    }
  // window list
  int k = 0;
  foreach(QObject *o, ide->windows())
    {
      QWidget *w = qobject_cast<QWidget*>(o);
      if (w == 0 || w == mdiMain)
        continue;
      QAction *action = 0;
      QString s = w->windowTitle();
      if (s.isEmpty())
        continue;
      action = menu->addAction(s.replace("[*]",""))
        << qVariantFromValue<QObject*>(o)
        << Connection(this, SLOT(doWindow()))
        << tr("Activate the specified window.");
      action->setCheckable(true);
      if (++k < 10)
        action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_0 + k));
    }
}


void 
QLuaMainWindow::Private::aboutToShowWindowMenu()
{
  QObject *o = 0;
  QWidget *w = 0;
  QLuaIde *ide = QLuaIde::instance();
  QLuaMdiMain *mdiMain = ide->mdiMain();
  foreach(QAction *action, windowMenu->actions())
    if (action->isCheckable() &&
        (o = qVariantValue<QObject*>(action->data())) &&
        (ide && ide->windows().contains(o)) &&
        (w = qobject_cast<QWidget*>(o)) )
      {
        if (mdiMain)
          action->setChecked(mdiMain->isActive(w));
        else
          action->setChecked(w->isActiveWindow());
      }
}


void 
QLuaMainWindow::Private::doConsole()
{
  QLuaIde *ide = QLuaIde::instance();
  ide->activateConsole(q);
}


void 
QLuaMainWindow::Private::doWindow()
{
  QObject *o = 0;
  QWidget *w = 0;
  QAction *a = qobject_cast<QAction*>(sender());
  if (a)
    o = qVariantValue<QObject*>(a->data());
  QLuaIde *ide = QLuaIde::instance();
  if (o && ide->windows().contains(o))
    w = qobject_cast<QWidget*>(o);
  if (w)
    ide->activateWidget(w);
}


void 
QLuaMainWindow::Private::fillModeMenu()
{
  if (modeMenu)
    {
      delete modeGroup;
      modeGroup = new QActionGroup(q);
      modeGroup->setExclusive(true);
      connect(modeGroup, SIGNAL(triggered(QAction*)),
              this, SLOT(doMode(QAction*)));
      foreach(QLuaTextEditModeFactory *mode, 
              QLuaTextEditModeFactory::factories())
        {
          QAction *action = modeMenu->addAction(mode->name());
          action->setToolTip("Select the named editor mode.");
          action->setCheckable(true);
          action->setData(qVariantFromValue<void*>(mode));
          modeGroup->addAction(action);
        }
      QAction *action = modeMenu->addAction("None");
      action->setToolTip("Cancel all editor mode.");
      action->setCheckable(true);
      action->setChecked(true);
      modeGroup->addAction(action);
    }
}


void 
QLuaMainWindow::Private::doMode(QAction *action)
{
  if (action) 
    {
      void *data = qVariantValue<void*>(action->data());
      QLuaTextEditModeFactory *f = (data) ? (QLuaTextEditModeFactory*)data : 0;
      q->doMode(f);
    }
}


void 
QLuaMainWindow::Private::messageChanged(QString s)
{
  if (statusBar && s.isEmpty() && !statusMessage.isEmpty())
    statusBar->showMessage(statusMessage);
  
}


QLuaMainWindow::~QLuaMainWindow()
{
  QLuaIde *ide = QLuaIde::instance();
  ide->saveRecentFiles();
  disconnect(ide, 0, this, 0);
  disconnect(ide, 0, d, 0);
}


QLuaMainWindow::QLuaMainWindow(QString objname, QWidget *parent)
  : QMainWindow(parent), d(new Private(this))
{
  setObjectName(objname);
}


QMenuBar *
QLuaMainWindow::menuBar()
{
  if (! d->menuBar)
    if ((d->menuBar = createMenuBar()))
      setMenuBar(d->menuBar);
  return d->menuBar;
}


QToolBar *
QLuaMainWindow::toolBar()
{
  if (! d->toolBar)
    if ((d->toolBar = createToolBar()))
      {
        if (d->toolBar->objectName().isEmpty())
          d->toolBar->setObjectName("mainToolBar");
        if (d->toolBar->windowTitle().isEmpty())
          d->toolBar->setWindowTitle(tr("Main ToolBar"));
        addToolBar(Qt::TopToolBarArea, d->toolBar);
      }
  return d->toolBar;
}


QStatusBar *
QLuaMainWindow::statusBar()
{
  if (! d->statusBar)
    if ((d->statusBar = createStatusBar()))
      {
        setStatusBar(d->statusBar);
        connect(d->statusBar, SIGNAL(messageChanged(QString)),
                d, SLOT(messageChanged(QString)) );
      }
  return d->statusBar;
}


QPrinter *
QLuaMainWindow::loadPageSetup()
{
  QPrinter *printer = d->printer;
  if (! printer)
    d->printer = printer = new QPrinter;
  QSettings s;
  if (s.contains("printer/pageSize"))
    {
      int n = s.value("printer/pageSize").toInt();
      if (n >= 0 || n < QPrinter::Custom)
        printer->setPaperSize((QPrinter::PageSize)n);
    }
  if (s.contains("printer/pageMargins/unit"))
    {
      int unit = s.value("printer/pageMargins/unit").toInt();
      if (unit >= 0 && unit < QPrinter::DevicePixel)
        {
          qreal left,top,right,bottom;
          left = s.value("printer/pageMargins/left").toDouble();  
          top = s.value("printer/pageMargins/top").toDouble();  
          right = s.value("printer/pageMargins/right").toDouble();  
          bottom = s.value("printer/pageMargins/bottom").toDouble();
          printer->setPageMargins(left,top,right,bottom,(QPrinter::Unit)unit);
        }
    }
  return printer;
}


void 
QLuaMainWindow::savePageSetup()
{
  QPrinter *printer = d->printer;
  if (printer)
    {
      QSettings s;
      QPrinter::PageSize ps = printer->paperSize();
      if (ps < QPrinter::Custom)
        s.setValue("printer/pageSize", (int)(printer->pageSize()));
      qreal left,top,right,bottom;
      QPrinter::Unit unit = QPrinter::Millimeter;
      printer->getPageMargins(&left,&top,&right,&bottom,unit);
      s.setValue("printer/pageMargins/left", left);  
      s.setValue("printer/pageMargins/top", top);  
      s.setValue("printer/pageMargins/right", right);  
      s.setValue("printer/pageMargins/bottom", bottom);
      s.setValue("printer/pageMargins/unit", (int)unit);
    }
}


void 
QLuaMainWindow::loadSettings()
{
  QLuaIde *ide = QLuaIde::instance();
  ide->loadWindowGeometry(this);
}


void 
QLuaMainWindow::saveSettings()
{
  QLuaIde *ide = QLuaIde::instance();
  ide->saveWindowGeometry(this);
}


QAction *
QLuaMainWindow::newAction(QString text)
{
  QAction *action = new QAction(text, this);
  action->setMenuRole(QAction::NoRole);
  return action;
}


QAction *
QLuaMainWindow::newAction(QString text, bool flag)
{
  QAction *action = new QAction(text, this);
  action->setMenuRole(QAction::NoRole);
  action->setCheckable(true);
  action->setChecked(flag);
  return action;
}


bool
QLuaMainWindow::hasAction(StdAction what)
{
  return d->actionMap.contains(what);
}


QAction*
QLuaMainWindow::stdAction(StdAction what, bool create)
{
  QString name;
  QAction *action = 0;
  if (d->actionMap.contains(what))
    return d->actionMap[what];
  if (! create)
    return 0;
  switch(what)
    {
    case ActionFileNew:
      action = newAction(tr("&New","file|new"))
        << QKeySequence(QKeySequence::New)
        << QIcon(":/images/filenew.png")
        << Connection(this, SLOT(doNew()))
        << tr("Create a new text editor window.");
      break;
      
    case ActionFileOpen:
      action = newAction(tr("&Open", "file|open"))
        << QKeySequence(QKeySequence::Open)
        << QIcon(":/images/fileopen.png")
        << Connection(this, SLOT(doOpen()))
        << tr("Open a new text editor window.");
      break;

    case ActionFileOpenRecent:
      d->recentMenu = new QMenu(tr("Open &Recent","file|recent"), this);
      connect(d->recentMenu, SIGNAL(aboutToShow()), d, SLOT(fillRecentMenu()));
      action = d->recentMenu->menuAction()
        << QIcon(":/images/filerecent.png")
        << tr("Create a new text editor window.");
      break;
      
    case ActionFileSave:
      action = newAction(tr("&Save", "file|save"))
        << QKeySequence(QKeySequence::Save)
        << QIcon(":/images/filesave.png")
        << Connection(this, SLOT(doSave()))
        << tr("Save the contents of this window.");
      break;
      
    case ActionFileSaveAs:
      action = newAction(tr("Save &As...", "file|saveas"))
        << QIcon(":/images/filesaveas.png")
        << Connection(this, SLOT(doSaveAs()))
        << tr("Save the contents of this window into a new file.");
      break;

    case ActionFilePrint:
      action = newAction(tr("&Print...", "file|print"))
        << QKeySequence(QKeySequence::Print)
        << QIcon(":/images/fileprint.png")
        << Connection(this, SLOT(doPrint()))
        << tr("Print the contents of this window.");
      break;
      
    case ActionFileClose:
      action = newAction(tr("&Close", "file|close"))
        << QKeySequence(QKeySequence::Close)
        << QIcon(":/images/fileclose.png")
        << Connection(this, SLOT(close()))
        << tr("Close this window.");
      break;

    case ActionFileQuit:
      action = newAction(tr("&Quit", "file|quit"))
        << QKeySequence(tr("Ctrl+Q", "file|quit"))
        << QIcon(":/images/filequit.png")
        << Connection(QLuaIde::instance(), SLOT(doClose()))
        << tr("Quit the application.")
        << QAction::QuitRole;
      break;
      
    case ActionEditSelectAll:
      action = newAction(tr("Select &All", "edit|selectall"))
        << QKeySequence(QKeySequence::SelectAll)
        << Connection(this, SLOT(doSelectAll()))
        << tr("Select everything.");
      break;

    case ActionEditUndo:
      action = newAction(tr("&Undo", "edit|undo"))
        << QKeySequence(QKeySequence::Undo)
        << QIcon(":/images/editundo.png")
        << Connection(this, SLOT(doUndo()))
        << tr("Undo last edit.");
      break;
      
    case ActionEditRedo:
      action = newAction(tr("&Redo", "edit|redo"))
        << QKeySequence(QKeySequence::Redo)
        << QIcon(":/images/editredo.png")
        << Connection(this, SLOT(doRedo()))
        << tr("Redo last undo.");
      break;
      
    case ActionEditCut:
      action = newAction(tr("Cu&t", "edit|cut"))
        << QKeySequence(QKeySequence::Cut)
        << QIcon(":/images/editcut.png")
        << Connection(this, SLOT(doCut()))
        << tr("Cut selection to clipboard.");
      break;

    case ActionEditCopy:
      action = newAction(tr("&Copy", "edit|copy"))
        << QKeySequence(QKeySequence::Copy)
        << QIcon(":/images/editcopy.png")
        << Connection(this, SLOT(doCopy()))
        << tr("Copy selection to clipboard.");
      break;
      
    case ActionEditPaste:
      action = newAction(tr("&Paste", "edit|paste"))
        << QKeySequence(QKeySequence::Paste)
        << QIcon(":/images/editpaste.png")
        << Connection(this, SLOT(doPaste()))
        << tr("Paste from clipboard.");
      break;
      
    case ActionEditGoto:
      action = newAction(tr("&Go to Line", "edit|goto"))
        << QKeySequence(tr("Ctrl+G", "edit|goto"))
        << QIcon(":/images/editgoto.png")
        << Connection(this, SLOT(doGoto()))
        << tr("Go to numbered line.");
      break;
      
    case ActionEditFind:
      action = newAction(tr("&Find", "edit|find"))
        << QKeySequence(QKeySequence::Find)
        << QIcon(":/images/editfind.png")
        << Connection(this, SLOT(doFind()))
        << tr("Find text.");
      break;
      
    case ActionEditReplace:
      action = newAction(tr("&Replace", "edit|findreplace"))
        << QKeySequence(QKeySequence::Replace)
        << QIcon(":/images/editreplace.png")
        << Connection(this, SLOT(doReplace()))
        << tr("Find and replace text.");
      break;

    case ActionHistoryUp:
      action = newAction(tr("&Up History","history|up"))
        << QKeySequence(tr("Ctrl+Up","history|up"))
        << QIcon(":/images/up.png")
        << tr("Previous item in command history.");
      break;

    case ActionHistoryDown:
      action = newAction(tr("&Down History","history|down"))
        << QKeySequence(tr("Ctrl+Down","history|down"))
        << QIcon(":/images/down.png")
        << tr("Next item in command history.");
      break;
      
    case ActionHistorySearch:
      action = newAction(tr("&Search History","history|search"))
        << QKeySequence(tr("Ctrl+R","history|search"))
        << QIcon(":/images/history.png")
        << tr("Search command history.");
      break;

    case ActionPreferences:
      action = newAction(tr("&Preferences", "edit|prefs")) 
        << QIcon(":/images/editprefs.png")
        << Connection(this, SLOT(doPreferences()))
        << tr("Show the preference dialog.")
        << QAction::PreferencesRole;
      break;
      
    case ActionLineNumbers:
      action = newAction(tr("Show Line &Numbers", "tools|linenumbers"), false)
        << Connection(this, SLOT(doLineNumbers(bool)))
        << tr("Show line numbers.");
      break;
      
    case ActionLineWrap:
      action = newAction(tr("&Wrap Long Lines", "tools|linewrap"), false)
        << Connection(this, SLOT(doLineWrap(bool)))
        << tr("Toggle line wrapping mode.");
      break;
      
    case ActionModeSelect:
      // connect actions
      d->modeMenu = new QMenu(tr("Mode","tools|mode"), this);
      d->fillModeMenu();
      action = d->modeMenu->menuAction() 
        << tr("Select the target language.");
      break;
      
    case ActionModeComplete:
      action = newAction(tr("Auto Com&pletion", "tools|complete"), true)
        << Connection(this, SLOT(doCompletion(bool)))
        << tr("Toggle automatic completion with TAB key.");
      break;
      
    case ActionModeAutoIndent:
      action = newAction(tr("Auto &Indent", "tools|autoindent"), true)
        << Connection(this, SLOT(doAutoIndent(bool)))
        << tr("Toggle automatic indentation with TAB and ENTER keys.");
      break;
      
    case ActionModeAutoMatch:
      action = newAction(tr("Show &Matches", "tools|automatch"), true)
        << Connection(this, SLOT(doAutoMatch(bool)))
        << tr("Toggle display of matching parenthesis or constructs");
      break;
      
    case ActionModeAutoHighlight:
      action = newAction(tr("&Colorize", "tools|autohighlight"), true)
        << Connection(this, SLOT(doHighlight(bool)))
        << tr("Toggle syntax colorization.");
      break;

    case ActionModeBalance:
      action = newAction(tr("&Balance", "tools|balance"))
        << QIcon(":/images/editbalance.png")
        << QKeySequence(tr("Ctrl+B","lua|lastexpr"))
        << Connection(this, SLOT(doBalance()))
        << tr("Select successive surrounding syntactical construct.");
      break;

    case ActionConsoleClear:
      action = newAction(tr("&Clear Console", "tools|clear"))
        << QIcon(":/images/clear.png")
        << Connection(this, SLOT(doClear()))
        << tr("Clear all text in console window.");
      break;

    case ActionLuaEval:
      action = newAction(tr("&Eval Lua Expression","lua|eval"))
        << QKeySequence(tr("Ctrl+E","lua|eval"))
#ifdef Q_WS_MAC
        << QKeySequence(tr("F4","lua|eval"))
        << QKeySequence(tr("Ctrl+Enter","lua|load"))
        << QKeySequence(tr("Ctrl+Return", "lua|load"))
#else
        << QKeySequence(tr("Ctrl+Enter","lua|load"))
        << QKeySequence(tr("Ctrl+Return", "lua|load"))
        << QKeySequence(tr("F4","lua|eval"))
#endif
        << QIcon(":/images/playerplay.png")
        << Connection(this, SLOT(doEval()))
        << tr("Evaluate the selected Lua expression.");
      break;
      
    case ActionLuaLoad:
      action = newAction(tr("&Load Lua File","lua|load"))
        << QIcon(":/images/playerload.png")
        << QKeySequence(tr("F5","lua|load"))
        << Connection(this, SLOT(doLoad()))
        << tr("Load the file into the Lua interpreter.");
      break;
      
    case ActionLuaRestart:
      action = newAction(tr("&Restart and Load File","lua|restart"))
        << QIcon(":/images/playerrestart.png")
        << QKeySequence(tr("Shift+F5","lua|restart"))
        << Connection(this, SLOT(doRestart()))
        << tr("Restart the Lua interpreter and load the file.");
      break;

    case ActionLuaPause:
      action = newAction(tr("&Pause","lua|pause"), false)
        << QIcon(":/images/playerpause.png")
        << QKeySequence(tr("ScrollLock","lua|pause"))
        << Connection(this, SLOT(doPause()))
        << tr("Suspend or resume the execution of the current Lua command.");
      break;

    case ActionLuaStop:
      action = newAction(tr("&Stop","lua|stop"))
        << QIcon(":/images/stop.png")
#ifdef Q_WS_MAC
        << QKeySequence(tr("Ctrl+Pause","lua|stop"))
        << QKeySequence(tr("Ctrl+.","lua|stop"))
#else
        << QKeySequence(tr("Ctrl+.","lua|stop"))
        << QKeySequence(tr("Ctrl+Pause","lua|stop"))
#endif
        << Connection(this, SLOT(doStop()))
        << tr("Stop the execution of the current Lua command.");
      break;

    case ActionWhatsThis:
      action = QWhatsThis::createAction();
      break;
      
    case ActionHelp:
      action = newAction(tr("Help Index", "help|help"))
        << QKeySequence(tr("F1", "help|help"))
        << QIcon(":/images/helpindex.png")
        << Connection(this, SLOT(doHelp()))
        << tr("Opens the help window.");
      break;
      
    case ActionAbout:
      name = QCoreApplication::applicationName();
      action = newAction(tr("About %1", "help|aboutqtlua").arg(name))
        << tr("Display information about %1.").arg(name)
        << Connection(QLuaApplication::instance(), SLOT(about()))
        << QAction::AboutRole;
      break;
      
    case ActionAboutQt:
      action = newAction(tr("About Qt", "help|aboutqt"))
        << tr("Display information about Qt.")
        << Connection(QLuaApplication::instance(), SLOT(aboutQt()))
        << QAction::AboutQtRole;
      break;
      
    default:
      break;
    }
  if (action)
    d->actionMap[what] = action;
  return action;
}


QAction *
QLuaMainWindow::addStdActionToMenu(StdAction what, QMenu *menu)
{
  QAction *action = stdAction(what);
  if (action)
    menu->addAction(action);
  return action;
}


QAction *
QLuaMainWindow::addStdWindowMenu(QMenuBar *menubar)
{
  QLuaIde *ide = QLuaIde::instance();
  if (!d->windowMenu)
    {
      d->windowMenu = new QMenu(tr("Windows","windows|"), this);
      connect(d->windowMenu, SIGNAL(aboutToShow()),
              d, SLOT(aboutToShowWindowMenu()) );
      connect(ide, SIGNAL(windowsChanged()), 
              d, SLOT(fillWindowMenu()) );
    }
  if (d->windowMenu)
    {
      d->fillWindowMenu();
      return menubar->addMenu(d->windowMenu);
    }
  return 0;
}


QAction *
QLuaMainWindow::addStdHelpMenu(QMenuBar *menubar)
{
  if (!d->helpMenu)
    {
      d->helpMenu = new QMenu(tr("&Help", "help|"), this);
      addStdActionToMenu(ActionHelp, d->helpMenu);
      d->helpMenu->addSeparator();
      addStdActionToMenu(ActionAbout, d->helpMenu);
      addStdActionToMenu(ActionAboutQt, d->helpMenu);
    }
  if (d->helpMenu)
    return menubar->addMenu(d->helpMenu);
  return 0;
}


void
QLuaMainWindow::closeEvent(QCloseEvent *event)
{
  if (isHidden())
    event->accept();
  else if (canClose()) {
    saveSettings();
    event->accept();
  } else 
    event->ignore();
}


void 
QLuaMainWindow::updateMode(QLuaTextEditModeFactory *factory)
{
  if  (d->modeGroup)
    foreach(QAction *action, d->modeGroup->actions())
      action->setChecked(qVariantValue<void*>(action->data()) 
                         == (void*)factory);
}


void 
QLuaMainWindow::updateActionsLater()
{
  if (! d->updateActionsScheduled)
    {
      d->updateActionsScheduled = true;
      QTimer::singleShot(0, this, SLOT(updateActions()));
    }
}


void 
QLuaMainWindow::updateActions()
{
  d->updateActionsScheduled = false;
}


void 
QLuaMainWindow::clearStatusMessage()
{
  d->statusMessage.clear();
  if (d->statusBar)
    d->statusBar->clearMessage();
}


void 
QLuaMainWindow::showStatusMessage(const QString & message, int timeout)
{
  if (! d->statusBar)
    d->statusBar = QMainWindow::statusBar();
  if (! timeout)
    d->statusMessage = message;
  if (d->statusBar)
    d->statusBar->showMessage(message, timeout);
}


int
QLuaMainWindow::messageBox(QString t, QString m, 
                           QMessageBox::StandardButtons buttons,
                           QMessageBox::StandardButton def,
                           QMessageBox::Icon icon)
{
  QMessageBox box(icon, t, m, buttons, this);
  box.setDefaultButton(def);
  return box.exec();
}


QByteArray
QLuaMainWindow::messageBox(QString t, QString m, QByteArray buttons,
                           QByteArray def, QByteArray icon)
{
  const QMetaObject *mo = &QMessageBox::staticMetaObject;
  const QMetaEnum meb = mo->enumerator(mo->indexOfEnumerator("StandardButtons"));
  const QMetaEnum mei = mo->enumerator(mo->indexOfEnumerator("Icon"));
  if (meb.isValid() && mei.isValid())
    {
      QMessageBox::StandardButtons b
        = (QMessageBox::StandardButtons)meb.keysToValue(buttons.constData());
      QMessageBox::StandardButton d
        = (QMessageBox::StandardButton)meb.keyToValue(def.constData());
      QMessageBox::Icon i
        = (QMessageBox::Icon)mei.keyToValue(icon.constData());
      if (b>=0 && d>=0 && i>=0)
        return meb.valueToKey(messageBox(t,m,b,d,i));
    }
  return QByteArray("<invalidargs>");
}


void 
QLuaMainWindow::doNew()
{
  QLuaEditor *e = QLuaIde::instance()->editor();
  e->updateActionsLater();
}


void 
QLuaMainWindow::doOpen()
{
  QString m = tr("Open File");
  QString d = QDir::currentPath();
  QString f = fileDialogFilters();
  QString s = allFilesFilter();
  QFileDialog::Options o = QFileDialog::DontUseNativeDialog;
  QStringList files = QFileDialog::getOpenFileNames(window(), m, d, f, &s, o);
  foreach(QString fname, files)
    if (! fname.isEmpty())
      doOpenFile(fname);
}


void 
QLuaMainWindow::doOpenFile(QString fname)
{
  QLuaEditor *e =  QLuaIde::instance()->editor(fname);
  if (e)
    e->updateActionsLater();
}


void 
QLuaMainWindow::doPreferences()
{
  QLuaIde::instance()->doPreferences(this);
}


void 
QLuaMainWindow::doHelp()
{
  QLuaIde::instance()->doHelp(this);
}






// ========================================
// QLUAMAINWINDOW STATICS



QString 
QLuaMainWindow::fileDialogFilters()
{
  QStringList filters;
  foreach(QLuaTextEditModeFactory *mode, 
          QLuaTextEditModeFactory::factories())
    filters += mode->filter();
  filters += allFilesFilter();
  return filters.join(";;");
}


QString 
QLuaMainWindow::allFilesFilter()
{
  return tr("All Files (*)");
}




// ========================================
// MOC


#include "qluamainwindow.moc"





/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */
