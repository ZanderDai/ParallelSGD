#include "neural_net.h"

feedForwardNN::feedForwardNN(int minibatchSize, int numLayer, int *numNeuronList, int *layerTypeList) {
	m_nMinibatchSize = minibatchSize;

	m_numLayer = numLayer;
	m_numNeuronList = new int[m_numLayer];
	m_layerTypeList = new int[m_numLayer];
	memcpy(m_numNeuronList, numNeuronList, sizeof(int) * m_numLayer);
	memcpy(m_layerTypeList, layerTypeList, sizeof(int) * m_numLayer);

	// Initialize layers
	for (int i=0; i<m_numLayer; i++) {
		int numNeuron = m_numNeuronList[i];
		int layerType = m_layerTypeList[i];
		layerBase *layer = initLayer(numNeuron, layerType);

		m_vecLayers.push_back(layer);
	}
	m_softmaxLayer = new softmaxLayer(m_numNeuronList[m_numLayer-1]);

	// Allocate memory to hold forward input to each non-input layer
	for (int connectIdx=0; connectIdx<m_numLayer-1; connectIdx++) {
		int numNeuron = m_numNeuronList[connectIdx+1];
		float * forwardInfo = new float[numNeuron];

		m_vecForwardInfo.push_back(forwardInfo);
	}

	// Allocate memory to hold backprop error to each non-output layer
	for (int connectIdx=0; connectIdx<m_numLayer-1; connectIdx++) {
		int numNeuron = m_numNeuronList[connectIdx];
		float * backpropInfo = new float[numNeuron];

		m_vecBackpropInfo.push_back(backpropInfo);
	}

	// Compute m_nParamSize
	m_nParamSize = 0;
	for (int connectIdx=0; connectIdx<m_numLayer-1; connectIdx++) {
		int fanIn = m_numNeuronList[connectIdx]+1;
		int fanOut = m_numNeuronList[connectIdx+1];

		m_nParamSize += fanIn*fanOut;
	}



}

feedForwardNN::~feedForwardNN() {
	if(!m_numNeuronList) {
		delete [] m_numNeuronList;
	}
	if(!m_layerTypeList) {
		delete [] m_layerTypeList;
	}
	for (std::vector<layerBase *>::iterator it = m_vecLayers.begin(); it != m_vecLayers.end(); ++it) {
		if(!(*it)) {
			delete *it;
		}
	}
	for (std::vector<float *>::iterator it = m_vecBackpropInfo.begin(); it != m_vecBackpropInfo.end(); ++it) {
		if(!(*it)) {
			delete [] *it;
		}
	}
	for (std::vector<float *>::iterator it = m_vecForwardInfo.begin(); it != m_vecForwardInfo.end(); ++it) {
		if(!(*it)) {
			delete [] *it;
		}
	}
}

layerBase *feedForwardNN::initLayer (int numNeuron, int layerType) {
	layerBase *layer;
	switch (layerType) {
		case 0:
			layer = new linearLayer(numNeuron);
			break;
		case 1:
			layer = new sigmoidLayer(numNeuron);
			break;		
		default:
			printf("Error in initLayer.");
			exit(-1);
	}
	return layer;
}

void feedForwardNN::initParams (float *params) {
	// Initialize values for weights
	float *cursor = params;
	for (int connectIdx=0; connectIdx<m_numLayer-1; ++connectIdx) {
		int fanIn = m_numNeuronList[connectIdx]+1;
		int fanOut = m_numNeuronList[connectIdx+1];

		float *weights = cursor;
		initWeights(weights, fanIn, fanOut);

		cursor += fanIn * fanOut;
	}
}

void feedForwardNN::initWeights (float *weights, int fanIn, int fanOut, int type) {
	switch(type) {
		case 0: {
			float multiplier = 4.f * sqrt(6.f / (float)(fanIn + fanOut));
			for (int i=0; i<fanIn*fanOut; i++) {
				weights[i] = multiplier * SYM_UNIFORM_RAND;
			}			
			break;
		}
		default: {
			printf("Error in initWeights.");
			exit(-1);
		}
	}
}

void feedForwardNN::feedForward (float *input) {	
	layerBase *inLayer, *outLayer;
	float *forwarInfo;
	float *weights;

	// input layer
	m_vecLayers[0]->activateFunc(input);

	// for each connection
	for (int connectIdx=0; connectIdx<m_numLayer-1; ++connectIdx) {

		// get fanIn and fanOut
		int fanIn = m_numNeuronList[connectIdx]+1;
		int fanOut = m_numNeuronList[connectIdx+1];

		// create ptr to corresponding weights
		weights = m_vecWeights[connectIdx];

		// create ptr to associated layers (in & out)
		inLayer = m_vecLayers[connectIdx];
		outLayer = m_vecLayers[connectIdx+1];

		// create ptr to associated forwardInfo & clean the buffer
		forwarInfo = m_vecForwardInfo[connectIdx];
		memset(forwarInfo, 0x00, sizeof(float)*m_numNeuronList[connectIdx+1]);

		// compute forwarInfo 		
		for (int in=0; in<fanIn-1; in++) {			
			float inAct = inLayer->m_activation[in];
			int wIdx = in * fanOut;
			for (int out=0; out<fanOut; out++) {
				forwarInfo[out] += inAct * weights[wIdx+out];
			}
		}
		// bias term
		int wIdx = (fanIn-1) * fanOut;
		for (int out=0; out<fanOut; out++) {
			forwarInfo[out] += weights[wIdx+out];
		}

		// outLayer compute activation
		outLayer->activateFunc(forwarInfo);
	}

	// softmax classification
	m_softmaxLayer->activateFunc(m_vecLayers[m_numLayer-1]->m_activation);
	// for (int i=0; i<m_numNeuronList[m_numLayer-1]; i++) {
	// 	printf("m_activation[%d]: %f\n", i, m_softmaxLayer->m_activation[i]);
	// }
}

