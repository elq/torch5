/* -*- C++ -*- */

#include <QtGlobal>
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDockWidget>
#include <QLayout>
#include <QLayoutItem>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QMenuBar>
#include <QPointer>
#include <QRegExp>
#include <QSet>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include "qluaide.h"
#include "qluamainwindow.h"
#include "qluaeditor.h"
#include "qluasdimain.h"
#include "qluamdimain.h"

#include "qluaapplication.h"




// ========================================
// CLASSES


class QLuaMdiMain::Private : public QObject 
{
  Q_OBJECT
public:
  Private(QLuaMdiMain *parent);
  bool eventFilter(QObject *watched, QEvent *event);
public slots:
  void dockCurrentWindow(bool b);
  void setActive(Client *client);
  void focusChange(QWidget *old, QWidget *now);
  void guiUpdateLater();
  void guiUpdate();
public:
  QLuaMdiMain *q;
  QMdiArea *area;
  QList<Client*> clients;
  QMap<QWidget*,Client*> table;
  QPointer<Client> active;
  bool closingDown;
  bool guiUpdateScheduled;
  bool tabMode;
  QByteArray clientClass;
  QPointer<QStatusBar> myStatusBar;
  QPointer<QMenu> fileMenu;
  QPointer<QMenu> editMenu;
  QPointer<QAction> tileAction;
  QPointer<QAction> cascadeAction;
  QPointer<QAction> dockAction;
};


class QLuaMdiMain::Client : public QObject 
{
  Q_OBJECT
public:
  ~Client();
  Client(QWidget *widget, Private *parent);
  bool computeWindowTitle();
  virtual bool eventFilter(QObject *watched, QEvent *event);
  void setVisible(bool);
  void deActivate();
  void reActivate();
  bool copyMenuBar(QMenuBar *mb);
public slots:
  void destroyed(QObject*);
  void stealMenuBar();
  void stealStatusBar();
  void disown();
  void fillToolMenu();
  void setupAsSubWindow();
  void setupAsDockWidget();
  void toplevelChanged(bool b);
public:
  Private *d;
  QPointer<QWidget> widget;
  QPointer<QMdiSubWindow> subWindow;
  QPointer<QDockWidget> dockWidget;
  QPointer<QMenuBar> menuBar;
  QPointer<QStatusBar> statusBar;
};




// ========================================
// SHELL



// We insert this between the qmdisubwindow and a plain qwidget
// because QMdiSubWindow really dislikes widgets without layouts
// and without size hints...

class QLuaMdiMain::Shell : public QWidget
{
  Q_OBJECT
  QPointer<QWidget> w;
  bool settingGeometry;
public:
  Shell(QWidget *w, QWidget *p = 0);
  virtual QSize sizeHint () const;
  virtual bool eventFilter(QObject *o, QEvent *e);
  virtual void resizeEvent(QResizeEvent *e);
};


QLuaMdiMain::Shell::Shell(QWidget *w, QWidget *p)
  : QWidget(p), w(w), settingGeometry(false)
{
  w->setParent(this);
  w->installEventFilter(this);
  setWindowModified(w->isWindowModified());
  setWindowTitle(w->windowTitle());
  setFocusProxy(w);
  w->move(0,0);
}


QSize
QLuaMdiMain::Shell::sizeHint() const
{
  return (w) ? w->size() : QSize();
}


void 
QLuaMdiMain::Shell::resizeEvent(QResizeEvent *e)
{
  if (w && w->size() != e->size())
    {
      settingGeometry = true;
      w->setGeometry(rect());
      settingGeometry = false;
    }
}


bool 
QLuaMdiMain::Shell::eventFilter(QObject *o, QEvent *e)
{
  if (o == w)
    switch(e->type())
      {
      case QEvent::Resize:
        if (! settingGeometry)
          {
            updateGeometry();
            QWidget *parent = parentWidget();
            while (parent && !qobject_cast<QMdiSubWindow*>(parent))
              parent = parentWidget();
            if (parent)
              parent->adjustSize();
          }
        break;
      case QEvent::ModifiedChange:
        setWindowModified(w->isWindowModified());
        break;
      case QEvent::WindowTitleChange:
        setWindowTitle(w->windowTitle());
        break;
      default:
        break;
      }
  return false;
}







// ========================================
// PRIVATE IMPLEMENTATION


