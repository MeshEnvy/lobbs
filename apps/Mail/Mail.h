#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailDal.h"

class MailApp
{
  public:
    explicit MailApp(LoDb &lodb);
    MailDal &dal() { return dal_; }

  private:
    MailDal dal_;
};

#endif
