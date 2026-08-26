#include "tflite_micro_model_config.h"
#include "ml/third_party/tflm/kernel_util.h"
#include "ml/accelerator/mvpv1/fully_connected_compiled.hpp"
#include "ml/compiled_model/compiled_model.hpp"
#include "ml/accelerator/mvpv1/compiled_registration_helper.hpp"
#include "ml/accelerator/mvpv1/mvpv1_kernel_config.hpp"
#include "ml/accelerator/mvpv1/tflite_micro_mvpv1_context.hpp"


namespace tflite {

using namespace npu_toolkit;


struct OpData
{
  float16_t* bias;
  float16_t* per_channel_output_scaler;
};


namespace
{

TfLiteStatus mvp_prepare(TfLiteContext* context, TfLiteNode* node)
{
  OpData& data = *static_cast<OpData*>(node->user_data);
  MicroContext* micro_context = GetMicroContext(context);

  TfLiteTensor* input =
          micro_context->AllocateTempInputTensor(node, kInputTensor);
  TfLiteTensor* weights = micro_context->AllocateTempInputTensor(
          node, kWeightsTensor);
  TfLiteTensor* bias = (node->inputs->size == 3) ?
      micro_context->AllocateTempInputTensor(node, kBiasTensor) : nullptr;
  TfLiteTensor* output = micro_context->AllocateTempOutputTensor(
          node, kOutputTensor);

  TF_LITE_ENSURE_STATUS(allocate_scaled_bias_tensor(context, bias, &data.bias));
  data.per_channel_output_scaler = nullptr;

  if (weights->quantization.type == kTfLiteAffineQuantization &&
      weights->quantization.params != nullptr)
  {
    const auto* affine_quantization = reinterpret_cast<TfLiteAffineQuantization*>(weights->quantization.params);

    // Dynamically allocate per-channel quantization parameters.
    const int per_channel_quantization_size = affine_quantization->scale->size;

    if(per_channel_quantization_size > 1)
    {
      TF_LITE_ENSURE_STATUS(allocate_output_multiplier_buffer(
        context,
        input,
        weights,
        output,
        per_channel_quantization_size,
        &data.per_channel_output_scaler
      ));
    }
  }

  return kTfLiteOk;
}


TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const float16_t zero_data(0);
  const OpData& data = *(static_cast<const OpData*>(node->user_data));
  const auto input = tflite::micro::GetEvalInput(context, node, kInputTensor);
  const auto weights = tflite::micro::GetEvalInput(context, node, kWeightsTensor);
  const auto bias   = NumInputs(node) == 3
                      ? tflite::micro::GetEvalInput(context, node, kBiasTensor)
                      : nullptr;
  auto output       = tflite::micro::GetEvalOutput(context, node, kOutputTensor);
  auto input_data     = tflite::micro::GetTensorData<int8_t>(input);
  auto weights_data   = tflite::micro::GetTensorData<int8_t>(weights);
  auto output_data    = tflite::micro::GetTensorData<int8_t>(output);
  const auto bias_data = (data.bias != nullptr) ? data.bias :
      tflite::micro::GetOptionalTensorData<float16_t>(bias);

  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;

  array_base_addresses[MVP_OUTPUT_ARRAY] = (uintptr_t)output_data;
  array_base_addresses[MVP_INPUT_ARRAY] = (uintptr_t)input_data;
  array_base_addresses[MVP_WEIGHTS_ARRAY] = (uintptr_t)weights_data;
  array_base_addresses[MVP_BIAS_ARRAY] = (uintptr_t)((bias_data == nullptr) ? &zero_data : bias_data);
  array_base_addresses[MVP_OUTPUT_MULTIPLIER_ARRAY] = (uintptr_t)data.per_channel_output_scaler;
  mvpv1_context.update_array_base_addresses_callback = nullptr;

  return compiled_model_process_layer_with_accelerator(context);
}


} // namespace




void* mvp_compiled_fully_connected_init(TfLiteContext* context, const char* buffer, size_t length)
{
  return compiled_registration_init(
    context,
    buffer,
    length,
    sizeof(OpData)
    #if MVPV1_KERNELS_FULLY_CONNECTED_FALLBACK_ENABLED == 1
    ,cmsis_nn_fully_connected_get_init_size()
    #endif
  );
}


TfLiteStatus mvp_compiled_fully_connected_prepare(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_prepare(
    context,
    node,
    mvp_prepare
    #if MVPV1_KERNELS_FULLY_CONNECTED_FALLBACK_ENABLED == 1
    ,cmsis_nn_fully_connected_prepare
    #endif
  );
}

TfLiteStatus mvp_compiled_fully_connected_invoke(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_invoke(
    context,
    node,
    mvp_invoke
    #if MVPV1_KERNELS_FULLY_CONNECTED_FALLBACK_ENABLED == 1
    ,cmsis_nn_fully_connected_invoke
    #endif
  );
}



// Define the this kernel's registeration if this is an embedded build
TFLMRegistration Register_FULLY_CONNECTED() {
  return tflite::micro::RegisterOp(
    mvp_compiled_fully_connected_init,
    mvp_compiled_fully_connected_prepare,
    mvp_compiled_fully_connected_invoke
  );
}

} // namespace tflite