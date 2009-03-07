/* -*- C++ -*- */


#include "qtwidget.h"

#include "qtlualistener.h"
#include "qtluapainter.h"
#include "qtluaprinter.h"

#include "lauxlib.h"
#include "lualib.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QGradient>
#include <QImage>
#include <QImageWriter>
#include <QMetaEnum>
#include <QMetaObject>
#include <QMetaType>
#include <QObject>
#include <QPen>
#include <QTransform>
#include <QVariant>
#include <QVector>


Q_DECLARE_METATYPE(QGradient)
Q_DECLARE_METATYPE(QPainterPath)
Q_DECLARE_METATYPE(QPolygon)
Q_DECLARE_METATYPE(QPolygonF)



// ========================================
// HELPERS FOR COMMON CLASSES

  
static QMetaEnum 
f_enumerator(const char *s, const QMetaObject *mo)
{
  int index = mo->indexOfEnumerator(s);
  if (mo >= 0)
    return mo->enumerator(index);
  return QMetaEnum();
}


#ifndef Q_MOC_RUN
static QMetaEnum 
f_enumerator(const char *s)
{
  struct QFakeObject : public QObject {
    static const QMetaObject* qt() { return &staticQtMetaObject; } };
  return f_enumerator(s, QFakeObject::qt());
}
#endif


