#ifndef _PERCEPTRON_H
#define _PERCEPTRON_H

#include <vector>

class Perceptron {
 public:
   Perceptron(const Perceptron& other);
   Perceptron(int n_features);
   Perceptron(int n_features, int learn_rate);
   ~Perceptron();

   void EpochTick();
   int GetEpoch() const;
   void ResetEpoch();
   double GetActivation(const std::vector<double>& features) const;
   bool GiveTrainingData(const std::vector<double>& features, bool label);

 private:
   std::vector<double> learned_weights_;
   int epoch_;
   const int learn_rate_;

   double Alpha() const;
};

#endif
