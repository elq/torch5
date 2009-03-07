// -*- C++ -*-

#include "qtluautils.h"
#include "qtluaengine.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <QCoreApplication>
#include <QLibrary>
#include <QLine>
#include <QLineF>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaType>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QStringList>
#include <QSize>
#include <QSizeF>
#include <QTimer>
#include <QVariant>


// ========================================
// More utilities



static int
luaQ_gettable(struct lua_State *L)
{
  luaL_checktype(L, -2, LUA_TTABLE);
  lua_gettable(L, -2);
  return 1;
}


/*! Same as lua_getfield but returns \a nil
  if an error occurs while processing the 
  metatable \a __index event. */

void
luaQ_getfield(struct lua_State *L, int index, const char *name)
{
  lua_pushvalue(L, index);
  lua_pushcfunction(L, luaQ_gettable);
  lua_insert(L, -2);
  lua_pushstring(L, name);
  lua_Hook hf = lua_gethook(L);
  int hm = lua_gethookmask(L);
  int hc = lua_gethookcount(L);
  lua_sethook(L, 0, 0, 0);
  if (lua_pcall(L, 2, 1, 0))
    {
      lua_pop(L, 1);
      lua_pushnil(L);
    }
  lua_sethook(L, hf, hm, hc);
}





// ========================================
// Error handlers




/*! Enrichs the message on the stack with a traceback.
  This function must be called with one string argument
  representing an error message for instance.
  It safely augments the string with a stack trace
  starting \a skip levels above the current level.
  This function does not cause an error.
  If something goes wrong during the stack exploration,
  the function silently returns the unchanged error message.
 */

int
luaQ_tracebackskip(struct lua_State *L, int skip)
{
  // stack: msg
  luaQ_getfield(L, LUA_GLOBALSINDEX, "debug");
  luaQ_getfield(L, -1, "traceback");
  // stack: traceback debug msg
  lua_remove(L, -2);
  // stack: traceback msg
  if (! lua_isfunction(L, -1)) 
    {
      lua_pop(L, 1);
    } 
  else 
    {
      lua_pushvalue(L, -2);
      lua_pushinteger(L, skip+1);
      // save hook
      lua_Hook hf = lua_gethook(L);
      int hm = lua_gethookmask(L);
      int hc = lua_gethookcount(L);
      lua_sethook(L, 0, 0, 0);
      // stack: skip msg traceback msg
      if (lua_pcall(L, 2, 1, 0))
        // stack: err msg
        lua_remove(L, -1);
      else
        // stack: msg msg
        lua_remove(L, -2);
      // restore hook
      lua_sethook(L, hf, hm, hc);
    }
  // stack: msg
  return 1;
}



/*! A safe error handler that enrichs
  the error message with a traceback. */

int
luaQ_traceback(struct lua_State *L)
{
  return luaQ_tracebackskip(L, 0);
}




// ========================================
// Completion



/*! Returns potential completion for a string.
  Takes as input a single string composed of 
  identifiers separated by '.' or ':' and returns a table 
  representing an array of potential completions.
  Each completion is a string that could be reasonably 
  appended to the initial argument.
  This function throws errors when something goes wrong.
  Use \a pcall to call it safely. */

