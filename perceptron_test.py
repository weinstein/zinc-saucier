#!/usr/bin/python

from __future__ import division
import perceptron
import numpy as np

def random_feature_vec(dims=2):
   return np.append(np.random.random_integers(0, 1, dims), 1)

n_features = 11000
coeff = np.random.randn(n_features + 1)
def actual_label(vec):
   return np.dot(vec, coeff) > 0

n_samples = 32
samples = [random_feature_vec(n_features) for i in range(0, n_samples)]
labels = [actual_label(vec) for vec in samples]
inds = range(0, n_samples)

max_epochs = n_samples
tol = 0.01
clf = perceptron.Perceptron(n_features + 1, max_epochs/100)

for i in range(0, max_epochs):
   print 'epoch', i
   correct = 0
   np.random.shuffle(inds)
   for idx in inds:
      if clf.give_training_data(samples[idx], labels[idx]):
         correct += 1
   clf.epoch_tick()
   print '  accuracy', correct / n_samples
   if 1 - correct / n_samples < tol:
      break
normal_learned = clf.learned_weights / np.linalg.norm(clf.learned_weights)
print 'learned weights', normal_learned
normal_coeff = coeff / np.linalg.norm(coeff)
print 'actual coeffs', normal_coeff

n_test = 50000
correct = 0
for i in range(0, n_test):
   data = random_feature_vec(n_features)
   if clf.classify(data) == actual_label(data):
      correct += 1
print 'random test data accuracy', correct / n_test