QLuaMdiMain::Private::Private(QLuaMdiMain *parent)
  : QObject(parent), 
    q(parent),
    closingDown(false),
    guiUpdateScheduled(false),
    tabMode(false)
{
  area = new QMdiArea(q);
  area->installEventFilter(this);
  connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*,QWidget*)),
          this, SLOT(focusChange(QWidget*,QWidget*)) );
}


bool
QLuaMdiMain::Private::eventFilter(QObject *watched, QEvent *event)
{
  if (watched != area)
    return false;
  if (event->type() == QEvent::Resize)
    {
      QRect ar = area->rect().adjusted(+20,+20,-20,-20);
      foreach(Client *client, clients)
        if (client->subWindow && 
            ! ar.intersects(client->subWindow->frameGeometry()) )
          {
            // move window to be at least partially visible
            QRect gr = client->subWindow->geometry();
            gr.moveRight(qMax(gr.right(), ar.left()));
            gr.moveLeft(qMin(gr.left(), ar.right()));
            gr.moveBottom(qMax(gr.bottom(), ar.top()));
            gr.moveTop(qMin(gr.top(), ar.bottom()));
            client->subWindow->setGeometry(gr);
          }
    }
  if (event->type() == QEvent::WindowActivate)
    guiUpdateLater();
  return false;
}


void 
QLuaMdiMain::Private::focusChange(QWidget *old, QWidget *now)
{
  QWidget *w = now;
  while (w && !table.contains(w))
    w = w->parentWidget();
  if (w && table.contains(w))
    setActive(table[w]);
}

void 
QLuaMdiMain::Private::setActive(Client *client)
{
  if (active != client)
    {
      guiUpdateLater();
      if (active)
        active->deActivate();
      if (client)
        active = client;
      if (active)
        active->reActivate();
    }
}

void 
QLuaMdiMain::Private::guiUpdateLater()
{
  if (! guiUpdateScheduled && ! closingDown)
    {
      guiUpdateScheduled = true;
      QTimer::singleShot(1, this, SLOT(guiUpdate()));
      // avoid flicker during complex update
      q->setUpdatesEnabled(false);
    }
}


void 
QLuaMdiMain::Private::guiUpdate()
{
  guiUpdateScheduled = false;
  if (closingDown)
    return;
  
  // maximize
  if (tabMode && active && active->subWindow)
    active->subWindow->showMaximized();

  // menubar
  QMenuBar *menubar = new QMenuBar;
  if (!active || !active->copyMenuBar(menubar))
    {
      q->addStdFileMenu(menubar);
      q->addStdEditMenu(menubar);
      q->addStdWindowMenu(menubar);
      q->addStdHelpMenu(menubar);
    }
#ifdef Q_WS_MAC
  // Qt/Mac does not like when one changes an active menubar
  if (q->menuBar()->activeAction())
    delete menubar;
  else
#endif
    q->setMenuBar(menubar);


  // dockaction
  QAction *da = q->dockAction();
  da->setEnabled(active != 0);
  da->setChecked(active && active->dockWidget!=0);

  // end flicker avoidance
  QApplication::sendPostedEvents();
  if (! closingDown)
    q->setUpdatesEnabled(true);
}


void 
QLuaMdiMain::Private::dockCurrentWindow(bool b)
{
  if (active)
    {
      if (b && !active->dockWidget)
        active->setupAsDockWidget();
      else if (!b && active->dockWidget) 
        active->setupAsSubWindow();
    }
}







// ========================================
// CLIENT IMPLEMENTATION


QLuaMdiMain::Client::~Client()
{
  QWidget *w = widget;
  if (w)
    {
      QLuaIde *ide = QLuaIde::instance();
      bool visible = w->isVisibleTo(w->parentWidget());
      disown();
      if (d->active == this)
        deActivate();
      if (menuBar && !menuBar->actions().isEmpty())
        menuBar->show();
      if (statusBar && qobject_cast<QMainWindow*>(w))
        static_cast<QMainWindow*>(w)->setStatusBar(statusBar);
      if (ide)
        ide->loadWindowGeometry(w);
      w->setUpdatesEnabled(true);
      w->setVisible(visible);
      statusBar = 0;
      menuBar = 0;
    }
  d->table.remove(w);
  d->table.remove(subWindow);
  d->table.remove(dockWidget);
  delete subWindow;
  delete dockWidget;
  d->clients.removeAll(this);
  if (d->clients.isEmpty())
    d->q->hide();
  delete statusBar;
  delete menuBar;
  d->guiUpdateLater();
}


