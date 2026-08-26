#include "tflite_micro_model_config.h"
#pragma once

#include <cstdint>
#include <cstdarg>

#include "ml/tflite_micro_model/tflite_micro_accelerator.hpp"


namespace npu_toolkit
{

/**
 * @addtogroup tflite_micro_model
 * @defgroup tflite_micro_accelerator_mvpv1
 * @{
 */


 /**
  * MVPv1 accelerator implementation
  */
class TfliteMicroMvpv1Accelerator : public TfliteMicroAccelerator
{
public:
  static TfliteMicroMvpv1Accelerator& instance()
  {
    return *static_cast<TfliteMicroMvpv1Accelerator*>(
      get_tflite_micro_accelerator()
    );
  }

  const char* name() const override;
  const char* description() const override;
  bool init() override;
  void deinit(TfLiteContext *context) override;
  TfliteMicroModelContext* create_context(
    TfLiteContext *context
  ) override;

  /**
   * Return if the model has been compiled
   */
  static bool is_model_compiled(TfLiteContext *context);

  /**
   * Return if the current ML model layer has pre-compiled data in the .tflite model file
  */
  static bool is_current_layer_compiled(TfLiteContext *context);

  /**
   * Prepare the accelerator for compiled kernel execution.
   *
   * This is invoked at the beginning of compiled_model_process_layer_with_accelerator()
   */
  void prepare_compiled_execution();

  /**
   * Execute the MVP program(s) for the current layer.
   *
   * This is invoked in compiled_model_process_layer_with_accelerator() after the accelerator
   * registers have been populated for the current program.
   *
   * If wait_for_completion=true, then this function should not return until the MVP program(s)
   * have completed execution.
   */
 void execute_program(bool wait_for_completion=true);

  /**
   * Wait for the MVP program execution to complete by waiting for the appropriate interrupt(s).
   */
  void wait_for_idle();

  /**
   * Finalize the accelerator after compiled kernel execution.
   *
   * This is invoked at the end of compiled_model_process_layer_with_accelerator(),
   * after all MVP programs for the current layer have been executed.
   */
  void finalize_compiled_execution();

   int program_counter = 0;

protected:
  TfliteMicroMvpv1Accelerator() = default;

  bool _is_initialized = false;

  friend TfliteMicroAccelerator* get_tflite_micro_accelerator();
};

} // namespace npu_toolkit

