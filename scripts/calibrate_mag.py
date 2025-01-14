#!/usr/bin/env python3

from __future__ import print_function
import sys
import math

verbose = True

def average(list):
  return sum(list)/len(list)

def percentile_to_value(list, *percentiles):
  list = sorted(list)
  return [list[int(p / 100.0 * (len(list)-1))] for p in percentiles]

def variance(list):
  m = average(list)
  return sum([(i-m)**2 for i in list]) / float(len(list))

def std_deviation(list):
  return math.sqrt(variance(list))

class Vector(object):
  def __init__(self, x, y, z):
    self.x, self.y, self.z = x, y, z
  def __str__(self):
    return "(" + str(self.x) + ", " + str(self.y) + ", " + str(self.z) + ")"

  def __getitem__(self, key):
    if key == 0:
      return self.x
    elif key == 1:
      return self.y
    elif key == 2:
      return self.z
    else:
      raise Exception("Invalid key")

  def magnitude(self):
    return math.sqrt(self.x**2 + self.y**2 + self.z**2)

Axes = range(3)

class Calibration:
  def __init__(self, values, raw_readings=None):
    self.values = values
    self.raw_readings = raw_readings

  def scale(self, raw_reading):
    return Vector((raw_reading[0] - self.values[0])/float(self.values[1] - self.values[0]) * 2 - 1,
      (raw_reading[1] - self.values[2])/float(self.values[3] - self.values[2]) * 2 - 1,
      (raw_reading[2] - self.values[4])/float(self.values[5] - self.values[4]) * 2 - 1)

  def __str__(self):
    return "%d %d %d %d %d %d" % tuple(self.values)

  def info_string(self):
    return "%-32s  avg=%7.4f stdev=%7.4f" % (
      str(self),
      average(self.scaled_magnitudes()),
      std_deviation(self.scaled_magnitudes())
    )

  def scaled_magnitudes(self):
    return [s.magnitude() for s in self.scaled_readings()]

  def scaled_readings(self):
    return [self.scale(r) for r in self.raw_readings]


def run(file=sys.stdin):
  try:
    raw_readings = read_vectors(file)

    if len(raw_readings) < 300:
      print("Warning: Only " + str(len(raw_readings)) + " readings were provided.",
            file=sys.stderr)

    cal = simple_minmax_calibration(raw_readings)

    if verbose:
      print("calibration info: " + cal.info_string(), file=sys.stderr)

    print(cal)

  except KeyboardInterrupt:
    print("", file=sys.stderr)   # newline to tidy things up
    pass

def read_vectors(file):
  vectors = [Vector(*[int(s) for s in line.split()[0:3]]) for line in file]
  return vectors

def simple_minmax_calibration(readings):
  guess = []
  for axis in Axes:
    values = [v[axis] for v in readings]
    guess.extend([min(values), max(values)])
  return Calibration(guess, readings)


if __name__=='__main__':
  run()