QLuaMdiMain::Client::Client(QWidget *w, Private *parent)
  : QObject(parent),
    d(parent),
    widget(w)
{
  // make sure we can embed this widget
  w->installEventFilter(this);
  connect(w, SIGNAL(destroyed(QObject*)), this, SLOT(destroyed(QObject*)));
  // record our existence
  d->clients.append(this);
  d->table[w] = this;
  // steal menubar and statusbar
  stealMenuBar();
  stealStatusBar();
}


void 
QLuaMdiMain::Client::destroyed(QObject *o)
{
  deleteLater();
}


bool
QLuaMdiMain::Client::computeWindowTitle()
{
  if (widget)
    {
      QString title = widget->windowTitle();
      bool modified = widget->isWindowModified();
      if (d->tabMode && subWindow)
        {
          if (title.contains("[*] - "))
            title.replace(QRegExp("\\[\\*\\] - .*$"), "[*]");
          if (title.contains("[*]"))
            title.replace("[*]", widget->isWindowModified() ? "*" : "");
          subWindow->setWindowTitle(title);
          return true;
        }
      else if (subWindow)
        {
          subWindow->setWindowTitle(title);
          subWindow->setWindowModified(modified);
          return true;
        }
      else if (dockWidget)
        {
          dockWidget->setWindowTitle(title);
          dockWidget->setWindowModified(modified);
          return true;
        }
    }
  return false;
}


bool 
QLuaMdiMain::Client::eventFilter(QObject *watched, QEvent *event)
{
  // watched == widget
  if (watched == widget)
    {
      switch(event->type())
        {
        case QEvent::HideToParent:
          setVisible(false);
          break;
        case QEvent::ShowToParent:
          setVisible(true);
          break;
        case QEvent::WindowTitleChange:
        case QEvent::ModifiedChange:
          return computeWindowTitle();
        default:
          break;
        }
    }
  // watched == subWindow
  else if (watched == subWindow)
    {
    }
  // watched == dockWidget
  else if (watched == dockWidget)
    {
      switch(event->type())
        {
        case QEvent::Close:
          if (widget && !widget->close())
            event->ignore();
          return true;
        default:
          break;
        }
    }
  // watched == menuBar
  else if (watched == menuBar)
    {
      switch(event->type())
        {
        case QEvent::ActionAdded:
        case QEvent::ActionChanged:
        case QEvent::ActionRemoved:
          d->guiUpdateLater();
          break;
        default:
          break;
        }
    }
  return false;
}


void
QLuaMdiMain::Client::setVisible(bool show)
{
  if (! widget)
    show = false;
  if (subWindow)
    subWindow->setVisible(show);
  else if (dockWidget)
    dockWidget->setVisible(show);
}


void 
QLuaMdiMain::Client::disown()
{
  QLuaIde *ide = QLuaIde::instance();
  if (widget && widget->parentWidget())
    {
      bool visible = widget->isVisibleTo(widget->parentWidget());
      if (subWindow)
        ide->saveWindowGeometry(widget);
      widget->hide();
      widget->setParent(0);
      d->table.remove(subWindow);
      d->table.remove(dockWidget);
      if (dockWidget)
        dockWidget->deleteLater();
      if (subWindow)
        subWindow->deleteLater();
    }
}


void 
QLuaMdiMain::Client::fillToolMenu()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  QMainWindow *main = qobject_cast<QMainWindow*>(widget);
  if (menu && widget)
    {
      menu->clear();
      QMenu *popup = main->createPopupMenu();
      if (popup)
        foreach (QAction *action, popup->actions())
          menu->addAction(action);
      delete popup;
    }
}