int 
luaQ_complete(struct lua_State *L)
{
  int k = 0;
  int loop = 0;
  const char *stem = luaL_checkstring(L, 1);
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  for(;;)
    {
      const char *s = stem;
      while (*s && *s != '.' && *s != ':')
        s++;
      if (*s == 0)
        break;
      // stack: table str
      lua_pushlstring(L, stem, s-stem);
      lua_gettable(L, -2);
      // stack: ntable table str
      lua_replace(L, -2);
      // stack: ntable str
      stem = s + 1;
    }
  lua_createtable(L, 0, 0);
  lua_insert(L, -2);
  // stack: maybetable anstable str
  if (lua_isuserdata(L, -1) && lua_getmetatable(L, -1))
    {
      lua_replace(L, -2);
      lua_pushliteral(L, "__index");
      lua_rawget(L, -2);
      if (lua_isfunction(L, -1))
        {
          lua_pop(L, 1);
          lua_pushliteral(L, "__metatable");
          lua_rawget(L, -2);
        }
      lua_replace(L, -2);
    }
  if (! lua_istable(L, -1))
    {
      lua_pop(L, 1);
      return 1;
    }
  // stack: table anstable str
  size_t stemlen = strlen(stem);
  for(;;)
    {
      lua_pushnil(L);
      while (lua_next(L, -2))
        {
          // stack: value key table anstable str
          bool ok = false;
          size_t keylen;
          const char *key = lua_tolstring(L, -2, &keylen);
          if (key && keylen > 0 && keylen >= stemlen)
            if (!strncmp(key, stem, stemlen))
              ok = true;
          if (ok && !isalpha(key[0]))
            ok = false;
          if (ok)
            for (int i=0; ok && i<(int)keylen; i++)
              if (!isalpha(key[i]) && !isdigit(key[i]) && key[i]!='_')
                ok = false;
          if (ok)
            {
              const char *suffix = "";
              switch (lua_type(L, -1)) 
                {
                case LUA_TFUNCTION: 
                  suffix = "("; 
                  break;
                case LUA_TTABLE: 
                  suffix = ".";
                  luaQ_getfield(L, -1, "_C");
                  if (lua_istable(L, -1))
                    suffix = ":";
                  lua_pop(L, 1);
                  break;
                case LUA_TUSERDATA: {
                  QVariant v = luaQ_toqvariant(L, -1);
                  const char *s = QMetaType::typeName(v.userType());
                  if (s && !strcmp(s,"QtLuaMethodInfo"))
                    suffix = "(";
                  else if (s && !strcmp(s, "QtLuaPropertyInfo"))
                    suffix = "";
                  else if (lua_getmetatable(L, -1)) {
                    lua_pop(L, 1);
                    suffix = ":";
                  } else 
                    suffix = "";
                } break;
                default:
                  break;
                }
              // stack: value key table anstable str
              lua_pushfstring(L, "%s%s", key+stemlen, suffix);
              lua_rawseti(L, -5, ++k);
            }
          lua_pop(L, 1);
        }
      // stack: table anstable str
      if (! lua_getmetatable(L, -1))
        break;
      lua_replace(L, -2);
      lua_pushliteral(L, "__index");
      lua_rawget(L, -2);
      lua_replace(L, -2);
      if (! lua_istable(L, -1))
        break;
      if (++loop > 100)
        luaL_error(L, "complete: infinite loop in metatables");
    }
  // stack: something anstable str
  lua_pop(L, 1);
  lua_replace(L, -2);
  return 1;
}




// ========================================
// Printing



static int
simple_print(lua_State *L)
{
  int i;
  int nr = lua_gettop(L);
  for (i=1; i<=nr; i++)
    {
      const char *s = lua_tostring(L, i);
      printf("%s", s ? s : "???");
      printf("%c", i<nr ? '\t' : '\n');
    }
  return 0;
}


int
luaQ_print(lua_State *L, int nr)
{
  int base = lua_gettop(L);
  nr = qMin(lua_gettop(L), nr);
  if (nr <= 0)
    return 0; 
  lua_getglobal(L, "print");
  if (lua_type(L, -1) != LUA_TFUNCTION)
    {
      lua_pop(L, 1);
      lua_pushcfunction(L, simple_print);
    }
  lua_checkstack(L, nr);
  for (int i=base-nr+1; i<=base; i++)
    lua_pushvalue(L, i);
  if (lua_pcall(L, nr, 0, 0))
    {
      const char *err = "error object is not a string";
      if (lua_isstring(L, -1))
        err = lua_tostring(L, -1);
      printf("error calling 'print' (%s)\n", err);
      lua_pop(L, 1);
    }
  return nr;
}




// ========================================
// Cross-thread calls



int 
luaQ_pcall(lua_State *L, int na, int nr, int eh, int oh)
{
  QtLuaEngine *engine = luaQ_engine(L);
  QObject *obj = engine;
  if (oh)
    obj = luaQ_toqobject(L, oh);
  if (! obj)
    luaL_error(L, "invalid qobject");
  return luaQ_pcall(L, na, nr, eh, obj);
}




