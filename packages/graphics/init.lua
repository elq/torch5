

require('torch')

local base = torch.packageLuaPath('graphics')

if package.preload['qt'] then 
   require("qt") 
end
if (qt and qt.qApp and not qt.qApp:runsWithoutGraphics()) then
   -- we are running qlua with graphics enabled
   dofile(base .. "/wrap-qt.lua")
else
   -- use lcairo
   dofile(base .. "/wrap-cairo.lua")
end

