local SVM = torch.class('svm.ClassificationSVM')

function SVM:__init(kernel)
   self.kernel = kernel
   self.sv = torch.Tensor()
   self.alpha = torch.Tensor()
   self.bias = 0
end

function SVM:forward(input)
   local sum = self.bias
   local kernel = self.kernel
   local sv = self.sv
   local i = 0
   self.alpha:apply(function(_alpha_)
                       i = i + 1
                       sum = sum + _alpha_ * kernel:eval(sv:select(2, i), input)
                    end)
   return sum
end

function SVM:train(data, y, C, noBias)
   local n = data:size(2)
   local solver = svm.QPSolver(n)
   if noBias then
      self.bias = 0
      solver:sumflag(false)
   end
   
   local cmin = torch.Tensor(n)
   local cmax = torch.Tensor(n)
   if type(C) == 'number' then
      for i=1,n do
         if y[i] > 0 then
            cmin[i] = 0
            cmax[i] = C
         else
            cmin[i] = -C
            cmax[i] = 0
         end
      end
   else
      for i=1,n do
         if y[i] > 0 then
            cmin[i] = 0
            cmax[i] = C[i]
         else
            cmin[i] = -C[i]
            cmax[i] = 0
         end
      end
   end

   solver:cmin(cmin)
   solver:cmax(cmax)
   solver:b(y)
   solver:run(data, self.kernel)

   if noBias == nil then
      self.bias = (solver:gmin()+solver:gmax())/2
   end

   local alpha = solver:x()
   local nsv = 0
   local nsvbound = 0
   for i=1,n do
      if alpha[i] ~= 0 then
         nsv = nsv + 1
      end
      if y[i] > 0 and alpha[i] == cmax[i] then nsvbound = nsvbound + 1 end 
      if y[i] < 0 and alpha[i] == cmin[i] then nsvbound = nsvbound + 1 end     
   end

   print('[number of SVs = ' .. nsv .. ']') 
   print('[number of SVs at bound = ' .. nsvbound .. ']')
  
   self.alpha:resize(nsv)
   self.sv:resize(data:size(1), nsv)
   nsv = 0
   for i=1,n do
      if alpha[i] ~= 0 then
         nsv = nsv + 1
         self.sv:select(2, nsv):copy(data:select(2, i))
         self.alpha[nsv] = alpha[i]
      end
   end
end
