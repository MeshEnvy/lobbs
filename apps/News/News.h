#pragma once
#include "NewsDal.h"

class NewsApp
{
  public:
    explicit NewsApp(LoDb &lodb);
    NewsDal &dal() { return dal_; }

  private:
    NewsDal dal_;
};

