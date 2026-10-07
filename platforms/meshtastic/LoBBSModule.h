#pragma once

#include "core/LoBBSKernel.h"
#include "MeshModule.h"
#include "SinglePortModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

class LoBBSModule : public SinglePortModule
{
  public:
    LoBBSModule();
    ~LoBBSModule() override = default;

    LoBBSKernel &kernel() { return kernel_; }
    const LoBBSKernel &kernel() const { return kernel_; }

  protected:
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    LoBBSKernel kernel_;
};