// ========================================
// Hooks 


// ------------------------------
// qt <--> table


static void
f_checktype(lua_State *L, int index, const char *name, int type)
{ 
  if (index)
    lua_getfield(L,index,name); 
  int t = lua_type(L, -1);
  if (t != type)
    luaL_error(L, "%s expected in field " LUA_QS ", got " LUA_QS,
               lua_typename(L, type), name, lua_typename(L, t));
}

static bool
f_opttype(lua_State *L, int index, const char *name, int type)
{ 
  lua_getfield(L,index,name); 
  if (lua_isnoneornil(L, -1))
    return false;
  f_checktype(L, 0, name, type);
  return true;
}



#define fromtable_bool(n,get,set) \
  { f_checktype(L, -1, n, LUA_TBOOLEAN); \
    bool x = lua_toboolean(L, -1); set; \
    lua_pop(L, 1); }

#define fromtable_int(n,get,set) \
  { f_checktype(L, -1, n, LUA_TNUMBER); \
    lua_Integer x = lua_tointeger(L, -1); set; \
    lua_pop(L, 1); }

#define fromtable_flt(n,get,set) \
  { f_checktype(L, -1, n, LUA_TNUMBER); \
    lua_Number x = lua_tonumber(L, -1); set; \
    lua_pop(L, 1); }

#define fromtable_str(n,get,set) \
  { f_checktype(L, -1, n, LUA_TSTRING); \
    QString x = QString::fromLocal8Bit(lua_tostring(L, -1)); set; \
    lua_pop(L, 1); }

#define fromtable_optbool(n,get,set) \
  { if (f_opttype(L, -1, n, LUA_TBOOLEAN)) { \
      bool x = lua_toboolean(L, -1); set; } \
    lua_pop(L, 1); }

#define fromtable_optint(n,get,set) \
  { if (f_opttype(L, -1, n, LUA_TNUMBER)) { \
      lua_Integer x = lua_tointeger(L, -1); set; } \
    lua_pop(L, 1); }

#define fromtable_optflt(n,get,set) \
  { if (f_opttype(L, -1, n, LUA_TNUMBER)) { \
      lua_Number x = lua_tonumber(L, -1); set; } \
    lua_pop(L, 1); }

#define do_fromtable(T,t,do,declare,construct) \
static int t ## _fromtable(lua_State *L) \
{ \
  declare; \
  if (! lua_isnoneornil(L, 1)) { \
    luaL_checktype(L, 1, LUA_TTABLE); \
    do(fromtable_) } \
  luaQ_pushqt(L, QVariant(construct)); \
  return 1; \
}


#define totable_bool(n,get,set) \
  { bool x; get; \
    lua_pushboolean(L,x); \
    lua_setfield(L, -2, n); }

#define totable_int(n,get,set) \
  { lua_Integer x; get; \
    lua_pushinteger(L,x); \
    lua_setfield(L, -2, n); }

#define totable_flt(n,get,set) \
  { lua_Number x; get; \
    lua_pushnumber(L,x); \
    lua_setfield(L, -2, n); }

#define totable_optbool(n,get,set) \
  totable_bool(n,get,set)

#define totable_optint(n,get,set) \
  totable_int(n,get,set)

#define totable_optflt(n,get,set) \
  totable_flt(n,get,set)

#define totable_optstr(n,get,set) \
  totable_str(n,get,set)

#define do_totable(T,t,do) \
static int t ## _totable(lua_State *L) \
{ \
  T s = luaQ_checkqvariant<T>(L, 1); \
  lua_createtable(L, 0, 2); \
  do(totable_) \
  return 1; \
}