void feedForwardNN::backProp (float *target) {
	layerBase *inLayer, *outLayer;
	float *backpropInfo;
	float *weights, *weightsGrad;

	// softmax layer
	m_softmaxLayer->computeDelta(target);
	// for (int i=0; i<m_numNeuronList[m_numLayer-1]; i++) {
	// 	printf("m_delta[%d]: %f\n", i, m_softmaxLayer->m_delta[i]);
	// 	printf("target[%d]: %f\n", i, target[i]);
	// }

	// computeDelta for output layer (error signal is different for output layer)
	m_vecLayers[m_numLayer-1]->computeDelta(m_softmaxLayer->m_delta);

	// for each connection
	for (int connectIdx=m_numLayer-2; connectIdx>=0; --connectIdx) {
		// get fanIn and fanOut
		int fanIn = m_numNeuronList[connectIdx]+1;
		int fanOut = m_numNeuronList[connectIdx+1];

		// create ptr to corresponding weights
		weights = m_vecWeights[connectIdx];

		// create ptr to associated layers (in & out)
		inLayer = m_vecLayers[connectIdx];
		outLayer = m_vecLayers[connectIdx+1];

		// create ptr to corresponding weightsGrad
		weightsGrad = m_vecWeightsGrad[connectIdx];
		
		// create ptr to associated forwardInfo & clean the buffer
		backpropInfo = m_vecForwardInfo[connectIdx];
		memset(backpropInfo, 0x00, sizeof(float)*m_numNeuronList[connectIdx]);

		// compute weightsGrad
		for (int in=0; in<fanIn-1; in++) {
			float inAct = inLayer->m_activation[in];
			int wIdx = in * fanOut;
			for (int out=0; out<fanOut; out++) {
				weightsGrad[wIdx+out] += inAct * outLayer->m_delta[out];
			}
		}
		// weightsGrad for bias term
		int wIdx = (fanIn-1) * fanOut;
		for (int out=0; out<fanOut; out++) {
			weightsGrad[wIdx+out] += outLayer->m_delta[out];
		}
		
		// compute backpropInfo
		for (int in=0; in<fanIn-1; in++) {
			int wIdx = in * fanOut;
			for (int out=0; out<fanOut; out++) {
				backpropInfo[in] += weights[wIdx+out] * outLayer->m_delta[out];
			}
		}

		// non-input inLayer computeDelta
		if (connectIdx > 0) {
			inLayer->computeDelta(backpropInfo);
		}
	}

}

float feedForwardNN::computeGrad (float *grad, float *params, float *data, float *label) {
	// bind weights
	float *paramsCursor = params;
	for (int connectIdx=0; connectIdx<m_numLayer-1; ++connectIdx) {
		int fanIn = m_numNeuronList[connectIdx]+1;
		int fanOut = m_numNeuronList[connectIdx+1];

		float *weights = paramsCursor;
		m_vecWeights.push_back(weights);

		paramsCursor += fanIn * fanOut;
	}

	// clean the grad buffer and bind it to 
	memset(grad, 0x00, sizeof(float)*m_nParamSize);
	float *gradCursor = grad;
	for (int connectIdx=0; connectIdx<m_numLayer-1; ++connectIdx) {
		int fanIn = m_numNeuronList[connectIdx]+1;
		int fanOut = m_numNeuronList[connectIdx+1];

		float *weightsGrad = gradCursor;		
		m_vecWeightsGrad.push_back(weightsGrad);

		gradCursor += fanIn * fanOut;
	}

	// compute grad by several forward pass and backward pass
	int dataDim = m_numNeuronList[0];
	int labelDim = m_numNeuronList[m_numLayer-1];
	float *dataCursor = data;
	float *labelCursor = label;
	float error = 0.f;
	
	for (int dataIdx=0; dataIdx<m_nMinibatchSize; dataIdx++) {
		feedForward(dataCursor);				
		backProp(labelCursor);
		for (int i=0; i<labelDim; i++) {
			error += labelCursor[i] * log(m_softmaxLayer->m_activation[i]);
		}		
		dataCursor += dataDim;
		labelCursor += labelDim;
	}	

	return error;
}