static void
f_checktype(lua_State *L, int index, const char *name, int type)
{ 
  if (index)
    lua_getfield(L,index,name); 
  int t = lua_type(L, -1);
  if (t != type)
    luaL_error(L, "%s expected in field '%s', got %s",
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

static void
f_pushflag(lua_State *L, int value, const QMetaEnum &me)
{
  QByteArray b;
  if (me.isValid() && me.isFlag())
    b = me.valueToKeys(value);
  else if (me.isValid())
    b = me.valueToKey(value);
  if (b.size() > 0)
    lua_pushstring(L, b.constData());
  else
    lua_pushinteger(L, value);
}

static void
f_checkflag(lua_State *L, int index, const char *name, const QMetaEnum &me)
{ 
  if (index)
    lua_getfield(L,index,name); 
  if (me.isValid() && lua_isstring(L, -1))
    {
      const char *s = lua_tostring(L, -1);
      int v = (me.isFlag()) ? me.keysToValue(s) : me.keyToValue(s);
      if (v == -1)
        luaL_error(L, "value '%s' from field '%s' illegal for type %s::%s", 
                   s, name, me.scope(), me.name() );
      lua_pushinteger(L, v);
      lua_replace(L, -2);
    }
  if (! lua_isnumber(L, -1))
    {
      if (! me.isValid())
        luaL_error(L, "integer expected in field '%s'", name);
      luaL_error(L, "%s::%s or integer expected in field '%s'",
                 me.scope(), me.name(), name);
    }
}

static bool
f_optflag(lua_State *L, int index, const char *name, const QMetaEnum &me)
{ 
  lua_getfield(L,index,name); 
  if (lua_isnoneornil(L, -1))
    return false;
  f_checkflag(L, 0, name, me);
  return true;
}

static void
f_checkvar(lua_State *L, int index, const char *name, int tid)
{ 
  if (index)
    lua_getfield(L,index,name); 
  QVariant v = luaQ_toqvariant(L, -1, tid);
  if (v.userType() != tid)
    luaL_error(L, "qt.%s expected in field '%s'", QMetaType::typeName(tid));
}

static bool
f_optvar(lua_State *L, int index, const char *name, int tid)
{ 
  lua_getfield(L,index,name); 
  if (lua_isnoneornil(L, -1))
    return false;
  f_checkvar(L, 0, name, tid);
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

#define fromtable_optstr(n,get,set) \
  { if (f_opttype(L, -1, n, LUA_TSTRING)) { \
      QString x = QString::fromLocal8Bit(lua_tostring(L, -1)); set; } \
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

#define totable_str(n,get,set) \
  { QString x; get; \
    lua_pushstring(L,x.toLocal8Bit().constData()); \
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





// ========================================
// QCOLOR


#define do_qcolor(do) \
  do ## flt("r",x=s.redF(),s[0]=x) \
  do ## flt("g",x=s.greenF(),s[1]=x) \
  do ## flt("b",x=s.blueF(),s[2]=x) \
  do ## optflt("a",x=s.alphaF(),s[3]=x)

do_totable(QColor,qcolor,do_qcolor)

static int
qcolor_fromtable(lua_State *L)
{
  QColor c;
  if (lua_gettop(L)>=3) {
    c.setRgbF(luaL_checknumber(L, 1),luaL_checknumber(L, 2),
              luaL_checknumber(L, 3),luaL_optnumber(L, 4, 1.0));
  } else if (lua_isstring(L,1)) {
    c.setNamedColor(lua_tostring(L,1));
  } else {
    qreal s[4] = {0,0,0,1};
    do_qcolor(fromtable_)
    c.setRgbF(s[0],s[1],s[2],s[3]);
  }
  luaQ_pushqt(L, QVariant(c));
  return 1;
}
  
do_luareg(qcolor)
do_hook(qcolor)



// ========================================
// QFONT
  

#define do_qfont(do) \
  do ## optstr("family",x=s.family(),s.setFamily(x)) \
  do ## optflt("pointSize",x=s.pointSizeF(), if (x>0) s.setPointSizeF(x)) \
  do ## optint("pixelSize",x=s.pixelSize(), if (x>0) s.setPixelSize(x)) \
  do ## optint("size",x=s.pixelSize(), if (x>0) s.setPixelSize(x)) \
  do ## optbool("bold",x=s.bold(),if (x) s.setBold(true)) \
  do ## optbool("italic",x=s.italic(),if (x) s.setItalic(true)) \
  do ## optbool("underline",x=s.underline(),s.setUnderline(x)) \
  do ## optbool("overline",x=s.overline(), s.setOverline(x)) \
  do ## optbool("strikeOut",x=s.strikeOut(),s.setStrikeOut(x)) \
  do ## optbool("fixedPitch",x=s.fixedPitch(),s.setFixedPitch(x)) \
  do ## optint("weight",x=s.weight(),s.setWeight(x)) \
  do ## optint("stretch",x=s.stretch(),s.setStretch(x)) \
  do ## optbool("typewriter",x=(s.styleHint()==QFont::TypeWriter),\
                if (x) s.setStyleHint(QFont::TypeWriter,QFont::PreferMatch)) \
  do ## optbool("serif",x=(s.styleHint()==QFont::Serif),\
                if (x) s.setStyleHint(QFont::Serif))\
  do ## optbool("sans",x=(s.styleHint()==QFont::SansSerif),\
                if (x) s.setStyleHint(QFont::SansSerif))

static int
qfont_tostring(lua_State *L)
{
  QFont f = luaQ_checkqvariant<QFont>(L, 1);
  QString s = f.toString();
  lua_pushstring(L, s.toLocal8Bit().constData());
  return 1;
}

do_totable(QFont,qfont,do_qfont)

static int
qfont_fromtable(lua_State *L)
{
  QFont s;
  if (! lua_isstring(L, 1)) {
    s.setFamily("");
    do_qfont(fromtable_)
  } else
    s.fromString(QString::fromLocal8Bit(lua_tostring(L, 1)));
  luaQ_pushqt(L, QVariant(s));
  return 1;
}

static struct luaL_Reg qfont_lib[] = {
  {"tostring", qfont_tostring },
  {"totable", qfont_totable },
  {"new", qfont_fromtable }, 
  {0,0} 
}; 

do_hook(qfont)



// ========================================
// QBRUSH


static int
qbrush_totable(lua_State *L)
{
  QBrush s = luaQ_checkqvariant<QBrush>(L, 1);
  lua_createtable(L, 0, 0);
  QMetaEnum m_style = f_enumerator("BrushStyle");
  Qt::BrushStyle style = s.style();
  f_pushflag(L, (int)style, m_style);
  lua_setfield(L, -2, "style");
  QVariant v;
  switch (style) 
    {
    case Qt::LinearGradientPattern:
    case Qt::ConicalGradientPattern:
    case Qt::RadialGradientPattern:
      v = qVariantFromValue<QGradient>(*s.gradient());
      luaQ_pushqt(L, v);
      lua_setfield(L, -2, "gradient");
      break;
    case Qt::TexturePattern:
      v = QVariant(s.textureImage());
      luaQ_pushqt(L, v);
      lua_setfield(L, -2, "texture");
      if (qVariantValue<QImage>(v).depth() > 1) 
        break;
    default:
      v = QVariant(s.color());
      luaQ_pushqt(L, v);
      lua_setfield(L, -2, "color");
    case Qt::NoBrush:
      break;
    }
  QTransform t = s.transform();
  if (t.isIdentity())
    return 1;
  luaQ_pushqt(L, QVariant(t));
  lua_setfield(L, -2, "transform");
  return 1;
}


static int
create_textured_brush(lua_State *L)
{
  QBrush brush(luaQ_checkqvariant<QImage>(L, 1));
  luaQ_pushqt(L, brush);
  return 1;
}


static int
qbrush_fromtable(lua_State *L)
{
  QBrush s;
  if (! lua_isnoneornil(L, 1)) 
    {
      luaL_checktype(L, 1, LUA_TTABLE);
      QMetaEnum m_style = f_enumerator("BrushStyle");
      const int t_gradient = qMetaTypeId<QGradient>();
      const int t_image = QMetaType::QImage;
      const int t_color = QMetaType::QColor;
      const int t_transform = QMetaType::QTransform;
      if (f_optvar(L, 1, "gradient", t_gradient))
        s = QBrush(qVariantValue<QGradient>
                   (luaQ_toqvariant(L, -1, t_gradient)));
      lua_pop(L, 1);
      if (f_optvar(L, 1, "texture", t_image))
        {
          // qt4.4 barks when creating an image texture 
          // brush outside the gui thread.
          // s=QBrush(qVariantValue<QImage>(luaQ_toqvariant(L,-1,t_image)));
          lua_pushcfunction(L, create_textured_brush); 
          lua_insert(L, -2);
          luaQ_call(L, 1, 1, 0);
          const int t_brush = QMetaType::QBrush;
          s = qVariantValue<QBrush>(luaQ_toqvariant(L, -1, t_brush));
        }
      lua_pop(L, 1);
      if (f_optflag(L, 1, "style", m_style))
        s.setStyle(Qt::BrushStyle(lua_tointeger(L, -1)));
      lua_pop(L, 1);
      if (f_optvar(L, 1, "color", t_color)) {
        if (s.style() == Qt::NoBrush) 
          s.setStyle(Qt::SolidPattern);
        s.setColor(qVariantValue<QColor>(luaQ_toqvariant(L, -1, t_color)));
      }
      lua_pop(L, 1);
      if (f_optvar(L, 1, "transform", t_transform))
        s.setTransform(qVariantValue<QTransform>
                       (luaQ_toqvariant(L, -1, t_transform)));
    lua_pop(L, 1);
    }
  luaQ_pushqt(L, QVariant(s));
  return 1;
}


do_luareg(qbrush)
do_hook(qbrush)



// ========================================
// QPEN


static int
qpen_totable(lua_State *L)
{
  QPen s = luaQ_checkqvariant<QPen>(L, 1);
  lua_createtable(L, 0, 0);
  QMetaEnum m_penstyle = f_enumerator("PenStyle");
  QMetaEnum m_capstyle = f_enumerator("PenCapStyle");
  QMetaEnum m_joinstyle = f_enumerator("PenJoinStyle");
  Qt::PenStyle style = s.style();
  f_pushflag(L, (int)style, m_penstyle);
  lua_setfield(L, -2, "style");
  f_pushflag(L, (int)s.capStyle(), m_capstyle);
  lua_setfield(L, -2, "capStyle");
  f_pushflag(L, (int)s.joinStyle(), m_joinstyle);
  lua_setfield(L, -2, "joinStyle");
  luaQ_pushqt(L, QVariant(s.brush()));
  lua_setfield(L, -2, "brush"); 
  if (s.color().isValid())
    {
      luaQ_pushqt(L, QVariant(s.color()));
      lua_setfield(L, -2, "color");  
    }
  lua_pushnumber(L, s.widthF());
  lua_setfield(L, -2, "width");  
  lua_pushboolean(L, s.isCosmetic());
  lua_setfield(L, -2, "cosmetic");  
  if (style != Qt::NoPen && style != Qt::SolidLine)
    {
      lua_pushnumber(L, s.dashOffset());
      lua_setfield(L, -2, "dashOffset");  
    }
  if (s.joinStyle() == Qt::MiterJoin)
    {
      lua_pushnumber(L, s.miterLimit());
      lua_setfield(L, -2, "miterLimit");  
    }
  if (style == Qt::CustomDashLine)
    {
      QVector<qreal> v = s.dashPattern();
      lua_createtable(L, v.size(), 0);
      for (int i=0; i<v.size(); i++)
        {
          lua_pushnumber(L, v[i]);
          lua_rawseti(L, -2, i+1);
        }
      lua_setfield(L, -2, "dashPattern");
    }
  return 1;
}

static int
qpen_fromtable(lua_State *L)
{
  QPen s;
  if (! lua_isnoneornil(L, 1)) 
    {
      luaL_checktype(L, 1, LUA_TTABLE);
      QMetaEnum m_penstyle = f_enumerator("PenStyle");
      QMetaEnum m_capstyle = f_enumerator("PenCapStyle");
      QMetaEnum m_joinstyle = f_enumerator("PenJoinStyle");
      QMetaEnum m_style = f_enumerator("BrushStyle");
      const int t_color = QMetaType::QColor;
      const int t_brush = QMetaType::QBrush;
      if (f_optflag(L, 1, "style", m_penstyle))
        s.setStyle(Qt::PenStyle(lua_tointeger(L, -1)));
      lua_pop(L, 1);
      if (f_optflag(L, 1, "capStyle", m_capstyle))
        s.setCapStyle(Qt::PenCapStyle(lua_tointeger(L, -1)));
      lua_pop(L, 1);
      if (f_optflag(L, 1, "joinStyle", m_joinstyle))
        s.setJoinStyle(Qt::PenJoinStyle(lua_tointeger(L, -1)));
      lua_pop(L, 1);
      if (f_optvar(L, 1, "color", t_color))
        s.setColor(qVariantValue<QColor>(luaQ_toqvariant(L, -1, t_color)));
      if (f_optvar(L, 1, "brush", t_brush))
        s.setBrush(qVariantValue<QBrush>(luaQ_toqvariant(L, -1, t_brush)));
      if (f_opttype(L, 1, "width", LUA_TNUMBER))
        s.setWidthF(lua_tonumber(L, -1));
      lua_pop(L, 1);
      if (f_opttype(L, 1, "cosmetic", LUA_TBOOLEAN))
        s.setCosmetic(lua_toboolean(L, -1));
      lua_pop(L, 1);
      if (f_opttype(L, 1, "miterLimit", LUA_TNUMBER))
        s.setMiterLimit(lua_tonumber(L, -1));
      lua_pop(L, 1);
      if (f_opttype(L, 1, "dashOffset", LUA_TNUMBER))
        s.setDashOffset(lua_tonumber(L, -1));
      lua_pop(L, 1);
      if (f_opttype(L, 1, "dashPattern", LUA_TTABLE))
        {
          QVector<qreal> v;
          int n = lua_objlen(L, -1);
          if (n & 1)
            luaL_error(L, "field 'dashPattern' must be an array with even length");
          for (int i=1; i<=n; i++)
            {
              lua_rawgeti(L, -1, i);
              v << lua_tonumber(L, -1);
              lua_pop(L, 1);
            }
          s.setDashPattern(v);
        }
      lua_pop(L, 1);
    }
  luaQ_pushqt(L, QVariant(s));
  return 1;
}

do_luareg(qpen)
do_hook(qpen)


// ========================================
// QTRANSFORM


#define do_qtransform(do) \
  do ## optflt("m11",x=s.m11(),m11=x) \
  do ## optflt("m12",x=s.m12(),m12=x) \
  do ## optflt("m13",x=s.m13(),m13=x) \
  do ## optflt("m21",x=s.m21(),m21=x) \
  do ## optflt("m22",x=s.m22(),m22=x) \
  do ## optflt("m23",x=s.m23(),m23=x) \
  do ## optflt("m31",x=s.m31(),m31=x) \
  do ## optflt("m32",x=s.m32(),m32=x) \
  do ## optflt("m33",x=s.m33(),m33=x) 


do_totable(QTransform,qtransform,do_qtransform)

static void
qtransform_getquad(lua_State *L, int k, QPolygonF &polygon)
{
  luaL_checktype(L, k, LUA_TTABLE);
  polygon.resize(4);
  for (int i=1; i<=4; i++) {
    lua_rawgeti(L, k, i);
    polygon[i-1] = luaQ_checkqvariant<QPointF>(L, -1);
    lua_pop(L, 1);
  }
}

static int
qtransform_fromtable(lua_State *L)
{
  QTransform c;
  if (lua_gettop(L) >= 2)
    {
      QPolygonF one, two;
      qtransform_getquad(L, 1, one);
      qtransform_getquad(L, 2, two);
      if (! QTransform::quadToQuad(one, two, c)) {
        lua_pushnil(L);
        return 1;
      }
    }
  else if (lua_gettop(L) >= 1)
    {
      qreal m11=0, m12=0, m13=0;
      qreal m21=0, m22=0, m23=0;
      qreal m31=0, m32=0, m33=0;
      do_qtransform(fromtable_);
      c.setMatrix(m11,m12,m13,m21,m22,m23,m31,m32,m33);
    }
  luaQ_pushqt(L, QVariant(c));
  return 1;
}


static int
qtransform_scaled(lua_State *L)
{
  QTransform c = luaQ_checkqvariant<QTransform>(L, 1);
  qreal x = luaL_checknumber(L, 2);
  qreal y = luaL_optnumber(L, 3, x);
  c.scale(x,y);
  luaQ_pushqt(L, QVariant(c));
  return 1;
}


static int
qtransform_translated(lua_State *L)
{
  QTransform c = luaQ_checkqvariant<QTransform>(L, 1);
  qreal x = luaL_checknumber(L, 2);
  qreal y = luaL_checknumber(L, 3);
  c.translate(x,y);
  luaQ_pushqt(L, QVariant(c));
  return 1;
}


static int
qtransform_sheared(lua_State *L)
{
  QTransform c = luaQ_checkqvariant<QTransform>(L, 1);
  qreal x = luaL_checknumber(L, 2);
  qreal y = luaL_checknumber(L, 3);
  c.shear(x,y);
  luaQ_pushqt(L, QVariant(c));
  return 1;
}


static int
qtransform_rotated(lua_State *L)
{
  static const char *theaxis[] = { "XAxis", "YAxis", "ZAxis", 0 };
  static const char *theunits[] = { "Degrees", "Radians", 0 };
  QTransform c = luaQ_checkqvariant<QTransform>(L, 1);
  qreal r = luaL_checknumber(L, 2);
  int axis = luaL_checkoption(L, 3, "ZAxis", theaxis);
  int unit = luaL_checkoption(L, 4, "Degrees", theunits);
  if (unit == 0)
    c.rotate(r, Qt::Axis(axis));
  else
    c.rotateRadians(r, Qt::Axis(axis));
  luaQ_pushqt(L, QVariant(c));
  return 1;
}

static int
qtransform_inverted(lua_State *L)
{
  bool invertible;
  QTransform c = luaQ_checkqvariant<QTransform>(L, 1);
  QTransform d = c.inverted(&invertible);
  if (invertible)
    luaQ_pushqt(L, QVariant(d));
  else
    lua_pushnil(L);
  return 1;
}

static int
qtransform_map(lua_State *L)
{
  QTransform c = luaQ_checkqvariant<QTransform>(L, 1);
  QVariant v = luaQ_toqvariant(L, 2);
  int type = v.userType();
  if (lua_isnumber(L, 2)) 
    {
        qreal tx, ty;
        qreal x = luaL_checknumber(L, 2);
        qreal y = luaL_checknumber(L, 3);
        c.map(x,y,&tx,&ty);
        lua_pushnumber(L, tx);
        lua_pushnumber(L, ty);
        return 2; 
    } 
#define DO(T,M) else if (type == qMetaTypeId<T>()) \
      luaQ_pushqt(L, qVariantFromValue<T>(c.M(qVariantValue<T>(v))))
  DO(QPoint,map);
  DO(QPointF,map);
  DO(QLine,map);
  DO(QLineF,map);
  DO(QPolygon,map);
  DO(QPolygonF,map);
  DO(QRegion,map);
  DO(QPainterPath,map);
  DO(QRect,mapRect);
  DO(QRectF,mapRect);
#undef DO
  else
    luaL_typerror(L, 2, "point, polygon, region, or path");
  return 1;
}


static luaL_Reg qtransform_lib[] = {
  {"totable", qtransform_totable},
  {"new", qtransform_fromtable},
  {"scaled", qtransform_scaled},
  {"translated", qtransform_translated},
  {"sheared", qtransform_sheared},
  {"rotated", qtransform_rotated},
  {"inverted", qtransform_inverted},
  {"map", qtransform_map},
  {0,0}
};


do_hook(qtransform)





// ========================================
// QIMAGE


static int
qimage_new(lua_State *L)
{
  QImage image;
  if (lua_isuserdata(L, 1))
    {
      void *udata = luaL_checkudata(L, 2, LUA_FILEHANDLE);
      const char *format = luaL_optstring(L, 2, 0);
      QFile f;
      if (! f.open(*(FILE**)udata, QIODevice::ReadOnly))
        luaL_error(L,"cannot use stream for reading (%s)", 
                   f.errorString().toLocal8Bit().constData() );
      if (!image.load(&f,format) || image.isNull())
        luaL_error(L,"unable to load image file");
    }
  else if (lua_type(L, 1) == LUA_TSTRING)
    {
      const char *fname = lua_tostring(L, 1);
      const char *format = luaL_optstring(L, 2, 0);
      if (!image.load(QString::fromUtf8(fname),format) || image.isNull())
        luaL_error(L,"unable to load image file");
    }
  else
    {
      int w = luaL_checkinteger(L, 1);
      int h = luaL_checkinteger(L, 2);
      bool monochrome = lua_toboolean(L, 3);
      if (! lua_isnone(L, 3))
        luaL_checktype(L, 3, LUA_TBOOLEAN);
      if (monochrome)
        image = QImage(w, h, QImage::Format_Mono);
      else
        image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    }
  luaQ_pushqt(L, QVariant(image));
  return 1;
}


static int
qimage_size(lua_State *L)
{
  QImage s = luaQ_checkqvariant<QImage>(L, 1);
  luaQ_pushqt(L, s.size());
  return 1;
}

static int
qimage_rect(lua_State *L)
{
  QImage s = luaQ_checkqvariant<QImage>(L, 1);
  luaQ_pushqt(L, s.rect());
  return 1;
}

static int
qimage_depth(lua_State *L)
{
  QImage s = luaQ_checkqvariant<QImage>(L, 1);
  lua_pushinteger(L, s.depth());
  return 1;
}

static int
qimage_topixmap(lua_State *L)
{
  QImage s = luaQ_checkqvariant<QImage>(L, 1);
  luaQ_pushqt(L, QPixmap::fromImage(s));
  return 1;
}

static int
qimage_save(lua_State *L)
{
  QImage s = luaQ_checkqvariant<QImage>(L, 1);
  const char *format = 0;
  QFile f;
  if (lua_isuserdata(L, 2))
    {
      void *udata = luaL_checkudata(L, 2, LUA_FILEHANDLE);
      if (! f.open(*(FILE**)udata, QIODevice::WriteOnly))
        luaL_error(L,"cannot use stream for writing (%s)", 
                   f.errorString().toLocal8Bit().constData() );
      format = luaL_checkstring(L, 3);
    }
  else
    {
      const char *fname = luaL_checkstring(L, 2);
      f.setFileName(QString::fromLocal8Bit(fname));
      if (! f.open(QIODevice::WriteOnly))
        luaL_error(L,"cannot open '%s'for writing (%s)", fname,
                   f.errorString().toLocal8Bit().constData() );
      format = strrchr(fname, '.');
      format = luaL_optstring(L, 3, (format) ? format+1 : 0);
    }
  QImageWriter writer(&f, format);
  if (! writer.write(s))
    {
      f.remove();
      if (writer.error() == QImageWriter::UnsupportedFormatError)
        luaL_error(L, "image format '%s' not supported for writing", format);
      else
        luaL_error(L, "error while writing file (%s)",
                   f.errorString().toLocal8Bit().constData() );
    }
  return 0;
}


static luaL_Reg qimage_lib[] = {
  {"new", qimage_new},
  {"rect", qimage_rect},
  {"size", qimage_size},
  {"depth", qimage_depth},
  {"save", qimage_save},
  {0,0}
};


static luaL_Reg qimage_guilib[] = {
  {"topixmap", qimage_topixmap},
  {0,0}
};


static int qimage_hook(lua_State *L) 
{
  lua_getfield(L, -1, "__metatable");
  luaL_register(L, 0, qimage_lib);
  luaQ_register(L, qimage_guilib, QCoreApplication::instance());
  return 0;
}




// ========================================
// QTLUAPAINTER


static int
qtluapainter_new(lua_State *L)
{
  QtLuaPainter *p = 0;
  QObject *o = luaQ_toqobject(L, 1);
  QVariant v = luaQ_toqvariant(L, 1);
  if (v.userType() == QMetaType::QPixmap)
    {
      p = new QtLuaPainter(qVariantValue<QPixmap>(v));
    }
  else if (v.userType() == QMetaType::QImage)
    {
      p = new QtLuaPainter(qVariantValue<QImage>(v));
    }
  else if (qobject_cast<QWidget*>(o))
    {
      QWidget *w = qobject_cast<QWidget*>(o);
      bool buffered = true;
      if (! lua_isnone(L, 2))
        {
          luaL_checktype(L, 2, LUA_TBOOLEAN);
          buffered = lua_toboolean(L, 2);
        }
      p = new QtLuaPainter(w, buffered);
    }
  else if (qobject_cast<QtLuaPrinter*>(o))
    {
      QtLuaPrinter *w = qobject_cast<QtLuaPrinter*>(o);
      p = new QtLuaPainter(w);
    }
  else if (lua_type(L, 1) == LUA_TSTRING)
    {
      const char *f = luaL_checkstring(L, 1);
      const char *format = luaL_optstring(L, 2, 0);
      p = new QtLuaPainter(QString::fromLocal8Bit(f), format);
      if (p->image().isNull())
        luaL_error(L,"cannot load image from file '%s'", f);
    }
  else if (lua_isuserdata(L, 1))
    {
      QFile f;
      void *udata = luaL_checkudata(L, 1, LUA_FILEHANDLE);
      const char *format = luaL_optstring(L, 2, 0);
      if (! f.open(*(FILE**)udata, QIODevice::ReadOnly))
        luaL_error(L,"cannot use stream for reading (%s)", 
                   f.errorString().toLocal8Bit().constData() );
      QImage img;
      if(! img.load(&f, format))
        luaL_error(L,"cannot load image from file");
      
    }
  else
    {
      int w = luaL_checkinteger(L, 1);
      int h = luaL_checkinteger(L, 2);
      bool monochrome = lua_toboolean(L, 3);
      if (! lua_isnone(L, 3))
        luaL_checktype(L, 3, LUA_TBOOLEAN);
      p = new QtLuaPainter(w, h, monochrome);
    }
  luaQ_pushqt(L, p, true);
  return 1;
}


#define qtluapainter_v(t) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  p->t(); \
  return 0; }

qtluapainter_v(gbegin)
qtluapainter_v(gend)
qtluapainter_v(refresh)
qtluapainter_v(initgraphics)
qtluapainter_v(initclip)
qtluapainter_v(initmatrix)
qtluapainter_v(gsave)
qtluapainter_v(grestore)
qtluapainter_v(newpath)
qtluapainter_v(closepath)
qtluapainter_v(showpage)


#define qtluapainter_V(t,V) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  luaQ_pushqt(L, qVariantFromValue<V>(p->t())); \
  return 1; }