void 
QLuaMdiMain::Client::setupAsSubWindow()
{
  QLuaIde *ide = QLuaIde::instance();
  if (! widget)
    return;
  if (dockWidget)
    disown();
  if (! subWindow)
    {
      // special for qobjects
      QWidget *w = widget;
      if (!w->layout() && w->metaObject() == &QWidget::staticMetaObject)
        w = new Shell(widget);
      // create subwindow
      subWindow = d->area->addSubWindow(w);
      d->table[subWindow] = this;
      // mark widget as a simple widget
      Qt::WindowFlags flags = widget->windowFlags() & ~Qt::WindowType_Mask;
      widget->setWindowFlags(flags | Qt::Widget);
      // subwindow callbacks
      subWindow->installEventFilter(this);
      // prepare toolbar action
      QMenu *toolMenu = new QMenu(subWindow);
      connect(toolMenu, SIGNAL(aboutToShow()), this, SLOT(fillToolMenu()));
      QAction *toolAction = new QAction(tr("Toolbars"), subWindow);
      toolAction->setMenu(toolMenu);
      // prepare dock action
      QAction *dockAction = new QAction(tr("Dock"), subWindow);
      dockAction->setCheckable(true);
      connect(dockAction, SIGNAL(triggered(bool)),
              this, SLOT(setupAsDockWidget()) );
      // tweak system menu
      QMenu *menu = subWindow->systemMenu();
      QKeySequence cseq(QKeySequence::Close);
      foreach (QAction *action, menu->actions())
        {
          if (action->shortcut() == cseq)
            action->setShortcut(QKeySequence());
          if ((action->shortcut() == cseq || action->isSeparator() ) ) 
            {
              if (dockAction)
                menu->insertAction(action, dockAction);
              dockAction = 0;
              if (toolAction && qobject_cast<QMainWindow*>(widget))
                menu->insertAction(action, toolAction);
              toolAction = 0;
            }
        }
      // show
      computeWindowTitle();
      ide->loadWindowGeometry(w);
      d->guiUpdateLater();
      widget->show();
      subWindow->show();
      // set focus
      d->setActive(this);
      QWidget *fw = widget;
      fw = fw->focusWidget() ? fw->focusWidget() : fw;
      fw->setFocus(Qt::OtherFocusReason);
    }
}


void 
QLuaMdiMain::Client::setupAsDockWidget()
{
  QLuaIde *ide = QLuaIde::instance();
  if (! widget)
    return;
  if (subWindow)
    disown();
  if (! dockWidget)
    {
      // special for qobjects without layout
      QWidget *w = widget;
      if (!w->layout() && w->metaObject() == &QWidget::staticMetaObject)
        w = new Shell(widget);
      // create
      dockWidget = new QDockWidget(widget->windowTitle(), d->q);
      QDockWidget::DockWidgetFeatures f = dockWidget->features();
      dockWidget->setFeatures(f | QDockWidget::DockWidgetVerticalTitleBar);
      dockWidget->setObjectName(widget->objectName());
      dockWidget->setWidget(w);
      d->table[dockWidget] = this;
      // mark widget as a subwindow (to isolate shortcuts)
      Qt::WindowFlags flags = widget->windowFlags() & ~Qt::WindowType_Mask;
      widget->setWindowFlags(flags | Qt::SubWindow);
      // callbacks
      dockWidget->installEventFilter(this);
      connect(dockWidget, SIGNAL(topLevelChanged(bool)),
              this, SLOT(toplevelChanged(bool)) );
      // show
      computeWindowTitle();
      d->q->addDockWidget(Qt::RightDockWidgetArea, dockWidget);
      ide->loadWindowGeometry(w);
      d->guiUpdateLater();
      widget->show();
      dockWidget->show();
      // set focus
      d->setActive(this);
      QWidget *fw = widget;
      fw = fw->focusWidget() ? fw->focusWidget() : fw;
      fw->setFocus(Qt::OtherFocusReason);
    }
}


void
QLuaMdiMain::Client::toplevelChanged(bool b)
{
  if (dockWidget)
    {
      dockWidget->setUpdatesEnabled(true);
      QDockWidget::DockWidgetFeatures flags = dockWidget->features();
      flags &= ~QDockWidget::DockWidgetVerticalTitleBar;
      if (! b)
        flags |= QDockWidget::DockWidgetVerticalTitleBar;
      dockWidget->setFeatures(flags);
    }
}


static QStatusBar*
extractStatusBar(QMainWindow *mw)
{
  // Making strong assumptions about mainwindow layout.
  QLayout *layout = mw->layout();
  QLayoutItem *item = 0;
  QStatusBar *sb = 0;
  if (layout)
    for (int i=0; (item = layout->itemAt(i)); i++)
      if ((sb = qobject_cast<QStatusBar*>(item->widget())))
        {
          item = layout->takeAt(i);
          sb->hide();
          sb->setParent(0);
          delete item;
          return sb;
        }
  return 0;
}


void 
QLuaMdiMain::Client::deActivate()
{
  // statusbar
  extractStatusBar(d->q);
  QStatusBar *sb = d->q->statusBar();
  d->q->setStatusBar(sb);
  sb->show();
}


void 
QLuaMdiMain::Client::reActivate()
{
  // statusbar
  extractStatusBar(d->q);
  QStatusBar *sb = d->q->statusBar();
  if (statusBar)
    sb = statusBar;
  d->q->setStatusBar(sb);
  sb->show();
}


