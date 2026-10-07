#pragma once
#include "WallDal.h"
#include <lodb/LoDB.h>

class WallApp
{
  public:
    explicit WallApp(LoDb &lodb);
    WallDal &dal() { return dal_; }

  private:
    WallDal dal_;
};