qtluapainter_V(rect, QRect)
qtluapainter_V(size, QSize)
qtluapainter_V(currentpen, QPen)
qtluapainter_V(currentbrush, QBrush)
qtluapainter_V(currentpoint, QPointF)
qtluapainter_V(currentpath, QPainterPath)
qtluapainter_V(currentclip, QPainterPath)
qtluapainter_V(currentfont, QFont)
qtluapainter_V(currentmatrix, QTransform)
qtluapainter_V(currentbackground, QBrush)
qtluapainter_V(currentstylesheet, QString)


static int 
qtluapainter_currentmode(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
  QMetaEnum e = f_enumerator("CompositionMode", mo);
  QtLuaPainter::CompositionMode s = p->currentmode();
  lua_pushstring(L, e.valueToKey((int)(s)));
  return 1; 
}

               
static int 
qtluapainter_currenthints(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
  QMetaEnum e = f_enumerator("RenderHints", mo);
  QtLuaPainter::RenderHints s = p->currenthints();
  lua_pushstring(L, e.valueToKeys((int)(s)).constData());
  return 1; 
}


static int 
qtluapainter_currentangleunit(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
  QMetaEnum e = f_enumerator("AngleUnit", mo);
  QtLuaPainter::AngleUnit s = p->currentangleunit();
  lua_pushstring(L, e.valueToKeys((int)(s)).constData());
  return 1; 
}


