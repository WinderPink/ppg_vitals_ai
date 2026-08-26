#include "tflite_micro_model_config.h"
#include "ml/third_party/tflm/kernel_util.h"
#include "ml/accelerator/mvpv1/conv2d_compiled.hpp"
#include "ml/compiled_model/compiled_model.hpp"
#include "ml/accelerator/mvpv1/compiled_registration_helper.hpp"
#include "ml/accelerator/mvpv1/mvpv1_kernel_config.hpp"
#include "ml/accelerator/mvpv1/tflite_micro_mvpv1_context.hpp"
#include "ml/tflite_micro_model/tflite_micro_model_helper.hpp"


namespace tflite {

using namespace npu_toolkit;

struct OpData
{
  float16_t* bias;
  float16_t* per_channel_output_scaler;
  int scratch_buffer_index;
};


namespace
{

TfLiteStatus mvp_prepare(TfLiteContext* context, TfLiteNode* node)
{
  OpData& data = *static_cast<OpData*>(node->user_data);
  MicroContext* micro_context = GetMicroContext(context);
  TfLiteTensor* output =
      micro_context->AllocateTempOutputTensor(node, kOutputTensor);
  TfLiteTensor* bias = (node->inputs->size == 3) ?
      micro_context->AllocateTempInputTensor(node, kBiasTensor) : nullptr;
  TfLiteTensor* input =
      micro_context->AllocateTempInputTensor(node, kInputTensor);
  TfLiteTensor* filter =
      micro_context->AllocateTempInputTensor(node, kFilterTensor);

  const auto input_shape = GetTensorShape(input);
  const int num_channels = filter->dims->data[kConvQuantizedDimension];

  TF_LITE_ENSURE_STATUS(allocate_scaled_bias_tensor(context, bias, &data.bias));

  TF_LITE_ENSURE_STATUS(allocate_output_multiplier_buffer(
      context, input, filter, output,
      num_channels,
      &data.per_channel_output_scaler
  ));

  // NOTE: We always request a scratch buffer to hold the scaled (int8 -> float16) input
  //       If the model has a memory plan, then this scratch buffer may actually point to the input buffer.
  const int input_flat_size = input_shape.FlatSize();
  TF_LITE_ENSURE_STATUS(context->RequestScratchBufferInArena(
      context,
      sizeof(float16_t)*input_flat_size,
      &data.scratch_buffer_index
  ));

  return kTfLiteOk;
}


TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const float16_t zero_data(0);
  const OpData& data = *(static_cast<const OpData*>(node->user_data));
  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;
  auto array_base_addresses2 = mvpv1_context.array_base_addresses2;

  auto scaled_input_buffer =
      NPU_TOOLKIT_GET_SCRATCH_BUFFER(float16_t, data.scratch_buffer_index);

  const auto input = tflite::micro::GetEvalInput(context, node, kInputTensor);
  const auto filter = tflite::micro::GetEvalInput(context, node, kFilterTensor);
  const auto bias   = NumInputs(node) == 3
                      ? tflite::micro::GetEvalInput(context, node, kBiasTensor)
                      : nullptr;
        auto output       = tflite::micro::GetEvalOutput(context, node, kOutputTensor);

  auto input_data     = tflite::micro::GetTensorData<int8_t>(input);
  auto filter_data   = tflite::micro::GetTensorData<int8_t>(filter);
  auto output_data    = tflite::micro::GetTensorData<int8_t>(output);
  const auto bias_data = (data.bias != nullptr) ? data.bias :
      tflite::micro::GetOptionalTensorData<float16_t>(bias);

  array_base_addresses[MVP_SCALED_INPUT_ARRAY] = scaled_input_buffer != nullptr ?
    (uintptr_t)scaled_input_buffer : (uintptr_t)input_data;
  array_base_addresses[MVP_INPUT_ARRAY] = (uintptr_t)input_data;
  array_base_addresses[MVP_FILTER_ARRAY] = (uintptr_t)filter_data;
  array_base_addresses[MVP_BIAS_ARRAY] = (uintptr_t)((bias_data == nullptr) ? &zero_data : bias_data);
  array_base_addresses[MVP_OUTPUT_SCALER_ARRAY] = (uintptr_t)data.per_channel_output_scaler;

  memcpy(array_base_addresses2, array_base_addresses, sizeof(mvpv1_context.array_base_addresses));

  array_base_addresses2[MVP_INPUT_ARRAY] = array_base_addresses[MVP_SCALED_INPUT_ARRAY];
  array_base_addresses2[MVP_OUTPUT_ARRAY] = (uintptr_t)output_data;

  mvpv1_context.update_array_base_addresses_callback = [](
    TfliteMicroMvpv1Context& ctx
  )
  {
    auto array_base_addresses = ctx.array_base_addresses;
    auto array_base_addresses2 = ctx.array_base_addresses2;
    memcpy(array_base_addresses, array_base_addresses2, sizeof(ctx.array_base_addresses));
  };

  return compiled_model_process_layer_with_accelerator(context);
}

} //namespace




void* mvp_compiled_conv2d_init(TfLiteContext* context, const char* buffer, size_t length)
{
  return compiled_registration_init(
    context,
    buffer,
    length,
    sizeof(OpData)
    #if MVPV1_KERNELS_CONV2D_FALLBACK_ENABLED == 1
    ,cmsis_nn_conv_get_init_size()
    #endif
  );
}


TfLiteStatus mvp_compiled_conv2d_prepare(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_prepare(
    context,
    node,
    mvp_prepare
    #if MVPV1_KERNELS_CONV2D_FALLBACK_ENABLED == 1
    ,cmsis_nn_conv_prepare
    #endif
  );
}

TfLiteStatus mvp_compiled_conv2d_invoke(TfLiteContext* context, TfLiteNode* node)
{
  return compiled_registration_invoke(
    context,
    node,
    mvp_invoke
    #if MVPV1_KERNELS_CONV2D_FALLBACK_ENABLED == 1
    ,cmsis_nn_conv_invoke
    #endif
  );
}



// Define the this kernel's registeration if this is an embedded build
TFLMRegistration Register_CONV_2D() {
  return tflite::micro::RegisterOp(
    mvp_compiled_conv2d_init,
    mvp_compiled_conv2d_prepare,
    mvp_compiled_conv2d_invoke
  );
}



} // namespace tflite