#include "tflite_micro_model_config.h"
#pragma once


#include "ml/tflite_micro_model/tflite_micro_model_context.hpp"


namespace npu_toolkit
{

class CompiledModelContext;

class TfliteMicroMvpv1Context : public TfliteMicroModelContext
{
public:

  static TfliteMicroMvpv1Context* create(TfLiteContext *context)
  {
    auto buffer = context->AllocatePersistentBuffer(context, sizeof(TfliteMicroMvpv1Context));
    if(buffer == nullptr)
    {
      return nullptr;
    }
    return new(buffer)TfliteMicroMvpv1Context();
  }

  static TfliteMicroMvpv1Context* get(TfLiteContext* context)
  {
    auto micro_context = tflite::GetMicroContext(context);
    assert(micro_context != nullptr);
    auto mvpv1_context = reinterpret_cast<TfliteMicroMvpv1Context*>(micro_context->external_context());
    assert(mvpv1_context != nullptr);
    return mvpv1_context;
  }

  bool init(TfliteMicroModel* model) override;
  bool load() override;

  CompiledModelContext* compiled_context = nullptr;
  uintptr_t array_base_addresses[5];
  uintptr_t array_base_addresses2[5];
  void (*update_array_base_addresses_callback)(TfliteMicroMvpv1Context&) = nullptr;

protected:
  TfliteMicroMvpv1Context() = default;

};



} // namespace npu_toolkit