#define qtluapainter_vV(t,V) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  V v = luaQ_checkqvariant<V>(L, 2); \
  p->t(v); \
  return 0; }

qtluapainter_vV(setpoint, QPointF)
qtluapainter_vV(setpath,QPainterPath)
qtluapainter_vV(setclip,QPainterPath)
qtluapainter_vV(setmatrix,QTransform)
qtluapainter_vV(concat,QTransform)
qtluapainter_vV(charpath,QString)
qtluapainter_vV(setstylesheet,QString)


#define qtluapainter_vT(t,V,ft) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  if (lua_gettop(L) == 2 && lua_istable(L, 2)) { \
    lua_pushcfunction(L, ft); lua_insert(L, 2); \
    lua_call(L, 1, 1); }\
  V v = luaQ_checkqvariant<V>(L, 2); \
  p->t(v); \
  return 0; }

qtluapainter_vT(setpen, QPen, qpen_fromtable)
qtluapainter_vT(setbrush, QBrush, qbrush_fromtable)
qtluapainter_vT(setfont, QFont, qfont_fromtable)
qtluapainter_vT(setbackground, QBrush, qbrush_fromtable)


static int 
qtluapainter_setmode(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  const char *s = luaL_checkstring(L, 2);
  const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
  QMetaEnum e = f_enumerator("CompositionMode", mo);
  int x = e.keyToValue(s);
  luaL_argcheck(L, x>=0, 2, "unrecognized composition mode");
  p->setmode((QtLuaPainter::CompositionMode)x);
  return 0;
}


