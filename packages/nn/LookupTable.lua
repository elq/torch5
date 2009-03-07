local LookupTable, parent = torch.class('nn.LookupTable', 'nn.Module')

function LookupTable:__init(nIndex, ...)
   parent.__init(self)

   if select('#', ...) == 1 and type(select(1, ...)) ~= "number" then
      local size = select(1, ...)
      self.size = torch.LongStorage(#size + 1)
      for i=1,#size do
         self.size[i] = size[i]
      end
   else
      self.size = torch.LongStorage(select('#', ...)+1)
      for i=1,select('#',...) do
         self.size[i] = select(i, ...)
      end
   end

   self.size[self.size:size()] = nIndex
   self.weight = torch.Tensor(self.size)
   self.currentGradWeight = torch.Tensor()
   self.currentInput = torch.Tensor()

   self:reset()
end

function LookupTable:reset(stdv)
   stdv = stdv or 1
   self.weight:apply(function()
                        return random.normal(0, stdv)
                     end)
end

function LookupTable:forward(input)
   local nIndex = input:size(1)
   local dim = self.size:size()
   self.size[dim] = nIndex
   self.output:resize(self.size)

   for i=1,input:size(1) do
      self.output:select(dim, i):copy(self.weight:select(dim, input[i]))
   end

   return self.output
end

function LookupTable:zeroGradParameters()
end

function LookupTable:backward(input, gradOutput)
   self.currentInput:resizeAs(input):copy(input)
   self.currentGradWeight:resizeAs(gradOutput)
   self.currentGradWeight:copy(gradOutput)
end

function LookupTable:updateParameters(learningRate)
   local currentInput = self.currentInput
   local dim = self.size:size()
   for i=1,self.currentInput:size(1) do
      self.weight:select(dim, currentInput[i]):add(-learningRate, self.currentGradWeight:select(dim, i))
   end
end

function LookupTable:write(file)
   parent.write(self, file)
   file:writeObject(self.size)
   file:writeObject(self.weight)
   file:writeObject(self.currentInput)
   file:writeObject(self.currentGradWeight)
end

function LookupTable:read(file, version)
   parent.read(self, file)
   if version > 0 then
      self.size = file:readObject()
   else
      local size = file:readObject()
      self.size = torch.LongStorage(size:size())
      self.size:copy(size)
   end
   self.weight = file:readObject()
   self.currentInput = file:readObject()
   self.currentGradWeight = file:readObject()
end
