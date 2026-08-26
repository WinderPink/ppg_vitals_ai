#include "tflite_micro_model_config.h"
#include "ml/accelerator/mvpv1/pooling2d_compiled.hpp"
#include "ml/compiled_model/compiled_model.hpp"
#include "ml/accelerator/mvpv1/compiled_registration_helper.hpp"
#include "ml/accelerator/mvpv1/mvpv1_kernel_config.hpp"
#include "ml/accelerator/mvpv1/tflite_micro_mvpv1_context.hpp"

namespace tflite {

using namespace npu_toolkit;

namespace {

TfLiteStatus mvp_prepare(TfLiteContext* context, TfLiteNode* node)
{
  return kTfLiteOk;
}

TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const auto input = tflite::micro::GetEvalInput(context, node, kInputTensor);
  const auto input_data = tflite::micro::GetTensorData<int8_t>(input);
  auto output = tflite::micro::GetEvalOutput(context, node, kOutputTensor);
  auto output_data = tflite::micro::GetTensorData<int8_t>(output);

  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;

  array_base_addresses[MVP_INPUT_ARRAY] = reinterpret_cast<uintptr_t>(input_data);
  array_base_addresses[MVP_OUTPUT_ARRAY] = reinterpret_cast<uintptr_t>(output_data);
  mvpv1_context.update_array_base_addresses_callback = nullptr;

  return compiled_model_process_layer_with_accelerator(context);
}

void* mvp_init(TfLiteContext* context, const char* buffer, size_t length)
{
  return compiled_registration_init(
      context,
      buffer,
      length,
      0
      #if MVPV1_KERNELS_MAX_POOL2D_FALLBACK_ENABLED == 1 || MVPV1_KERNELS_AVERAGE_POOL2D_FALLBACK_ENABLED == 1
      , cmsis_nn_pooling_get_init_size()
      #endif
  );
}

} // namespace

void* mvp_compiled_average_pool_init(TfLiteContext* context, const char* buffer, size_t length)
{
  return mvp_init(context, buffer, length);
}

TfLiteStatus mvp_compiled_average_pool_prepare(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_prepare(
      context,
      node,
      mvp_prepare
      #if MVPV1_KERNELS_AVERAGE_POOL2D_FALLBACK_ENABLED == 1
      , cmsis_nn_average_pool_prepare
      #endif
  );
}

TfLiteStatus mvp_compiled_average_pool_invoke(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_invoke(
      context,
      node,
      mvp_invoke
      #if MVPV1_KERNELS_AVERAGE_POOL2D_FALLBACK_ENABLED == 1
      , cmsis_nn_average_pool_invoke
      #endif
  );
}

void* mvp_compiled_max_pool_init(TfLiteContext* context, const char* buffer, size_t length)
{
  return mvp_init(context, buffer, length);
}

TfLiteStatus mvp_compiled_max_pool_prepare(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_prepare(
      context,
      node,
      mvp_prepare
      #if MVPV1_KERNELS_MAX_POOL2D_FALLBACK_ENABLED == 1
      , cmsis_nn_max_pool_prepare
      #endif
  );
}

TfLiteStatus mvp_compiled_max_pool_invoke(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_invoke(
      context,
      node,
      mvp_invoke
      #if MVPV1_KERNELS_MAX_POOL2D_FALLBACK_ENABLED == 1
      , cmsis_nn_max_pool_invoke
      #endif
  );
}

TFLMRegistration Register_AVERAGE_POOL_2D() {
  return tflite::micro::RegisterOp(
      mvp_compiled_average_pool_init,
      mvp_compiled_average_pool_prepare,
      mvp_compiled_average_pool_invoke
  );
}

TFLMRegistration Register_MAX_POOL_2D() {
  return tflite::micro::RegisterOp(
      mvp_compiled_max_pool_init,
      mvp_compiled_max_pool_prepare,
      mvp_compiled_max_pool_invoke
  );
}

} // namespace tflite