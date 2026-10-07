#pragma once
#include "MailDal.h"

class MailApp
{
  public:
    explicit MailApp(LoDb &lodb);
    MailDal &dal() { return dal_; }

  private:
    MailDal dal_;
};