void 
QLuaMdiMain::Client::stealStatusBar()
{
  QLuaMainWindow *main = qobject_cast<QLuaMainWindow*>(widget);
  if (main)
    statusBar = extractStatusBar(main);
}


void 
QLuaMdiMain::Client::stealMenuBar()
{
  QMainWindow *main = qobject_cast<QMainWindow*>(widget);
  if (main)
    {
      QMenuBar *mb = main->menuBar();
      if (mb && mb != menuBar)
        {
          menuBar = mb;
          mb->hide();
          mb->installEventFilter(this);
          d->guiUpdateLater();
        }
    }
}


bool
QLuaMdiMain::Client::copyMenuBar(QMenuBar *into)
{
  QMenuBar *orig = menuBar;
  if (!orig)
    return false;
  orig->hide();
  if (orig->actions().isEmpty())
    return false;
  foreach (QAction *action, orig->actions())
    {
#ifdef Q_WS_MAC
      // Qt/Mac menu merging fails unless we also copy submenus...
      QMenu *menu = action->menu();
      if (menu)
        {
          QMenu *newmenu = into->addMenu(menu->title());
          foreach (QAction *ma, menu->actions())
            newmenu->addAction(ma);
        }
      else
#endif
        into->addAction(action);
    }
  return true;
}





// ========================================
// QLUAMDIMAIN



QLuaMdiMain::~QLuaMdiMain()
{
  d->closingDown = true;
  while (d->clients.size())
    delete d->clients.takeFirst();
  setUpdatesEnabled(true);
}


QLuaMdiMain::QLuaMdiMain(QWidget *parent)
  : QLuaMainWindow("qLuaMdiMain", parent),
    d(new Private(this))
{
  installEventFilter(d);
  setCentralWidget(d->area);
  setWindowTitle(QApplication::applicationName());
  setDockOptions(QMainWindow::AnimatedDocks);
  menuBar();
  statusBar();
  loadSettings();
  setClientClass("QWidget");
}


QMdiArea* 
QLuaMdiMain::mdiArea()
{
  return d->area;
}


bool
QLuaMdiMain::isActive(QWidget *w)
{
  if (! isActiveWindow())
    return w->isActiveWindow();
  while (w && w->windowType() != Qt::Window)
    {
      if (d->table.contains(w))
        if (d->table[w] == d->active)
          return true;
      w = w->parentWidget();
    }
  return false;
}


bool 
QLuaMdiMain::canClose()
{
  QLuaIde *ide = QLuaIde::instance();
  // close everything if we contain sdimain.
  foreach(Client *c, d->clients)
    if (qobject_cast<QLuaSdiMain*>(c->widget))
      if (ide && !ide->close(this))
        return false;
  // otherwise close only our windows
  foreach(Client *c, d->clients)
    if (c->widget && !c->widget->close())
      return false;
  // done
  return true;
}


void 
QLuaMdiMain::loadSettings()
{
  QSettings s;
  restoreGeometry(s.value("ide/geometry").toByteArray());
}


void 
QLuaMdiMain::saveMdiSettings()
{
  QSettings s;
  s.setValue("ide/geometry", saveGeometry());
  s.setValue("ide/state", saveState());
  QStringList docked;
  foreach (Client *client, d->clients)
    if (client->widget && client->dockWidget)
      docked += client->widget->objectName();
  s.setValue("ide/dockedWindows", docked);
}


QAction *
QLuaMdiMain::addStdFileMenu(QMenuBar *menubar)
{
  if (!d->fileMenu)
    {
      QMenu *menu = new QMenu(tr("&File","file|"), this);
      addStdActionToMenu(ActionFileNew, menu);
      addStdActionToMenu(ActionFileOpen, menu);
      addStdActionToMenu(ActionFileOpenRecent, menu);
      menu->addSeparator();
      QAction *action = addStdActionToMenu(ActionFileClose, menu);
      disconnect(action, SIGNAL(triggered(bool)), 0, 0);
      connect(action, SIGNAL(triggered(bool)), 
              d->area, SLOT(closeActiveSubWindow()) );
      addStdActionToMenu(ActionFileQuit, menu);
      d->fileMenu = menu;
    }
  if (d->fileMenu)
    return menubar->addMenu(d->fileMenu);
  return 0;
}


