/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4C_TEST_OPTIMIZATIONS_H
#define VC4C_TEST_OPTIMIZATIONS_H

#include "cpptest.h"

#include "TestEmulator.h"

#include <set>
#include <string>

class TestOptimizations : public TestEmulator
{
public:
    TestOptimizations();

    void testEmptyIterator(std::string passParamName);
    void testEmptyKernel(std::string passParamName);
    void testHelloWorld(std::string passParamName);
    void testHelloWorldVector(std::string passParamName);
    void testBarrier(std::string passParamName);
    void testBranches(std::string passParamName);
    void testWorkItem(std::string passParamName);
    
    void testFibonacci(std::string passParamName);
    void testStruct(std::string passParamName);
    void testCopy(std::string passParamName);
    void testAtomics(std::string passParamName);
    void testF2I(std::string passParamName);
    void testGlobalData(std::string passParamName);
    void testSelect(std::string passParamName);
    void testDot3Local(std::string passParamName);
    void testVectorAdd(std::string passParamName);
    void testArithmetic(std::string passParamName);
    void testClamp(std::string passParamName);
    void testCross(std::string passParamName);
};

#endif /* VC4C_TEST_OPTIMIZATIONS_H */