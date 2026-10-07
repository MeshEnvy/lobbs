#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "NewsDal.h"

class NewsApp
{
  public:
    explicit NewsApp(LoDb &lodb);
    NewsDal &dal() { return dal_; }

  private:
    NewsDal dal_;
};

#endif
