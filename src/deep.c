#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SIZE_CLASSES 10
#define SIZE_MINI_BATCH 100
#define SIZE_OUTPUT 10
#define SIZE_HIDDEN 32

typedef struct {
    unsigned char *images, *labels;
    int nImages, rows, cols;
} InputData;


typedef struct Layer{
    float *weights, *biases, *inputs, *outputs;
    size_t size_inputs, size_neurons;
    void (*activation_forward)(struct Layer* layer, float *input, float *output, size_t size_batch);
    void (*activation_backward)(struct Layer* layer, float *dout, float *din);
} Layer;

typedef struct {
    Layer **layers;
    size_t size_layers;
} Model;

typedef struct {
    float *activations;
    size_t size_activations;
} Activations;



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to concatenate two strings and return the new string
char *concatStrings(const char *str1, const char *str2) {
    // Allocate memory for the new string
    char *newStr = malloc(strlen(str1) + strlen(str2) + 1);  // +1 for null terminator
    if (newStr == NULL) {
        printf("Memory allocation failed\n");
        return NULL;  // Return NULL if memory allocation fails
    }

    // Copy the first string
    strcpy(newStr, str1);

    // Append the second string
    strcat(newStr, str2);

    return newStr;  // Return the concatenated string
}



void matmul_forward(Layer *layer, float *input, float *output, size_t size_batch)
{
    for (size_t idx_image = 0; idx_image < size_batch; idx_image++) {
        for (size_t idx_neuron = 0; idx_neuron < layer->size_neurons; idx_neuron++) {
            output[idx_image * layer->size_neurons + idx_neuron] = layer->biases[idx_neuron];
            for (size_t idx_input = 0; idx_input < layer->size_inputs; idx_input++) {
                output[idx_image * layer->size_neurons + idx_neuron] += 
                layer->weights[idx_input * layer->size_neurons + idx_neuron] * input[idx_image * layer->size_inputs + idx_input];
            }
        }
    }
} 


void relu_forward(Layer *layer, float *input, float *output, size_t size_batch)
{
    for (size_t idx_image = 0; idx_image < size_batch; idx_image++) {
        for (size_t idx_neuron = 0; idx_neuron < layer->size_neurons; idx_neuron++) {
            output[idx_image * layer->size_neurons + idx_neuron] = fmaxf(0.0f, input[idx_image * layer->size_neurons + idx_neuron]);
        }
    }
}

void softmax_forward(Layer *layer, float *logits, float *probs, size_t size_batch)
{
    for (size_t idx_image = 0; idx_image < size_batch; idx_image++) {
        float max_logit = logits[idx_image * layer->size_neurons];
        for (size_t idx_neuron = 1; idx_neuron < layer->size_neurons; idx_neuron++) {
            if (logits[idx_image * layer->size_neurons + idx_neuron] > max_logit) {
                max_logit = logits[idx_image * layer->size_neurons + idx_neuron];
            }
        }

        float sum_exp = 0.0f;
        for (size_t idx_neuron = 0; idx_neuron < layer->size_neurons; idx_neuron++) {
            probs[idx_image * layer->size_neurons + idx_neuron] = expf(logits[idx_image * layer->size_neurons + idx_neuron] - max_logit);
            sum_exp += probs[idx_image * layer->size_neurons + idx_neuron];
        }

        for (size_t idx_neuron = 0; idx_neuron < layer->size_neurons; idx_neuron++) {
            probs[idx_image * layer->size_neurons + idx_neuron] /= sum_exp;
        }
    }
}

void read_mnist_images(const char *filename, InputData *data) {
    FILE *file = fopen(filename, "rb");
    if (!file) exit(1);

    int temp;
    fread(&temp, sizeof(int), 1, file);
    fread(&data->nImages, sizeof(int), 1, file);
    data->nImages = __builtin_bswap32(data->nImages);

    fread(&data->rows, sizeof(int), 1, file);
    fread(&data->cols, sizeof(int), 1, file);

    data->rows = __builtin_bswap32(data->rows);
    data->cols = __builtin_bswap32(data->cols);
    printf("rows: %d, cols: %d\n", data->rows, data->cols);

    data->images = malloc(data->nImages * data->rows * data->cols);
    fread(data->images, sizeof(unsigned char), data->nImages * data->rows * data->cols, file);
    fclose(file);

}


void read_mnist_labels(const char *filename, unsigned char **labels, int *nLabels) {
    FILE *file = fopen(filename, "rb");
    if (!file) exit(1);

    int temp;
    fread(&temp, sizeof(int), 1, file);
    fread(nLabels, sizeof(int), 1, file);
    *nLabels = __builtin_bswap32(*nLabels);

    *labels = malloc((*nLabels) * sizeof(unsigned char));
    fread(*labels, sizeof(unsigned char), (*nLabels), file);
    fclose(file);
}

float generate_normal_random_number()
{
    // 2 uniformly distributed random numbers between 0 and 1
    float u1 = ((float)rand() / RAND_MAX);
    float u2 = ((float)rand() / RAND_MAX);

    // use Box-Muller to return a normally distributed number
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

// Xavier (Glorot) Normal Initialization
float generate_xavier_number(size_t inputs, size_t outputs)
{
    float stddev = sqrt(2.0f / (inputs + outputs));
    return generate_normal_random_number() * stddev;
}

// Kaiming Normal Initialization
float generate_kaiming_number(size_t inputs, size_t outputs)
{
    float stddev = sqrt(2.0f / inputs);
    return generate_normal_random_number() * stddev;
}

void kaiming_initialize_layer(Layer *layer, size_t inputs, size_t outputs)
{
    layer->size_inputs = inputs;
    layer->size_neurons = outputs;

    layer->activation_forward = relu_forward;

    layer->weights = malloc(inputs * outputs * sizeof(float));
    layer->biases = malloc(outputs * sizeof(float));

    for (size_t i = 0; i < inputs * outputs; i++)
        layer->weights[i] = generate_xavier_number(inputs, outputs);

    for (size_t i = 0; i < outputs; i++)
        layer->biases[i] = 0.0f;
}

void xavier_initialize_layer(Layer *layer, size_t inputs, size_t outputs)
{
    layer->size_inputs = inputs;
    layer->size_neurons = outputs;

    layer->activation_forward = softmax_forward;

    layer->weights = malloc(inputs * outputs * sizeof(float));
    layer->biases = malloc(outputs * sizeof(float));

    for (size_t i = 0; i < inputs * outputs; i++)
        layer->weights[i] = generate_xavier_number(inputs, outputs);

    for (size_t i = 0; i < outputs; i++)
        layer->biases[i] = 0.0f;
}

void add_layer(Model *model, Layer *layer)
{
    if (model->size_layers == 0) {
        model->layers = malloc(sizeof(Layer *));
    } else {
        model->layers = realloc(model->layers, (model->size_layers + 1) * sizeof(Layer *));
    }
    model->layers[model->size_layers] = layer;
    model->size_layers++;
}

void print_layer(Layer *layer)
{
    printf("Layer: %zu inputs, %zu outputs\n", layer->size_inputs, layer->size_neurons);
    printf("Weights:\n");
    for (size_t i = 0; i < layer->size_inputs * layer->size_neurons; i++)
        printf("%f ", layer->weights[i]);
    printf("\nBiases:\n");
    for (size_t i = 0; i < layer->size_neurons; i++)
        printf("%f ", layer->biases[i]);
    printf("\n");
}

void free_layer(Layer *layer)
{
    free(layer->weights);
    free(layer->biases);
}

void free_model(Model *model)
{
    for (size_t i = 0; i < model->size_layers; i++)
      free_layer(model->layers[i]);
    free(model->layers);
}

void print_probs(Model *model, Activations *activations, InputData *data)
{
    printf("Probabilities:\n");
    float *probs = model->layers[model->size_layers - 1]->outputs;
    float prob_sum = 0.0f;
    for (size_t idx_image = 0; idx_image < 2; idx_image++) {
        size_t start_sample = idx_image * SIZE_CLASSES;
        prob_sum = 0.0f;
        for (size_t idx_prob = 0; idx_prob < SIZE_CLASSES; idx_prob++) {
            prob_sum += probs[start_sample + idx_prob];
            printf("%f ", probs[start_sample + idx_prob]);
        }
        printf("\n");
        printf("\nprob sum = %f\n", prob_sum);
    }
}


void model_forward(Model *model, Activations *activations, InputData *data)
{
    printf("\nModel forward pass...\n");
    for (size_t idx_layer = 0; idx_layer < model->size_layers; idx_layer++) {
        Layer *layer = model->layers[idx_layer];
        matmul_forward(layer, layer->inputs, layer->outputs, data->nImages);
        layer->activation_forward(layer, layer->outputs, layer->outputs, data->nImages);
    }
} 

float get_loss(Model *model, Activations *activations, InputData *data)
{
    float loss = 0.0f;
    float *probs = model->layers[model->size_layers - 1]->outputs;
    for (size_t idx_image = 0; idx_image < data->nImages; idx_image++) {
        unsigned char label = data->labels[idx_image];
        size_t start_sample = idx_image * SIZE_CLASSES;
        loss -= logf(probs[start_sample + label]);
    }
    return loss / data->nImages;
}


size_t get_size_parameters(Model *model)
{
    size_t size = 0;
    for (size_t i = 0; i < model->size_layers; i++) {
        size += model->layers[i]->size_inputs * model->layers[i]->size_neurons;
        size += model->layers[i]->size_neurons;
    }
    return size;
}

void initialise_activations(Activations * activations, Model *model, InputData *data)
{
    for (size_t i = 0; i < model->size_layers; i++) {
        activations->size_activations += model->layers[i]->size_neurons;
    }
    activations->size_activations += data->rows * data->cols;
    activations->size_activations *= data->nImages;
    activations->activations = calloc(activations->size_activations, sizeof(float));

    for (size_t idx_pixel = 0; idx_pixel < data->nImages * data->rows * data->cols; idx_pixel++) {
        activations->activations[idx_pixel] = (float)data->images[idx_pixel] / 255.0f;
    }

    float *inputs = activations->activations;
    float *outputs = activations->activations + data->rows * data->cols;

    for (size_t idx_layer = 0; idx_layer < model->size_layers; idx_layer++) {
        Layer *layer = model->layers[idx_layer];
        layer->inputs = idx_layer == 0 ? inputs : model->layers[idx_layer - 1]->outputs;
        layer->outputs = idx_layer == 0? outputs : outputs + model->layers[idx_layer -1]->size_neurons * data->nImages;
    }
}

void free_activations(Activations * activations)
{
    free(activations->activations);
}


int main() {
    // read input data
    InputData data_training, data_test = {0};

    const char * data_path = getenv("DATA_PATH");
    
    const char * training_images_path = concatStrings(data_path, "/train-images.idx3-ubyte");
    const char *training_labels_path = concatStrings(data_path, "/train-labels.idx1-ubyte");
    read_mnist_images(training_images_path, &data_training);
    read_mnist_labels(training_labels_path, &data_training.labels, &data_training.nImages);
    printf("Number of training images: %d\n", data_training.nImages);

    const char *test_images_path = concatStrings(data_path, "/t10k-images.idx3-ubyte");
    const char *test_labels_path = concatStrings(data_path, "/t10k-labels.idx1-ubyte");
    read_mnist_images(test_images_path, &data_test);
    read_mnist_labels(test_labels_path, &data_test.labels, &data_test.nImages);
    printf("Number of test images: %d\n", data_test.nImages);

    // create model
    Model model = {0};
    // create layers
    Layer layer_hidden, layer_output;
    kaiming_initialize_layer(&layer_hidden, data_test.cols * data_test.rows, SIZE_HIDDEN);
    xavier_initialize_layer(&layer_output, SIZE_HIDDEN, SIZE_OUTPUT);
    // add layers to model
    add_layer(&model, &layer_hidden);
    add_layer(&model, &layer_output);
    printf("Number of parameters: %zu\n", get_size_parameters(&model));
    // test loss before training
    Activations activations = {0};
    initialise_activations(&activations, &model, &data_test);
    model_forward(&model, &activations, &data_test);
    printf("Test loss before training: %f\n", get_loss(&model, &activations, &data_test));
    // free activations
    free_activations(&activations);
    // free model
    free_model(&model);
    // free input data
    free(data_training.images);
    free(data_training.labels);
    free(data_test.images);
    free(data_test.labels);

    return 0;
}