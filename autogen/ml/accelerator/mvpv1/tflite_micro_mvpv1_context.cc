#include "tflite_micro_model_config.h"

#include "ml/compiled_model/compiled_model_context.hpp"
#include "ml/compiled_model/compiled_model.hpp"
#include "tflite_micro_model.hpp"
#include "ml/accelerator/mvpv1/tflite_micro_mvpv1_accelerator.hpp"
#include "ml/accelerator/mvpv1/tflite_micro_mvpv1_context.hpp"


namespace npu_toolkit
{



bool TfliteMicroMvpv1Context::init(TfliteMicroModel* model)
{
  if(!TfliteMicroModelContext::init(model))
  {
    return false;
  }

  auto tfl_context  = model->tflite_context();
  const void* compiled_data = compiled_model_retrieve_compilation_data(tfl_context);

  if(compiled_data == nullptr)
  {
    return true;
  }

  auto compiled_context = CompiledModelContext::create(
    tfl_context,
    compiled_data
  );
  if(compiled_context == nullptr)
  {
    return false;
  }

  if(!compiled_context->init())
  {
    return false;
  }

  this->compiled_context = compiled_context;

  return true;
}

bool TfliteMicroMvpv1Context::load()
{
  auto mvpv1_context = TfliteMicroMvpv1Context::get(_model->tflite_context());
  if(mvpv1_context->compiled_context != nullptr)
  {
    if(!mvpv1_context->compiled_context->load())
    {
      return false;
    }
  }

  return true;
}


} // namespace npu_toolkit