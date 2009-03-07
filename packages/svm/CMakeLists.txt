SET(src svqp2.h svqp2.cpp QPSolver.cpp init.c Kernel.c)
SET(luasrc init.lua Kernel.lua ClassificationSVM.lua)

ADD_TORCH_PACKAGE(svm "${src}" "${luasrc}")

TARGET_LINK_LIBRARIES(svm luaT TH)
