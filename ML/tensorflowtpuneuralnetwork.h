#pragma once

#include <QMap>

#include "abstractneuralnetwork.h"

#include "tensorflow/lite/core/api/error_reporter.h"
#include "tensorflow/lite/core/interpreter.h"
#include "tensorflow/lite/core/model.h"
#include "tensorflow/lite/graph_info.h"
#include "tensorflow/lite/kernels/register.h"

#include <edgetpu.h>

#include <queue>

template <class T>
void get_top_n(T* prediction, int prediction_size, size_t num_results,
               float threshold, std::vector<std::pair<float, int>>* top_results,
               bool input_floating)
{
    // Will contain top N results in ascending order.
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>,
                        std::greater<std::pair<float, int>>>
        top_result_pq;

    const long count = prediction_size;  // NOLINT(runtime/int)
    for (int i = 0; i < count; ++i) {
      float value;
      if (input_floating)
        value = prediction[i];
      else
        value = prediction[i] / 255.0;
      // Only add it if it beats the threshold and has a chance at being in
      // the top N.
      if (value < threshold) {
        continue;
      }

      top_result_pq.push(std::pair<float, int>(value, i));

      // If at capacity, kick the smallest value out.
      if (top_result_pq.size() > num_results) {
        top_result_pq.pop();
      }
    }

    // Copy to output vector and reverse into descending order.
    while (!top_result_pq.empty()) {
      top_results->push_back(top_result_pq.top());
      top_result_pq.pop();
    }
    std::reverse(top_results->begin(), top_results->end());
}

// explicit instantiation so that we can use them otherwhere
template void get_top_n<uint8_t>(uint8_t*, int, size_t, float,
                                 std::vector<std::pair<float, int>>*, bool);
template void get_top_n<float>(float*, int, size_t, float,
                               std::vector<std::pair<float, int>>*, bool);

using namespace tflite;

class TensorFlowTPUNeuralNetwork : public AbstractNeuralNetwork
{
public:

    enum TensorFlowNetworkType {
        TF_OBJECT_DETECTION = 1,
        TF_IMAGE_CLASSIFIER
    };


    TensorFlowTPUNeuralNetwork();

    bool init(const QString& modelFile, const QString& labelsFile, const QString& configurationFile) override;
    bool setInputImage(QVideoFrame &input, bool flip) override;
    bool process() override;
    NeuralNetworksBackend type() override;

private:
    QMap<int, QString> m_classes;

    edgetpu::EdgeTpuContext* m_tpuContext = nullptr;
    // Model
    std::unique_ptr<FlatBufferModel> m_model;
    // Resolver
    tflite::ops::builtin::BuiltinOpResolver m_resolver;
    // Interpreter
    std::unique_ptr<Interpreter> m_interpreter;
    // Error reporter
    StderrReporter error_reporter;
    // Outputs
    std::vector<TfLiteTensor*> outputs;
    int wanted_height, wanted_width, wanted_channels;
    int img_height, img_width, img_channels;
    TensorFlowNetworkType m_networkType;
    // Threshold
    double m_threshold = 0.6;
    // Configuration constants
    const double MASK_THRESHOLD = 0.3;
    bool getObjectOutputsTFLite();
    bool getClassfierOutputsTFLite(std::vector<std::pair<float, int> > *top_results);
    bool cocoReadLabels(const QString &fileName);
    bool setInputsTFLite(const QImage &image);
};
