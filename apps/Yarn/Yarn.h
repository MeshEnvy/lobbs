#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "YarnDal.h"
#include <lodb/LoDB.h>

class YarnApp
{
  public:
    explicit YarnApp(LoDb &lodb);
    YarnDal &dal() { return dal_; }

  private:
    YarnDal dal_;
};

#endif