static int 
qtluapainter_sethints(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  const char *s = luaL_checkstring(L, 2);
  const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
  QMetaEnum e = f_enumerator("RenderHints", mo);
  int x = e.keysToValue(s);
  luaL_argcheck(L, x>=0, 2, "unrecognized render hints");
  p->sethints((QtLuaPainter::RenderHints)x);
  return 0;
}

static int 
qtluapainter_setangleunit(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  const char *s = luaL_checkstring(L, 2);
  const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
  QMetaEnum e = f_enumerator("AngleUnit", mo);
  int x = e.keysToValue(s);
  luaL_argcheck(L, x>=0, 2, "unrecognized render hints");
  p->setangleunit((QtLuaPainter::AngleUnit)x);
  return 0;
}



#define qtluapainter_vr(t) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  qreal r = luaL_checknumber(L, 2); \
  p->t(r); \
  return 0; }

qtluapainter_vr(rotate)


#define qtluapainter_vrr(t) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  qreal x = luaL_checknumber(L, 2); \
  qreal y = luaL_checknumber(L, 3); \
  p->t(x,y); \
  return 0; }

qtluapainter_vrr(scale)
qtluapainter_vrr(translate)
qtluapainter_vrr(moveto)
qtluapainter_vrr(lineto)
qtluapainter_vrr(rmoveto)
qtluapainter_vrr(rlineto)


