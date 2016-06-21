/*
* BaseTest.h
*
*  Created on: Nov 23, 2014
*      Author: felipegb94
*/

#ifndef BaseTest_H_
#define BaseTest_H_

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <stdint.h>

#include "Object.h"

class BaseTest {

public:

  /// Constructor: Every test has to have a name and an associated project to it.
  BaseTest(const std::string& testName,
           const std::string& testProjectName
           )
  : m_name(testName),
    m_projectName(testProjectName),
    m_passed(false)
  {
    m_path = "../RESULTS/";
    m_jsonTest.AddMember("name", m_name);
    m_jsonTest.AddMember("project", m_projectName);
  }

  virtual ~BaseTest() {}

  /// Main function for running the test
  void run()
  {
    m_passed = execute();
    finalizeJson();
  }

  /// Add a test-specific metric (a key-value pair)
  void addMetric(const std::string& metricName,
                 double             metricValue)
  {
    m_jsonMetrics.AddMember(metricName, metricValue);
  }

  void addMetric(const std::string& metricName,
                 int                metricValue)
  {
    m_jsonMetrics.AddMember(metricName, metricValue);
  }

  void addMetric(const std::string& metricName,
                 uint64_t           metricValue)
  {
    m_jsonMetrics.AddMember(metricName, metricValue);
  }

  void addMetric(const std::string& metricName,
                 const std::string& metricValue)
  {
    m_jsonMetrics.AddMember(metricName, metricValue);
  }

  // void addMetric(const std::string&   metricName,
  //                std::vector<double>& metricValue) {
  //   m_jsonMetrics.AddMember(metricName, metricValue);
  // }

  /// Contains Test. Returns if test passed or failed
  virtual bool execute() = 0;

  /// Return total execution time for this test.
  virtual double getExecutionTime() const = 0;

  /// Print
  void print() {
    std::cout << "Test Information: " << std::endl;
    std::cout << m_jsonTest.GetObject();
  };

private:

  std::ofstream jsonfile; // Json file to write at the end
  std::string m_path;        ///< Path to save result
  std::string m_name;        ///< Name of test
  std::string m_projectName; ///< Name of the project: e.g: chrono, chronoRender, etc.
  bool        m_passed;      ///< Did the test pass or fail?

  /// Metrics that are being tested (key-value pairs)
  Object   m_jsonMetrics;
  Object m_jsonTest;     ///< JSON output of the test

  /// This function finalizaes the json object and writes to a file
  void finalizeJson() {

    /// Add remaining results to json file.
    m_jsonTest.AddMember("passed", m_passed);
    m_jsonTest.AddMember("execution_time", getExecutionTime());
    m_jsonTest.AddMember("metrics", m_jsonMetrics);
    /// Set values to json elements
    jsonfile.open(m_path + m_name + ".json");
    jsonfile << m_jsonTest.GetObject();
    jsonfile.close();
  }

};

#endif // BaseTest_H_
