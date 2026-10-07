#pragma once
#include "AuthDal.h"

class AuthApp
{
  public:
    explicit AuthApp(LoDb &lodb);
    AuthDal &dal() { return dal_; }

  private:
    AuthDal dal_;
};