QAction *
QLuaMdiMain::addStdEditMenu(QMenuBar *menubar)
{
  if (!d->editMenu)
    {
      QMenu *menu = new QMenu(tr("&Edit", "edit|"), this);
      addStdActionToMenu(ActionEditCut, menu)->setEnabled(false);
      addStdActionToMenu(ActionEditCopy, menu)->setEnabled(false);
      addStdActionToMenu(ActionEditPaste, menu)->setEnabled(false);
      menu->addSeparator();
      addStdActionToMenu(ActionPreferences, menu);
      d->editMenu = menu;
    }
  if (d->editMenu)
    return menubar->addMenu(d->editMenu);
  return 0;
}


QMenuBar*
QLuaMdiMain::createMenuBar()
{
  QMenuBar *menubar = new QMenuBar(this);
  addStdFileMenu(menubar);
  addStdEditMenu(menubar);
  addStdWindowMenu(menubar);
  addStdHelpMenu(menubar);
  return menubar;
}


QToolBar *
QLuaMdiMain::createToolBar()
{
  return 0;
}


QStatusBar *
QLuaMdiMain::createStatusBar()
{
  return new QStatusBar(this);
}


bool
QLuaMdiMain::activate(QWidget *w)
{
  Client *client = 0;
  QWidget *window = w->window();
  while (w && w->windowType() != Qt::Window 
         && !d->table.contains(w))
    w = w->parentWidget();
  if (d->table.contains(w))
    {
      window->show();
      window->raise();
      window->activateWindow();
      d->guiUpdateLater();
      Client *client = d->table[w];
      if (client->subWindow)
        client->subWindow->raise();
      w = w->focusWidget() ? w->focusWidget() : w;
      w->setFocus(Qt::OtherFocusReason);
      return true;
    }
  return false;
}


bool
QLuaMdiMain::adopt(QWidget *w)
{
  if (w && w != this)
    {
      if (!d->clientClass.isEmpty() && 
          !w->inherits(d->clientClass.constData()) )
        return false;
      show();
      Client *client = new Client(w, d);
      if (w->objectName().startsWith("qLuaSdiMain"))
        {
          QSettings s;
          QStringList docked = s.value("ide/dockedWindows").toStringList();
          if (docked.contains(w->objectName()))
            {
              client->setupAsDockWidget();
              QApplication::sendPostedEvents();
              restoreState(s.value("ide/state").toByteArray());
              return true;
            }
        }
      client->setupAsSubWindow();
      client->subWindow->showNormal();
      return true;
    }
  return false;
}


void 
QLuaMdiMain::adoptAll()
{
  QLuaIde *ide = QLuaIde::instance();
  foreach(QObject *o, ide->windows())
    adopt(qobject_cast<QWidget*>(o));
}


QAction *
QLuaMdiMain::tileAction()
{
  if (! d->tileAction)
    {
      d->tileAction = new QAction(tr("&Tile Windows"), this);
      connect(d->tileAction, SIGNAL(triggered(bool)), 
              d->area, SLOT(tileSubWindows()) );
    }
  return d->tileAction;
}


QAction *
QLuaMdiMain::cascadeAction()
{
  if (! d->cascadeAction)
    {
      d->cascadeAction = new QAction(tr("&Cascade Windows"), this);
      connect(d->cascadeAction, SIGNAL(triggered(bool)), 
              d->area, SLOT(cascadeSubWindows()) );
    }
  return d->cascadeAction;
}


QAction *
QLuaMdiMain::dockAction()
{
  if (! d->dockAction)
    {
      QAction *a = new QAction(tr("&Dock Window"), this);
      connect(a, SIGNAL(triggered(bool)), d, SLOT(dockCurrentWindow(bool)));
      a->setCheckable(true);
      d->dockAction =a;
    }
  return d->dockAction;
}


bool 
QLuaMdiMain::isTabMode() const
{
  return d->tabMode;
}


QByteArray 
QLuaMdiMain::clientClass() const
{
  return d->clientClass;
}


void 
QLuaMdiMain::setTabMode(bool b)
{
  if (b != d->tabMode)
    {
      d->tabMode = b;
      if (b) 
        d->area->setViewMode(QMdiArea::TabbedView);
      else
        d->area->setViewMode(QMdiArea::SubWindowView);
      foreach(Client *c, d->clients)
        c->computeWindowTitle();
    }
}


void
QLuaMdiMain::setClientClass(QByteArray c)
{
  d->clientClass = c;
}


void
QLuaMdiMain::doNew()
{
  QLuaEditor *n = QLuaIde::instance()->editor();
  n->widget()->setEditorMode("lua");
  n->updateActionsLater();
}




// ========================================
// MOC


#include "qluamdimain.moc"





/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */
