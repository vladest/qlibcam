#include "tensorflowtpuneuralnetwork.h"

#include "tensorflow/lite/kernels/internal/tensor.h"
#include "tensorflow/lite/kernels/internal/tensor_utils.h"

#include <QFile>
#include <QDebug>
#include <QMatrix4x4>

bool TensorFlowTPUNeuralNetwork::cocoReadLabels(const QString& fileName)
{
    QFile textFile(fileName);

    if (textFile.exists()) {
        QByteArray line;

        //labels.resize(100);
        textFile.open(QIODevice::ReadOnly);

        int index = 0;
        line = textFile.readLine().trimmed();
        while(!line.isEmpty()) {
            m_classes.insert(index++, line);
            line = textFile.readLine().trimmed();
        }

        textFile.close();
        return true;
    }

    return false;
}

std::unique_ptr<tflite::Interpreter> BuildEdgeTpuInterpreter(
        const tflite::FlatBufferModel& model,
        edgetpu::EdgeTpuContext* edgetpu_context) {
    tflite::ops::builtin::BuiltinOpResolver resolver;
    resolver.AddCustom(edgetpu::kCustomOp, edgetpu::RegisterCustomOp());
    std::unique_ptr<tflite::Interpreter> interpreter;
    if (tflite::InterpreterBuilder(model, resolver)(&interpreter) != kTfLiteOk) {
        qWarning() << "Failed to build interpreter.";
    }
    // Bind given context with interpreter.
    interpreter->SetExternalContext(kTfLiteEdgeTpuContext, edgetpu_context);
    interpreter->SetNumThreads(1);
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        qWarning() << "Failed to allocate tensors.";
    }
    return interpreter;
}

bool TensorFlowTPUNeuralNetwork::getClassfierOutputsTFLite(std::vector<std::pair<float, int>> *top_results)
{
    const int    output_size = 1000;
    const size_t num_results = 5;

    // Assume one output
    if (m_interpreter->outputs().size() > 0) {
        int output = m_interpreter->outputs()[0];

        switch (m_interpreter->tensor(output)->type) {
        case kTfLiteFloat32: {
            get_top_n<float>(m_interpreter->typed_output_tensor<float>(0), output_size,
                                                  num_results, m_threshold, top_results, true);
            break;
        }
        case kTfLiteUInt8: {
            get_top_n<uint8_t>(m_interpreter->typed_output_tensor<uint8_t>(0),
                                                    output_size, num_results, m_threshold, top_results,false);
            break;
        }
        default: {
            qDebug() << "Cannot handle output type" << m_interpreter->tensor(output)->type << "yet";
            return false;
        }
        }
        return true;
    }
    return false;
}

template<typename T>
T* TensorData(TfLiteTensor* tensor, int batch_index);

template<>
float* TensorData(TfLiteTensor* tensor, int batch_index) {
    int nelems = 1;
    for (int i = 1; i < tensor->dims->size; i++) nelems *= tensor->dims->data[i];
    switch (tensor->type) {
    case kTfLiteFloat32:
        return tensor->data.f + nelems * batch_index;
    default:
        qDebug() << "Should not reach here!";
    }
    return nullptr;
}

template<>
uint8_t* TensorData(TfLiteTensor* tensor, int batch_index) {
    int nelems = 0;
    for (int i = 1; i < tensor->dims->size; i++) nelems *= tensor->dims->data[i];
    switch (tensor->type) {
    case kTfLiteUInt8:
        return tensor->data.uint8 + nelems * batch_index;
    default:
        qDebug() << "Should not reach here!";
    }
    return nullptr;
}

