#pragma once
#include <lodb/LoDB.h>
#include <loscalar/LoScalar.h>
#include <map>
#include <string>
#include <vector>

class LoBBSKernel;
struct LoBBSCommandCtx;

class ConfigDal
{
  public:
    explicit ConfigDal(LoDb &lodb);

    void reloadOverrides();
    void resetCache();
    void ensureRegistry(LoBBSCommandCtx &ctx);
    const LoScalar *findKeyDef(const char *key);
    const std::vector<LoScalar> &keyDefs(LoBBSCommandCtx &ctx);
    uint32_t effectiveValue(LoBBSCommandCtx &ctx, const char *key);
    const char *validateAndSet(LoBBSCommandCtx &ctx, const char *key, uint32_t value);
    const char *resetKey(LoBBSCommandCtx &ctx, const char *key);
    void notifyDatabaseOpened(LoBBSKernel &kernel);

  private:
    lodb_uuid_t rowUuid(const char *key) const;
    void fireConfigChanged(LoBBSCommandCtx &ctx, const char *key, uint32_t value);

    LoDb &lodb_;
    std::map<std::string, uint32_t> overrides_;
    std::vector<LoScalar> registry_;
    bool registryBuilt_ = false;
    bool overridesLoaded_ = false;
    char lastValidateError_[128] = {0};
};

