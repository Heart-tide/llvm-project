//===- Transforms/IPO/SampleProfileProbeSelector.h ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file provides the interface for selective probe.
//
//===----------------------------------------------------------------------===//

#ifndef SAMPLEPROFILEPROBESELECTOR_H
#define SAMPLEPROFILEPROBESELECTOR_H

#include "llvm/Transforms/Instrumentation/CFGMST.h"

namespace llvm {

template<class Edge, class BBInfo>
class ProbeCFGMST: CFGMST<Edge, BBInfo> {

};

class ProbeSelectorBase {
public:
  ProbeSelectorBase(Function* Func): F(Func) {}
  virtual void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) = 0;
  virtual void resolveBBWeights() = 0;
protected:
  Function* F;
};

class ProbeSelectorMST: ProbeSelectorBase {
public:
  ProbeSelectorMST(Function* Func): ProbeSelectorBase(Func) {}
  void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) override {}
  void resolveBBWeights() override {}
};

};

#endif //SAMPLEPROFILEPROBESELECTOR_H