#define qtluapainter_vrrrrr(t) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  qreal x1 = luaL_checknumber(L, 2); \
  qreal y1 = luaL_checknumber(L, 3); \
  qreal x2 = luaL_checknumber(L, 4); \
  qreal y2 = luaL_checknumber(L, 5); \
  qreal r = luaL_checknumber(L, 6); \
  p->t(x1,y1,x2,y2,r); \
  return 0; }

qtluapainter_vrrrrr(arc)
qtluapainter_vrrrrr(arcn)
qtluapainter_vrrrrr(arcto)


#define qtluapainter_vrrrrrr(t) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  qreal x1 = luaL_checknumber(L, 2); \
  qreal y1 = luaL_checknumber(L, 3); \
  qreal x2 = luaL_checknumber(L, 4); \
  qreal y2 = luaL_checknumber(L, 5); \
  qreal x3 = luaL_checknumber(L, 6); \
  qreal y3 = luaL_checknumber(L, 7); \
  p->t(x1,y1,x2,y2,x3,y3); \
  return 0; }

qtluapainter_vrrrrrr(curveto)
qtluapainter_vrrrrrr(rcurveto)


#define qtluapainter_vb(t) \
static int qtluapainter_ ## t (lua_State *L) { \
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);\
  if (lua_isnone(L, 2)) { p->t(); return 0; } \
  luaL_checktype(L, 2, LUA_TBOOLEAN); \
  p->t(lua_toboolean(L, 2)); \
  return 0; }

qtluapainter_vb(stroke)
qtluapainter_vb(fill)
qtluapainter_vb(eofill)
qtluapainter_vb(clip)
qtluapainter_vb(eoclip)


static int qtluapainter_show(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  QString s =luaQ_checkqvariant<QString>(L, 2);
  if (lua_gettop(L) == 2)
    {
      p->show(s);
    }
  else
    {
      qreal x = luaL_checknumber(L, 3);
      qreal y = luaL_checknumber(L, 4); 
      qreal w = luaL_checknumber(L, 5); 
      qreal h = luaL_checknumber(L, 6);
      const char *sf = luaL_optstring(L, 7, "AlignLeft");
      const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
      QMetaEnum e = f_enumerator("TextFlags", mo);
      int f = e.keysToValue(sf);
      luaL_argcheck(L, f>=0, 7, "unrecognized flag");
      p->show(s,x,y,w,h,f);
    }
  return 0;
}


static int qtluapainter_stringwidth(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  QString s =luaQ_checkqvariant<QString>(L, 2);
  qreal dx, dy;
  p->stringwidth(s, &dx, &dy);
  lua_pushnumber(L, dx);
  lua_pushnumber(L, dy);
  return 2;
}


static int qtluapainter_stringrect(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  QString s =luaQ_checkqvariant<QString>(L, 2);
  if (lua_gettop(L) == 2)
    {
      luaQ_pushqt(L, qVariantFromValue(p->stringrect(s)));
    }
  else
    {
      qreal x = luaL_checknumber(L, 3);
      qreal y = luaL_checknumber(L, 4); 
      qreal w = luaL_checknumber(L, 5); 
      qreal h = luaL_checknumber(L, 6);
      const char *sf = luaL_optstring(L, 7, "");
      const QMetaObject *mo = &QtLuaPainter::staticMetaObject;
      QMetaEnum e = f_enumerator("TextFlags", mo);
      int f = e.keysToValue(sf);
      luaL_argcheck(L, f>=0, 7, "unrecognized flag");
      luaQ_pushqt(L, qVariantFromValue(p->stringrect(s,x,y,w,h,f)));
    }
  return 1;
}


static int qtluapainter_rectangle(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  qreal x =luaL_checknumber(L, 2);
  qreal y =luaL_checknumber(L, 3);
  qreal w =luaL_checknumber(L, 4);
  qreal h =luaL_checknumber(L, 5);
  p->rectangle(x,y,w,h);
  return 0;
}


static int qtluapainter_image(lua_State *L)
{
  QtLuaPainter *p = luaQ_checkqobject<QtLuaPainter>(L, 1);
  if (lua_gettop(L) == 1)
    {
      luaQ_pushqt(L, QVariant(p->image()));
      return 1;
    }
  qreal x, y, w=-1, h=-1;
  qreal sx=0, sy=0, sw=-1, sh=-1;
  int k = 2;
  x = luaL_checknumber(L, k++);
  y = luaL_checknumber(L, k++);
  if (lua_isnumber(L, k)) {
    w = luaL_checknumber(L, k++);
    h = luaL_checknumber(L, k++);
  }
  QtLuaPainter *o = qobject_cast<QtLuaPainter*>(luaQ_toqobject(L, k));
  QVariant v = luaQ_toqvariant(L, k);
  if (lua_istable(L, k)) {
    lua_getfield(L, k, "image");
    if (lua_isfunction(L, -1)) {
      lua_pushvalue(L, k);
      lua_call(L, 1, 1);
      v = luaQ_toqvariant(L, -1);
    }
    lua_pop(L, 1);
  }
  if (v.userType() == QMetaType::QImage) {
    QImage q = qVariantValue<QImage>(v);
    sw = q.width(); 
    sh = q.height(); 
  } else if (v.userType() == QMetaType::QPixmap) {
    QPixmap q = qVariantValue<QPixmap>(v);
    sw = q.width(); 
    sh = q.height();
  } else if (o) {
    sw = o->width();
    sh = o->height();
  } else
    luaL_typerror(L, k, "QPixmap or QImage");
  k += 1;
  if (lua_isnumber(L, k)) {
    sx = luaL_checknumber(L, k++);
    sy = luaL_checknumber(L, k++);
  }
  if (lua_isnumber(L, k)) {
    sw = luaL_checknumber(L, k++);
    sh = luaL_checknumber(L, k++);
  }
  if (w <= 0)
    w = sw;
  if (h <= 0)
    h = sh;
  QRectF dst(x,y,w,h);
  QRectF src(sx,sy,sw,sh);
  if (v.userType() == QMetaType::QPixmap)
    p->image(dst, qVariantValue<QPixmap>(v), src);
  else if (v.userType() == QMetaType::QImage)
    p->image(dst, qVariantValue<QImage>(v), src);
  else if (o)
    p->image(dst, o, src);    
  return 0;
}


struct luaL_Reg qtluapainter_lib[] = {
  {"rect", qtluapainter_rect},
  {"size", qtluapainter_size},
  {"gbegin", qtluapainter_gbegin},
  {"refresh", qtluapainter_refresh},
  {"gend", qtluapainter_gend},
  {"gsave", qtluapainter_gsave},
  {"grestore", qtluapainter_grestore},
  {"currentpen", qtluapainter_currentpen},
  {"currentbrush", qtluapainter_currentbrush},
  {"currentpoint", qtluapainter_currentpoint},
  {"currentpath", qtluapainter_currentpath},
  {"currentclip", qtluapainter_currentclip},
  {"currentfont", qtluapainter_currentfont},
  {"currentmatrix", qtluapainter_currentmatrix},
  {"currentbackground", qtluapainter_currentbackground},
  {"currentmode", qtluapainter_currentmode},
  {"currenthints", qtluapainter_currenthints},
  {"currentangleunit", qtluapainter_currentangleunit},
  {"currentstylesheet", qtluapainter_currentstylesheet},
  {"setpen", qtluapainter_setpen},
  {"setbrush", qtluapainter_setbrush},
  {"setfont", qtluapainter_setfont},
  {"setpoint", qtluapainter_setpoint},
  {"setpath", qtluapainter_setpath},
  {"setclip", qtluapainter_setclip},
  {"setmatrix", qtluapainter_setmatrix},
  {"setbackground", qtluapainter_setbackground},
  {"setmode", qtluapainter_setmode},
  {"sethints", qtluapainter_sethints},
  {"setangleunit", qtluapainter_setangleunit},
  {"setstylesheet", qtluapainter_setstylesheet},
  {"initclip", qtluapainter_initclip},
  {"initmatrix", qtluapainter_initmatrix},
  {"initgraphics", qtluapainter_initgraphics},
  {"scale", qtluapainter_scale},
  {"rotate", qtluapainter_rotate},
  {"translate", qtluapainter_translate},
  {"concat", qtluapainter_concat},
  {"newpath", qtluapainter_newpath},
  {"moveto", qtluapainter_moveto},
  {"lineto", qtluapainter_lineto},
  {"curveto", qtluapainter_curveto},
  {"arc", qtluapainter_arc},
  {"arcn", qtluapainter_arcn},
  {"arcto", qtluapainter_arcto},
  {"rmoveto", qtluapainter_rmoveto},
  {"rlineto", qtluapainter_rlineto},
  {"rcurveto", qtluapainter_rcurveto},
  {"closepath", qtluapainter_closepath},
  {"stroke", qtluapainter_stroke},
  {"fill", qtluapainter_fill},
  {"eofill", qtluapainter_eofill},
  {"clip", qtluapainter_clip},
  {"eoclip", qtluapainter_eoclip},
  {"image", qtluapainter_image},
  {"showpage", qtluapainter_showpage},
  {"rectangle", qtluapainter_rectangle},
  {0,0}
};


