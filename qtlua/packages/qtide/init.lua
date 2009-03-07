
require 'qt'
require 'paths'

if (qt and qt.qApp and qt.qApp:runsWithoutGraphics()) then
   print("qlua: not loading module qtide (running with -nographics)")
   return
end


qt.require 'libqtide'

local G = _G
local error = error
local loadstring = loadstring
local print = print
local qt = qt
local string = string
local tonumber = tonumber
local tostring = tostring
local type = type
local paths = paths

module('qtide')

-- Startup --

local qluaide = qt.QLuaIde()

local qluaidemode = nil

local function realmode(mode)
   mode = mode or qt.qApp:readSettings("ide/mode")
   if qluaide.mdiDefault then
      return mode or 'mdi'
   else
      return mode or 'sdi'
   end
end

function setup(mode)
   mode = realmode(mode)
   if qt.qLuaMdiMain then
      if (mode == qluaidemode) then return end
      qt.qcall(qt.qLuaMdiMain,
               function() qt.qLuaMdiMain:deleteLater() qt.doevents() end )
   end
   if (mode == "mdi") then     -- subwindows within a big window
      local mdi = qluaide:createMdiMain()
      mdi.tabMode = false
      mdi.clientClass = "QWidget"
      mdi:adoptAll()
   elseif (mode == "tab") then     -- groups all editors in tabs
      local mdi = qluaide:createMdiMain()
      mdi.tabMode = true
      mdi.clientClass = "QLuaEditor"
      mdi:adoptAll()
   elseif (mode == "tab2") then    -- groups all editors + console in tabs
      local mdi = qluaide:createMdiMain()
      mdi.tabMode = true
      mdi.clientClass = "QLuaMainWindow"
      mdi:adoptAll()
   elseif (mode ~= "sdi") then     -- all windows separate
      print("Warning: The recognized ide styles are: sdi, mdi, tab")
      mode = 'sdi'
   end
   qt.qApp:writeSettings("ide/mode", mode)
   qluaidemode = mode
end

function start(mode)
   setup(mode)
   if not qt.qLuaSdiMain then
      qluaide:createSdiMain()
      qluaide.editOnError = true
   end
end



-- Editor --

function editor(s)
   local e = qluaide:editor(s or "")
   if e == nil and type(s) == "string" then
      error(string.format("Unable to read file '%s'", s))
   end
   return e
end

function doeditor(e)
   -- validate parameter
   if not qt.isa(e, 'QLuaEditor*') then 
      error(string.format("QLuaEditor expected, got %s.", s)); 
   end
   -- retrieve text
   local n = "qt." .. tostring(e.objectName)
   local chunk, m = loadstring(e:widget().plainText:tostring(), n)
   if not chunk then
      print(m)
      -- error while parsing the data
      local _,_,l,m = string.find(m,"^%[string.*%]:(%d+): +(.*)")
      if l and m and qluaide.editOnError then 
         e:widget():showLine(tonumber(l)) 
         e:showStatusMessage(m)
      end
   else
      -- execution starts
      chunk()
   end
end





-- Inspector --

function inspector(...)
   error("Function qtide.inspector is not yet working")
end





-- Help --

function browser(url)
   qluaide:browser(url or "about:/")
end


local appname = qt.qApp.applicationName:tostring():lower()
local helpindex1 = paths.concat(paths.install_html, appname, "index.html")
local helpindex2 = paths.concat(paths.install_html, "index.html")
local helperror = 
   "The HTML version of the help files are not installed." ..
   "Did you set the HTML_DOC option in Cmake?"

qt.disconnect(qluaide, 'helpRequested(QLuaMainWindow*)')
qt.connect(qluaide,'helpRequested(QLuaMainWindow*)',
           function(main)
              if helpindex1 and paths.filep(helpindex1) then
                 qluaide:browser(helpindex1)
              elseif helpindex2 and paths.filep(helpindex2) then
                 qluaide:browser(helpindex2)
              else
                 main:messageBox("QLua Warning", helperror)
              end
           end )


-- Preferences --

function preferences()
   error("Function qtide.preferences is not yet working")
end

qt.disconnect(qluaide,'prefsRequested(QLuaMainWindow*)')
qt.connect(qluaide,'prefsRequested(QLuaMainWindow*)',
           function(main)
              main:messageBox("QLua Warning",
                              "The preference dialog is not yet working!")
           end )