template <class T>
void formatImageTFLite(T* out, const uint8_t* in,
                       int image_height, int image_width, int image_channels,
                       int wanted_height, int wanted_width, int wanted_channels,
                       bool input_floating)
{
    const float input_mean = 127.5f;
    const float input_std  = 127.5f;

    int number_of_pixels = image_height * image_width * image_channels;
    std::unique_ptr<Interpreter> interpreter(new Interpreter);

    int base_index = 0;

    // two inputs: input and new_sizes
    interpreter->AddTensors(2, &base_index);

    // one output
    interpreter->AddTensors(1, &base_index);

    // set input and output tensors
    interpreter->SetInputs({0, 1});
    interpreter->SetOutputs({2});

    // set parameters of tensors
    TfLiteQuantizationParams quant;
    interpreter->SetTensorParametersReadWrite(0, kTfLiteFloat32, "input",
    { 1, image_height, image_width, image_channels }, quant);
    interpreter->SetTensorParametersReadWrite(1, kTfLiteInt32,   "new_size", { 2 }, quant);
    interpreter->SetTensorParametersReadWrite(2, kTfLiteFloat32, "output",
    { 1, wanted_height, wanted_width, wanted_channels }, quant);

    ops::builtin::BuiltinOpResolver resolver;
    const TfLiteRegistration *resize_op = resolver.FindOp(BuiltinOperator_RESIZE_BILINEAR, 1);
    auto* params = reinterpret_cast<TfLiteResizeBilinearParams*>(malloc(sizeof(TfLiteResizeBilinearParams)));
    params->align_corners = false;
    interpreter->AddNodeWithParameters({0, 1}, {2}, nullptr, 0, params, resize_op, nullptr);
    interpreter->AllocateTensors();


    // fill input image
    // in[] are integers, cannot do memcpy() directly
    auto input = interpreter->typed_tensor<float>(0);
    for (int i = 0; i < number_of_pixels; i++)
        input[i] = in[i];

    // fill new_sizes
    interpreter->typed_tensor<int>(1)[0] = wanted_height;
    interpreter->typed_tensor<int>(1)[1] = wanted_width;

    interpreter->Invoke();

    auto output = interpreter->typed_tensor<float>(2);
    auto output_number_of_pixels = wanted_height * wanted_height * wanted_channels;

    for (int i = 0; i < output_number_of_pixels; i++) {
        if (input_floating)
            out[i] = (output[i] - input_mean) / input_std;
        else
            out[i] = (uint8_t)output[i];
    }
}

bool TensorFlowTPUNeuralNetwork::setInputsTFLite(const QImage& image)
{
    // Get inputs
    std::vector<int> inputs = m_interpreter->inputs();
    // Store original image properties
    img_width    = image.width();
    img_height   = image.height();
    img_channels = 3;
    // Set inputs
    for(unsigned int i=0; i<m_interpreter->inputs().size(); i++) {
        int input = inputs[i];

        // Convert input
        switch (m_interpreter->tensor(input)->type) {
        case kTfLiteFloat32:
        {
            formatImageTFLite<float>(m_interpreter->typed_tensor<float>(input), image.bits(),
                                     image.height(), image.width(), img_channels,
                                     wanted_height, wanted_width, wanted_channels, true);
            //formatImageQt<float>(interpreter->typed_tensor<float>(input),image,img_channels,
            //                     wanted_height,wanted_width,wanted_channels,true,true);
            break;
        }
        case kTfLiteUInt8:
        {
            formatImageTFLite<uint8_t>(m_interpreter->typed_tensor<uint8_t>(input),image.bits(),
                                       img_height, img_width, img_channels,
                                       wanted_height, wanted_width, wanted_channels, false);

            //formatImageQt<uint8_t>(interpreter->typed_tensor<uint8_t>(input),image,img_channels,
            //                       wanted_height,wanted_width,wanted_channels,false);
            break;
        }
        default:
        {
            qDebug() << "Cannot handle input type" << m_interpreter->tensor(input)->type << "yet";
            return false;
        }
        }
    }

    return true;
}

QImage rotateImage(QImage img, double rotation)
{
    QPoint center = img.rect().center();
    QTransform matrix;
    matrix.translate(center.x(), center.y());
    matrix.rotate(rotation);

    return img.transformed(matrix);
}