#define do_luareg(t) \
static struct luaL_Reg t ## _lib[] = {\
  {"totable", t ## _totable }, \
  {"new", t ## _fromtable }, \
  {0,0} \
}; \

#define do_hook(t) \
static int t ## _hook(lua_State *L) \
{ \
  lua_getfield(L, -1, "__metatable"); \
  luaL_register(L, 0, t ## _lib); \
  return 0; \
}


#define do_all(T,t,do) \
  do_totable(T,t,do) \
  do_fromtable(T,t,do,T s,s) \
  do_luareg(t) \
  do_hook(t)




// ------------------------------
// qsize, qpoint, qrect, qline
// qsizef, qpointf, qrectf, qlinef


#define do_qsize(do) \
  do ## int("width",x=s.width(),s.rwidth()=x) \
  do ## int("height",x=s.height(),s.rheight()=x)

do_all(QSize,qsize,do_qsize)


#define do_qsizef(do) \
  do ## flt("width",x=s.width(),s.rwidth()=x) \
  do ## flt("height",x=s.height(),s.rheight()=x)

do_all(QSizeF,qsizef,do_qsizef)


#define do_qpoint(do) \
  do ## int("x",x=s.x(),s.rx()=x) \
  do ## int("y",x=s.y(),s.ry()=x)

do_all(QPoint,qpoint,do_qpoint)


#define do_qpointf(do) \
  do ## flt("x",x=s.x(),s.rx()=x) \
  do ## flt("y",x=s.y(),s.ry()=x)

do_all(QPointF,qpointf,do_qpointf)


#define do_qrect(do) \
  do ## int("x",x=s.x(),s.setLeft(x)) \
  do ## int("y",x=s.y(),s.setTop(x)) \
  do ## int("width",x=s.width(),s.setWidth(x)) \
  do ## int("height",x=s.height(),s.setHeight(x))

do_all(QRect,qrect,do_qrect)


#define do_qrectf(do) \
  do ## flt("x",x=s.x(),s.setLeft(x)) \
  do ## flt("y",x=s.y(),s.setTop(x)) \
  do ## flt("width",x=s.width(),s.setWidth(x)) \
  do ## flt("height",x=s.height(),s.setHeight(x))

do_all(QRectF,qrectf,do_qrectf)


#define do_qline(do) \
  do ## int("x1",x=s.x1(),s[0]=x) \
  do ## int("y1",x=s.y1(),s[1]=x) \
  do ## int("x2",x=s.x2(),s[2]=x) \
  do ## int("y2",x=s.y2(),s[3]=x)

do_totable(QLine,qline,do_qline)
do_fromtable(QLine,qline,do_qline,
             int s[4];s[0]=s[1]=s[2]=s[3]=0,
             QLine(s[0],s[1],s[2],s[3]))
do_luareg(qline)
do_hook(qline)


#define do_qlinef(do) \
  do ## flt("x1",x=s.x1(),s[0]=x) \
  do ## flt("y1",x=s.y1(),s[1]=x) \
  do ## flt("x2",x=s.x2(),s[2]=x) \
  do ## flt("y2",x=s.y2(),s[3]=x)

do_totable(QLineF,qlinef,do_qlinef)
do_fromtable(QLineF,qlinef,do_qlinef,
             qreal s[4];s[0]=s[1]=s[2]=s[3]=0,
             QLineF(s[0],s[1],s[2],s[3]))
do_luareg(qlinef)
do_hook(qlinef)




// ------------------------------
// qstringlist


static int
qstringlist_totable(lua_State *L)
{
  QStringList l = luaQ_checkqvariant<QStringList>(L, 1);
  lua_createtable(L, l.size(), 0);
  for (int i=0; i<l.size(); i++)
    {
      luaQ_pushqt(L, QVariant(l[i]));
      lua_rawseti(L, -2, i+1);
    }
  return 1;
}


static int
qstringlist_new(lua_State *L)
{
  QStringList l;
  if (! lua_isnone(L, 1))
    {
      luaL_checktype(L, 1, LUA_TTABLE);
      int n = lua_objlen(L, 1);
      for (int i=1; i<=n; i++)
        {
          lua_rawgeti(L, 1, i);
          QVariant v = luaQ_toqvariant(L, -1, QMetaType::QString);
          if (v.userType() != QMetaType::QString)
            luaL_error(L, "table element cannot be converted to a QString");
          l += v.toString();
          lua_pop(L, 1);
        }
    }
  luaQ_pushqt(L, QVariant(l));
  return 1;
}


static const luaL_Reg qstringlist_lib[] = {
  {"totable", qstringlist_totable},
  {"new", qstringlist_new},
  {0, 0}
};


static int
qstringlist_hook(lua_State *L)
{
  lua_getfield(L, -1, "__metatable");
  luaL_register(L, 0, qstringlist_lib);
  return 0;
}


// ------------------------------
// qvariantlist


static int
qvariantlist_totable(lua_State *L)
{
  QVariantList l = luaQ_checkqvariant<QVariantList>(L, 1);
  lua_createtable(L, l.size(), 0);
  for (int i=0; i<l.size(); i++)
    {
      luaQ_pushqt(L, QVariant(l[i]));
      lua_rawseti(L, -2, i+1);
    }
  return 1;
}


static int
qvariantlist_new(lua_State *L)
{
  QVariantList l;
  if (! lua_isnone(L, 1))
    {
      luaL_checktype(L, 1, LUA_TTABLE);
      int n = lua_objlen(L, 1);
      for (int i=1; i<=n; i++)
        {
          lua_rawgeti(L, 1, i);
          l += luaQ_toqvariant(L, -1);
          lua_pop(L, 1);
        }
    }
  luaQ_pushqt(L, QVariant(l));
  return 1;
}


static const luaL_Reg qvariantlist_lib[] = {
  {"totable", qvariantlist_totable},
  {"new", qvariantlist_new},
  {0, 0}
};


static int
qvariantlist_hook(lua_State *L)
{
  lua_getfield(L, -1, "__metatable");
  luaL_register(L, 0, qvariantlist_lib);
  return 0;
}



// ------------------------------
// qvariantmap


static int
qvariantmap_totable(lua_State *L)
{
  QVariantMap m = luaQ_checkqvariant<QVariantMap>(L, 1);
  lua_createtable(L, 0, 0);
  for (QVariantMap::const_iterator it=m.constBegin(); it!=m.constEnd(); ++it)
    {
      lua_pushstring(L, it.key().toLocal8Bit().constData());
      luaQ_pushqt(L, it.value());
      lua_rawset(L, -3);
    }
  return 1;
}


static int
qvariantmap_new(lua_State *L)
{
  QVariantMap l;
  if (! lua_isnone(L, 1))
    {
      luaL_checktype(L, 1, LUA_TTABLE);
      lua_pushnil(L);
      while (lua_next(L, -2))
        {
          QVariant k = luaQ_toqvariant(L, -2, QMetaType::QString);
          if (k.userType() != QMetaType::QString)
            luaL_error(L, "table element cannot be converted to a QString");
          l[k.toString()] = luaQ_toqvariant(L, -1);
          lua_pop(L, 1);
        }
    }
  luaQ_pushqt(L, QVariant(l));
  return 1;
}


static const luaL_Reg qvariantmap_lib[] = {
  {"totable", qvariantmap_totable},
  {"new", qvariantmap_new},
  {0, 0}
};


static int
qvariantmap_hook(lua_State *L)
{
  lua_getfield(L, -1, "__metatable");
  luaL_register(L, 0, qvariantmap_lib);
  return 0;
}



// ------------------------------
// QObject


static int qobject_parent(lua_State *L)
{
  QObject *w = luaQ_checkqobject<QObject>(L, 1);
  luaQ_pushqt(L, w->parent());
  return 1;
}


static int qobject_children(lua_State *L)
{
  QObject *w = luaQ_checkqobject<QObject>(L, 1);
  QVariantList v;
  QObjectPointer p;
  foreach(p, w->children())
    v << qVariantFromValue<QObjectPointer>(p);
  luaQ_pushqt(L, v);
  return 1;
}


static int qobject_dumpobjectinfo(lua_State *L)
{
  QObject *w = luaQ_checkqobject<QObject>(L, 1);
  w->dumpObjectInfo();
  return 0;
}


static int qobject_dumpobjecttree(lua_State *L)
{
  QObject *w = luaQ_checkqobject<QObject>(L, 1);
  w->dumpObjectTree();
  return 0;
}


static struct luaL_Reg qobject_lib[] = {
  {"parent", qobject_parent},
  {"children", qobject_children},
  {"dumpObjectInfo", qobject_dumpobjectinfo},
  {"dumpObjectTree", qobject_dumpobjecttree},
  {0,0}
};


static int qobject_hook(lua_State *L) 
{
  lua_getfield(L, -1, "__metatable");
  luaQ_register(L, qobject_lib, 0);
  return 0;
}



// ------------------------------
// QTimer


static int
qtimer_new(lua_State *L)
{
  QObject *parent = luaQ_optqobject<QObject>(L, 1);
  QTimer *t = new QTimer(parent);
  luaQ_pushqt(L, t, !parent);
  return 1;
}

static const luaL_Reg qtimer_lib[] = {
  {"new", qtimer_new},
  {0,0}
};

static int
qtimer_hook(lua_State *L)
{
  lua_getfield(L, -1, "__metatable");
  luaL_register(L, 0, qtimer_lib);
  return 0;
}


// ------------------------------
// QtLuaEngine, QCoreApplication
// hide deleteLater()


static int
no_methodcall(lua_State *L)
{
  luaL_error(L, "This class prevents lua to call this method");
  return 0;
}

static int
no_delete_hook(lua_State *L)
{
  // ..stack: metatable
  lua_getfield(L, -1, "__metatable");
  // ..stack: metaclass
  lua_pushcfunction(L, no_methodcall);
  lua_setfield(L, -2, "deleteLater");
  lua_pushcfunction(L, no_methodcall);
  lua_setfield(L, -2, "deleteLater(QObject*)");
  return 0;
}







// ========================================
// Qt library functions



static int
qt_connect(lua_State *L)
{
  // LUA: "qt.connect(object signal closure)" 
  // Connects signal to closure.
  
  // LUA: "qt.connect(object signal object signal_or_slot)"
  // Connects signal to signal or slot.
  
  QObject *obj = luaQ_checkqobject<QObject>(L, 1);
  const char *sig = luaL_checkstring(L, 2);
  QObject *robj = luaQ_toqobject(L, 3);
  if (robj)
    {
      // search signal or slot
      QByteArray rsig = luaL_checkstring(L, 4);
      const QMetaObject *mo = robj->metaObject();
      int idx = mo->indexOfMethod(rsig.constData());
      if (idx < 0)
        {
          rsig = QMetaObject::normalizedSignature(rsig.constData());
          idx = mo->indexOfMethod(rsig.constData());
          if (idx < 0)
            luaL_error(L, "cannot find target slot or signal %s", 
                       rsig.constData());
        }
      // prepend signal or slot indicator
      QMetaMethod method = mo->method(idx);
      if (method.methodType() == QMetaMethod::Signal)
        rsig.prepend('0' + QSIGNAL_CODE);
      else if (method.methodType() == QMetaMethod::Slot)
        rsig.prepend('0' + QSLOT_CODE);
      else
        luaL_error(L, "target %s is not a slot or a signal",
                   rsig.constData());
      // connect
      QByteArray ssig = sig;
      ssig.prepend('0' + QSIGNAL_CODE);
      if (! QObject::connect(obj, ssig.constData(), robj, rsig.constData()))
        luaL_error(L, "cannot find source signal %s", sig);
    }
  else
    {
      luaL_checktype(L, 3, LUA_TFUNCTION);
      if (! luaQ_connect(L, obj, sig, 3))
        luaL_error(L, "cannot find source signal %s", sig);
    }
  return 0;
}


static int
qt_disconnect(lua_State *L)
{
  // LUA: qt.disconnect(object [signal [closure]])
  // LUA: qt.disconnect(object signal object signal_or_slot)
  // Disconnect all connections between 
  // the specified signal and a lua function.
  // Returns boolean indicating if such signal were found.
  bool ok = false;
  QObject *obj = luaQ_checkqobject<QObject>(L, 1);
  const char *sig = luaL_optstring(L, 2, 0);
  int narg = lua_gettop(L);
  if (narg>3 || lua_isuserdata(L, 3))
    {
      QObject *robj = luaQ_optqobject<QObject>(L, 3, 0);
      const char *rsig = luaL_optstring(L, 4, 0);
      QByteArray bsig(sig);
      QByteArray brsig(rsig);
      bsig.prepend('0'+QSIGNAL_CODE);
      brsig.prepend('0'+QSLOT_CODE);
      sig = (sig) ? bsig.constData() : 0;
      rsig = (rsig) ? brsig.constData() : 0;
      ok = QObject::disconnect(obj, sig, robj, rsig);
      brsig[0] = '0' + QSIGNAL_CODE;
      if (rsig)
        ok |= QObject::disconnect(obj, sig, robj, brsig.constData());
    }
  else
    {
      int findex = 3;
      if (lua_isnoneornil(L, 3))
        findex = 0;
      else if (! lua_isfunction(L, 3))
        luaL_typerror(L, 3, "function");
      ok = luaQ_disconnect(L, obj, sig, findex);
    }
  lua_pushboolean(L, ok);
  return 1;
}


static int
qt_doevents(lua_State *L)
{
  luaQ_doevents(L);
  return 0;
}


static int
qt_qcall(lua_State *L)
{
  QObject *obj = luaQ_checkqobject<QObject>(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  int base = 2;
  int narg = lua_gettop(L) - base;
  int status = luaQ_pcall(L, narg, LUA_MULTRET, 0, obj);
  lua_pushboolean(L, !status);
  lua_insert(L, base);
  return lua_gettop(L) - base + 1;
}


static int
qt_xqcall(lua_State *L)
{
  QObject *obj = luaQ_checkqobject<QObject>(L, 1);
  lua_insert(L, 2);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  luaL_checktype(L, 3, LUA_TFUNCTION);
  int base = 3;
  int narg = lua_gettop(L) - base;
  int status = luaQ_pcall(L, narg, LUA_MULTRET, 2, obj);
  lua_pushboolean(L, !status);
  lua_insert(L, base);
  return lua_gettop(L) - base + 1;
}


static int
qt_type(lua_State *L)
{
  QVariant v;
  if (lua_type(L, 1) == LUA_TUSERDATA)
    v = luaQ_toqvariant(L, 1);
  if (v.type() != QVariant::Invalid)
    {
      lua_pushvalue(L, 1);
      lua_getfield(L, -1, "type");
      if (lua_isfunction(L, -1))
        {
          lua_insert(L, -2);
          lua_call(L, 1, 1);
          return 1;
        }
    }
  lua_pushnil(L);
  return 1;
}


static int
qt_isa(lua_State *L)
{
  QVariant v;
  if (lua_type(L, 1) == LUA_TUSERDATA)
    v = luaQ_toqvariant(L, 1);
  if (v.type() != QVariant::Invalid)
    {
      lua_pushvalue(L, 1);
      lua_getfield(L, -1, "isa");
      if (lua_isfunction(L, -1))
        {
          lua_insert(L, 1);
          lua_call(L, lua_gettop(L)-1, 1);
          return 1;
        }
    }
  lua_pushnil(L);
  return 1;
}



// {{{ functions copied or derived from loadlib.c

static int readable (const char *filename) 
{  
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}

static const char *pushnexttemplate (lua_State *L, const char *path) 
{
  const char *l;
  while (*path == *LUA_PATHSEP) path++;  /* skip separators */
  if (*path == '\0') return NULL;  /* no more templates */
  l = strchr(path, *LUA_PATHSEP);  /* find next separator */
  if (l == NULL) l = path + strlen(path);
  lua_pushlstring(L, path, l - path);  /* template */
  return l;
}

static const char *pushfilename (lua_State *L, const char *name) 
{
  const char *path;
  const char *filename;
  luaQ_getfield(L, LUA_GLOBALSINDEX, "package");
  luaQ_getfield(L, -1, "cpath");
  lua_remove(L, -2);
  if (! (path = lua_tostring(L, -1)))
    luaL_error(L, LUA_QL("package.cpath") " must be a string");
  lua_pushliteral(L, ""); 
  while ((path = pushnexttemplate(L, path))) {
    filename = luaL_gsub(L, lua_tostring(L, -1), "?", name);
    lua_remove(L, -2);
    if (readable(filename))
      { // stack:  cpath errmsg filename
        lua_remove(L, -3);
        lua_remove(L, -2);
        return lua_tostring(L, -1);
      }
    lua_pushfstring(L, "\n\tno file " LUA_QS, filename);
    lua_remove(L, -2); /* remove file name */
    lua_concat(L, 2);  /* add entry to possible error message */
  }
  lua_pushfstring(L, "module" LUA_QS "not found", name);
  lua_replace(L, -3);
  lua_concat(L, 2);
  lua_error(L);
  return 0;
}

// functions copied or derived from loadlib.c }}}

static int
qt_require(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  lua_settop(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");  // index 2
  lua_getfield(L, 2, name);
  if (lua_toboolean(L, -1))
    return 1;
  const char *filename = pushfilename(L, name);  // index 3
  QLibrary library(QString::fromLocal8Bit(filename));
  if (! library.load())
    luaL_error(L, "cannot load " LUA_QS, filename);
  lua_pushfstring(L, "luaopen_%s", name);  // index 4
  lua_CFunction func = (lua_CFunction)library.resolve(lua_tostring(L, -1));
  if (! func)
    luaL_error(L, "no symbol " LUA_QS " in module " LUA_QS, 
               lua_tostring(L, -1), filename);
  lua_pushboolean(L, 1);
  lua_setfield(L, 2, name);
  lua_pushcfunction(L, func);
  lua_pushstring(L, name);
  lua_call(L, 1, 1);
  if (! lua_isnil(L, -1))
    lua_setfield(L, 2, name);
  lua_getfield(L, 2, name);
  return 1;
}


static const luaL_Reg qt_lib[] = {
  {"connect", qt_connect},
  {"disconnect", qt_disconnect},
  {"doevents", qt_doevents},
  {"qcall", qt_qcall},
  {"xqcall", qt_xqcall},
  {"type", qt_type},
  {"typename", qt_type},
  {"isa", qt_isa},
  {"require", qt_require},
  {0, 0}
};


static int
qt_m__index(lua_State *L)
{
  const char *s = luaL_checkstring(L, 2);
  QtLuaEngine *engine = luaQ_engine(L);
  QObject *obj = engine->namedObject(s);
  if (obj)
    luaQ_pushqt(L, obj);
  else
    lua_pushnil(L);
  return 1;
}

int  
luaopen_qt(lua_State *L)
{
  const char *qt = luaL_optstring(L, 1, "qt");
  luaQ_pushqt(L);
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_GLOBALSINDEX, qt);
  luaL_register(L, qt, qt_lib);

  // add a metatable with __index
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, qt_m__index);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);

  // hooks for objects
#define HOOK(T, t) \
     lua_pushcfunction(L, t ## _hook);\
     luaQ_pushmeta(L, &T::staticMetaObject);\
     lua_call(L, 1, 0);
  
  HOOK(QObject, qobject)
  HOOK(QTimer, qtimer)
  HOOK(QtLuaEngine, no_delete)
  HOOK(QCoreApplication, no_delete)
#undef HOOK

  // hooks for qvariants
#define HOOK(T, t) \
     lua_pushcfunction(L, t ## _hook);\
     luaQ_pushmeta(L, QMetaType::T);\
     lua_call(L, 1, 0)
  
  HOOK(QStringList, qstringlist);
  HOOK(QVariantList, qvariantlist);
  HOOK(QVariantMap, qvariantmap);
  HOOK(QSize,qsize);
  HOOK(QSizeF,qsizef);
  HOOK(QPoint,qpoint);
  HOOK(QPointF,qpointf);
  HOOK(QRect,qrect);
  HOOK(QRectF,qrectf);
  HOOK(QLine,qline);
  HOOK(QLineF,qlinef);
#undef HOOK

  return 1;
}





/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */


