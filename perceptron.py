from __future__ import division
import numpy as np

class Perceptron:
   def __init__(self, n_features, learn_rate=1):
      self.learned_weights = np.random.randn(n_features)
      self.epoch = 0
      self.learn_rate = learn_rate

   def epoch_tick(self):
      self.epoch += 1

   def get_epoch(self):
      return self.epoch

   def reset_epoch(self):
      self.epoch = 0

   def get_activation(self, features):
      return np.dot(self.learned_weights, features)

   def classify(self, features):
      return self.get_activation(features) > 0

   def alpha(self):
      return self.learn_rate / (self.learn_rate + self.epoch)

   def give_training_data(self, features, label):
      act = self.get_activation(features)
      sgn = 1 if act > 0 else -1
      step = self.alpha() * ((1 if bool(label) else -1) - sgn)
      self.learned_weights += step * features
      return bool(act > 0) == bool(label)

