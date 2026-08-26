#include "tflite_micro_model_config.h"
/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "ml/third_party/tflm/reshape.h"

#include <cstring>

#include "ml/third_party/tflm/tlc-builtin_op_data.h"
#include "ml/third_party/tflm/common.h"
#include "ml/third_party/tflm/tensor_ctypes.h"
#include "ml/third_party/tflm/kernel_util.h"
#include "ml/third_party/tflm/op_macros.h"
#include "ml/third_party/tflm/tlmk-kernel_util.h"
#include "ml/third_party/tflm/memory_helpers.h"
#include "ml/third_party/tflm/micro_utils.h"

namespace tflite {

TfLiteStatus EvalReshapeReference(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteEvalTensor* input =
      tflite::micro::GetEvalInput(context, node, kReshapeInputTensor);
  TfLiteEvalTensor* output =
      tflite::micro::GetEvalOutput(context, node, kReshapeOutputTensor);

  // TODO(b/162522304): storing input bytes in OpData increases some models
  // significantly, possibly due to alignment issues.
  size_t input_bytes;
  TF_LITE_ENSURE_STATUS(TfLiteTypeSizeOf(input->type, &input_bytes));
  input_bytes *= ElementCount(*input->dims);

  // Do nothing for in-place reshape.
  if (input->data.raw != output->data.raw) {
    // Otherwise perform reshape with copy.
    memcpy(output->data.raw, input->data.raw, input_bytes);
  }
  return kTfLiteOk;
}

#if !defined(TFLITE_MICRO_SW_REF_KERNEL_REGISTRATIONS_DISABLED) && !defined(TFLITE_MICRO_SW_REF_RESHAPE_KERNEL_REGISTRATION_DISABLED)
TFLMRegistration Register_RESHAPE() {
  return tflite::micro::RegisterOp(nullptr, PrepareReshapeReference,
                                   EvalReshapeReference);
}
#endif

}  // namespace tflite