bool TensorFlowTPUNeuralNetwork::getObjectOutputsTFLite()
{
    if (outputs.size() >= 4) {
        const int    num_detections    = *TensorData<float>(outputs[3], 0);
        const float* detection_classes =  TensorData<float>(outputs[1], 0);
        const float* detection_scores  =  TensorData<float>(outputs[2], 0);
        const float* detection_boxes   =  TensorData<float>(outputs[0], 0);
        const float* detection_masks   =  /*!has_detection_masks || */outputs.size() < 5 ?
                    nullptr : TensorData<float>(outputs[4], 0);

        const auto ck = (float)m_classes.size() / (float)QColor::colorNames().size();
        for (int i = 0; i < num_detections; i++) {

            // Get score
            const float score = detection_scores[i];
            // Check minimum score
            if (score < m_threshold)
                break;

            DetectionResult det_result;

            // Get class
            const int cls = detection_classes[i];

            // Ignore first one
            //if (cls == 0) continue;
            // Get class label
            const QString label = m_classes.value(cls);
            det_result.color = QColor(QColor::colorNames().at(int(cls * ck)));

            //qDebug() << "detection class" << label << score;

            // Get coordinates
            const float top    = detection_boxes[4 * i]     * img_height;
            const float left   = detection_boxes[4 * i + 1] * img_width;
            const float bottom = detection_boxes[4 * i + 2] * img_height;
            const float right  = detection_boxes[4 * i + 3] * img_width;

            // Save coordinates
            QRectF box(left,top,right-left,bottom-top);

            //qDebug() << "box" << box << "label" << label << "score" << score << "color" << det_result.color << cls;
            // Get masks
            if (detection_masks != nullptr) {
                const int dim1 = outputs[4]->dims->data[2];
                const int dim2 = outputs[4]->dims->data[3];
                QImage mask(dim1,dim2,QImage::Format_ARGB32_Premultiplied);

                // Set binary mask [dim1,dim2]
                for(int j = 0; j < mask.height(); j++)
                    for(int k = 0; k < mask.width(); k++)
                        mask.setPixel(k, j, detection_masks[i*dim1*dim2 + j*dim2 + k] >= MASK_THRESHOLD ?
                                    det_result.color.rgba() : QColor(Qt::transparent).rgba());

                // Scale mask to box size
                det_result.mask = mask.scaled(box.width(), box.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
            }

            // Save remaining data

            det_result.label = label;
            det_result.confidence = score;
            det_result.objectRect = box;
            m_results.append(det_result);
        }

        return true;
    }
    return false;
}

TensorFlowTPUNeuralNetwork::TensorFlowTPUNeuralNetwork()
{
}

bool TensorFlowTPUNeuralNetwork::init(const QString &modelFile, const QString &labelsFile, const QString &configurationFile)
{
    m_initialized = false;
    Q_UNUSED(configurationFile)
    try {
        // Open model & assign error reporter
        m_model = FlatBufferModel::BuildFromFile(modelFile.toLatin1().data(), &error_reporter);

        if(m_model == nullptr) {
            qWarning() << "TensorFlow model loading: ERROR";
            return false;
        }
        edgetpu::EdgeTpuManager::GetSingleton()->SetVerbosity(0);

        m_tpuContext = edgetpu::EdgeTpuManager::GetSingleton()->NewEdgeTpuContext().release();
        if (m_tpuContext == nullptr) {
            qWarning() << "Cant acqure TensorFlow TPU context";
            return false;
        }

        m_interpreter = BuildEdgeTpuInterpreter(*m_model.get(), m_tpuContext);

        if(m_interpreter->AllocateTensors() != kTfLiteOk) {
            qWarning() << "Allocate tensors: ERROR";
            return false;
        }

        // Set kind of network
        m_networkType = m_interpreter->outputs().size() > 1 ? TF_OBJECT_DETECTION : TF_IMAGE_CLASSIFIER;


        int i_size = m_interpreter->inputs().size();
        int o_size = m_interpreter->outputs().size();
        int t_size = m_interpreter->tensors_size();

        qDebug() << "tensors size: "  << t_size;
        qDebug() << "nodes size: "    << m_interpreter->nodes_size();
        qDebug() << "inputs: "        << i_size;
        qDebug() << "outputs: "       << o_size;

        for (int i = 0; i < i_size; i++)
            qDebug() << "input" << i << "name:" << m_interpreter->GetInputName(i) << ", type:" << m_interpreter->tensor(m_interpreter->inputs()[i])->type;

        for (int i = 0; i < o_size; i++)
            qDebug() << "output" << i << "name:" << m_interpreter->GetOutputName(i) << ", type:" << m_interpreter->tensor(m_interpreter->outputs()[i])->type;

        for (int i = 0; i < t_size; i++) {
            if (m_interpreter->tensor(i)->name)
                qDebug()  << i << ":" << m_interpreter->tensor(i)->name << ","
                          << m_interpreter->tensor(i)->bytes << ","
                          << m_interpreter->tensor(i)->type << ","
                          << m_interpreter->tensor(i)->params.scale << ","
                          << m_interpreter->tensor(i)->params.zero_point;
        }

        // Get input dimension from the input tensor metadata
        // Assuming one input only
        const int input = m_interpreter->inputs()[0];
        TfLiteIntArray* dims = m_interpreter->tensor(input)->dims;

        // Save outputs
        outputs.clear();
        for(unsigned int i=0;i < m_interpreter->outputs().size();i++)
            outputs.push_back(m_interpreter->tensor(m_interpreter->outputs()[i]));

        wanted_height   = dims->data[1];
        wanted_width    = dims->data[2];
        wanted_channels = dims->data[3];


        qDebug() << "Wanted height:"   << wanted_height;
        qDebug() << "Wanted width:"    << wanted_width;
        qDebug() << "Wanted channels:" << wanted_channels;

        //        if (numThreads > 1)
        //            interpreter->SetNumThreads(numThreads);

        // Read labels
        if (cocoReadLabels(labelsFile))
            qDebug() << "There are" << m_classes.count() << "labels.";
        else qDebug() << "There are NO labels";

        qDebug() << "Tensorflow initialization: OK";
        m_initialized = true;
        return true;

    } catch(...) {
        qWarning() << "Exception loading model";
        return false;
    }
}

bool TensorFlowTPUNeuralNetwork::setInputImage(QVideoFrame& input, bool flip)
{
    Q_UNUSED(flip)

    QImage img = input.toImage();

    const auto surfaceFormat = input.surfaceFormat();

    bool mirrorHorizontal = false;
    bool mirrorVertical = false;

    if (!mirrorVertical)
        mirrorVertical = surfaceFormat.isMirrored();
    mirrorHorizontal = surfaceFormat.scanLineDirection() == QVideoFrameFormat::BottomToTop;

    if (!img.isNull()) {
        img = img.convertToFormat(QImage::Format_RGB888);
        //qDebug() << img.pixelFormat().channelCount();
        //img = rotateImage(img, -180);
        setInputsTFLite(img);
    } else {
        qWarning() << "converted image not valid";
        return false;
    }
    return true;
}

bool TensorFlowTPUNeuralNetwork::process()
{
    if (m_interpreter->Invoke() != kTfLiteOk) {
        qDebug() << "Failed to invoke interpreter";
        return false;
    }
    m_results.clear();
    // Image classifier
    if (m_networkType == TF_IMAGE_CLASSIFIER) {
        std::vector<std::pair<float, int>> top_results;

        getClassfierOutputsTFLite(&top_results);

        for (const auto& result : top_results) {
            DetectionResult det_result;
            det_result.label = m_classes.value(result.second);
            det_result.confidence = result.first;
            m_results.append(det_result);
        }
    } else if (m_networkType == TF_OBJECT_DETECTION) {             // Object detection

        getObjectOutputsTFLite();
    }
    return true;
}

AbstractNeuralNetwork::NeuralNetworksBackend TensorFlowTPUNeuralNetwork::type()
{
    return TensorFlow;
}
