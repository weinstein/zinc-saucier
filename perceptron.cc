#include "perceptron.h"
#include <vector>
using std::vector;

Perceptron::Perceptron(int n_features)
      : learned_weights_(n_features),
        epoch_(0),
        learn_rate_(1) {
}

Perceptron::Perceptron(int n_features, int learn_rate)
      : learned_weights_(n_features),
        epoch_(0),
        learn_rate_(learn_rate) {
}

Perceptron::Perceptron(const Perceptron& other)
      : learned_weights_(other.learned_weights_),
        epoch_(other.epoch_),
        learn_rate_(other.learn_rate_) {
}

Perceptron::~Perceptron() {
}

void Perceptron::EpochTick() {
   ++epoch_;
}

int Perceptron::GetEpoch() const {
   return epoch_;
}

void Perceptron::ResetEpoch() {
   epoch_ = 0;
}

double Perceptron::GetActivation(const vector<double>& features) const {
   double activation = 0;
   for (int i = 0; i < features.size() && i < learned_weights_.size(); ++i) {
      activation += features[i] * learned_weights_[i];
   }
   return activation;
}

bool Perceptron::GiveTrainingData(const vector<double>& features, bool label) {
   bool sgn = GetActivation(features) > 0;
   double step = Alpha() * ((label ? 1 : -1) - (sgn ? 1 : -1));
   for (int i = 0; i < learned_weights_.size() && i < features.size(); ++i) {
      learned_weights_[i] += step * features[i];
   }
   return label == sgn;
}

double Perceptron::Alpha() const {
   return learn_rate_ / (learn_rate_ + epoch_);
}
