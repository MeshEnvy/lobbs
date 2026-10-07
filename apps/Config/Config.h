#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "ConfigDal.h"
#include <lodb/LoDB.h>

class ConfigApp
{
  public:
    explicit ConfigApp(LoDb &lodb);
    ConfigDal &dal() { return dal_; }

  private:
    ConfigDal dal_;
};

#endif
