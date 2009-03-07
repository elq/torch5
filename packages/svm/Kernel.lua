local Kernel = torch.class('svm.Kernel')

local LinearKernel = torch.class('svm.LinearKernel', 'svm.Kernel')
function LinearKernel:__init(gamma)
   gamma = gamma or 1
   self:__cinit(gamma)
end

local RBFKernel = torch.class('svm.RBFKernel', 'svm.Kernel')
function RBFKernel:__init(gamma)
   gamma = gamma or 1
   self:__cinit(gamma)
end

local PolynomialKernel = torch.class('svm.PolynomialKernel', 'svm.Kernel')
function PolynomialKernel:__init(degree, gamma, bias)
   degree = degree or 1
   gamma = gamma or 1
   bias = bias or 0
   self:__cinit(degree, gamma, bias)
end

local TanhKernel = torch.class('svm.TanhKernel', 'svm.Kernel')
function TanhKernel:__init(gamma, bias)
   gamma = gamma or 1
   bias = bias or 0
   self:__cinit(gamma, bias)
end

-- DEBUG: note: __gc peut etre redirige pour caller aussi __cinit? (not sure)

-- on peut mettre un __gc optionel dans les tables... celui-ci pointe sur un userdata vide.
-- on peut pas tout mettre dans une table. genre un tensor, tu veux que ca prenne pas trop de place. euh, c'est sur ca?
-- parce que ca serait vraiment plus clean -- pour l'instant, dur d'apprehender le torch_newmetatable, plus chiant
-- pour heritage (lua vs non-lua... tricky pour kernel par exemple... t'as vu pourquoi ca marche??)
-- ou alors tout descend d'un userdata + eventuel get/set env...
-- pcall pour eviter plantage bus error? ou modification du code de svqp2?