struct luaL_Reg qtluapainter_guilib[] = {
  // things that have to be done in the gui thread
  {"new", qtluapainter_new},
  {"charpath", qtluapainter_charpath},
  {"show", qtluapainter_show},
  {"stringwidth", qtluapainter_stringwidth},
  {"stringrect", qtluapainter_stringrect},
  {"stringrect", qtluapainter_stringrect},
  {0,0}
};


static int qtluapainter_hook(lua_State *L) 
{
  lua_getfield(L, -1, "__metatable");
  luaL_register(L, 0, qtluapainter_lib);
  // luaQ_register(L, qtluapainter_lib, QCoreApplication::instance());
  luaQ_register(L, qtluapainter_guilib, QCoreApplication::instance());
  return 0;
}





// ========================================
// WIDGET




static int qwidget_new(lua_State *L)
{
  QWidget *parent = luaQ_optqobject<QWidget>(L, 1);
  QWidget *w = new QWidget(parent);
  luaQ_pushqt(L, w, !parent);
  return 1;
}


static int qwidget_window(lua_State *L)
{
  QWidget *w = luaQ_checkqobject<QWidget>(L, 1);
  luaQ_pushqt(L, w->window());
  return 1;
}


static struct luaL_Reg qwidget_lib[] = {
  {"new", qwidget_new},
  {"window", qwidget_window},
  {0,0}
};


static int qwidget_hook(lua_State *L) 
{
  lua_getfield(L, -1, "__metatable");
  luaQ_register(L, qwidget_lib, QCoreApplication::instance());
  return 0;
}



// ========================================
// LISTENER



static int qtlualistener_new(lua_State *L)
{
  QWidget *w = luaQ_checkqobject<QWidget>(L, 1);
  QtLuaListener *l = new QtLuaListener(w);
  l->setObjectName("listener");
  luaQ_pushqt(L, l);
  return 1;
}


static struct luaL_Reg qtlualistener_lib[] = {
  {"new", qtlualistener_new},
  {0,0}
};


static int qtlualistener_hook(lua_State *L) 
{
  lua_getfield(L, -1, "__metatable");
  luaQ_register(L, qtlualistener_lib, QCoreApplication::instance());
  return 0;
}



// ========================================
// QTLUAPRINTER


static int qtluaprinter_new(lua_State *L)
{
  static const char *modes[] = {"ScreenResolution","HighResolution",0};
  QPrinter::PrinterMode mode = QPrinter::ScreenResolution;
  if (luaL_checkoption(L, 1, "ScreenResolution", modes))
    mode = QPrinter::HighResolution;
  QtLuaPrinter *p = new QtLuaPrinter(mode);
  luaQ_pushqt(L, p, true);
  return 1;
}


static luaL_Reg qtluaprinter_lib[] = {
  {"new", qtluaprinter_new},
  {0,0}
};


static int qtluaprinter_hook(lua_State *L) 
{
  lua_getfield(L, -1, "__metatable");
  luaQ_register(L, qtluaprinter_lib, QCoreApplication::instance());
  return 0;
}



// ========================================
// REGISTER


LUA_EXTERNC QTWIDGET_API 
int luaopen_libqtwidget(lua_State *L)
{ 
  // load module 'qt'
  if (luaL_dostring(L, "require 'qt'"))
    lua_error(L);
  if (QApplication::type() == QApplication::Tty)
    luaL_error(L, "Graphics have been disabled (running with -nographics)");

  // call hook for qobjects
#define HOOK_QOBJECT(T, t) \
     lua_pushcfunction(L, t ## _hook);\
     luaQ_pushmeta(L, &T::staticMetaObject);\
     lua_call(L, 1, 0)
  
  // call hook for qvariants
#define HOOK_QVARIANT(T, t) \
     lua_pushcfunction(L, t ## _hook);\
     luaQ_pushmeta(L, QMetaType::T);\
     lua_call(L, 1, 0)
  
  HOOK_QVARIANT(QFont, qfont);
  HOOK_QVARIANT(QColor, qcolor);
  HOOK_QVARIANT(QBrush, qbrush);
  HOOK_QVARIANT(QPen, qpen);
  HOOK_QVARIANT(QTransform, qtransform);
  HOOK_QVARIANT(QImage, qimage);
  HOOK_QOBJECT(QWidget, qwidget);  
  HOOK_QOBJECT(QtLuaPrinter, qtluaprinter);  
  HOOK_QOBJECT(QtLuaPainter, qtluapainter);  
  HOOK_QOBJECT(QtLuaListener, qtlualistener);  
  return 1;
}




/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */


