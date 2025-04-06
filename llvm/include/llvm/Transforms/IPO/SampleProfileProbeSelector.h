//===- SampleProfileProbeSelector.cpp ----------*- C++ -*-===//
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

#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/ADT/PriorityQueue.h"
#include "llvm/IR/CFG.h"
#include <map>
#include <list>

namespace llvm {

struct ProbeEdge {
  BasicBlock* SrcBB;
  BasicBlock* DestBB;
  bool inSpanningTree;
  bool isCritical;
  uint64_t Weight;

  ProbeEdge(BasicBlock* Src, BasicBlock* Dest, bool Critical);
  ProbeEdge(BasicBlock* Src, BasicBlock* Dest);
  bool operator<(const ProbeEdge& other) const;
};

class ProbeCFGSpanningTree {
private:
  Function* F;
  std::set<std::unique_ptr<ProbeEdge>> AllEdges;
  EquivalenceClasses<BasicBlock*> EC;
  void findAllEdges();

public:
  bool isComplex; // the function has edges that can form a circle
  std::set<ProbeEdge> getAllNSTEdges();
  bool markSTEdges();  // return true if success, otherwise return false
  explicit ProbeCFGSpanningTree(Function* Func);
};

class ProbeCFGRecover {
  struct BBInfo {
    BasicBlock* BB;
    std::list<ProbeEdge*> Edges; // InEdges at front, OutEdges at back
    size_t InEdgesCount;

    BBInfo(BasicBlock* BB);

    void insertEdge(ProbeEdge* E);
    unsigned getDegree() const;

    // assume there is unique edge of this block whose weight is unknown,
    // evaluate it and assign estimated block weight.
    ProbeEdge* evaluateUniqueEdgeWeight();
    uint64_t evaluateWeight();

    bool operator<(const BBInfo& other) const;
  };

  Function* F;

  std::set<ProbeEdge*> AllEdges;
  std::map<const BasicBlock*, BBInfo> BB2Info;

  void insertEdge(ProbeEdge* Edge);
  void reassignBB(const BasicBlock* BB);
  void findAllEdges();

public:
  void markEdgesWeight(const std::map<const BasicBlock*, uint64_t>& BlockWeights);

  // propagate weights and store block weights to BlockWeights.
  // if we can get weight of all blocks, return true, otherwise return false.
  bool propagateWeights(std::map<const BasicBlock*, uint64_t>& BlockWeights);

  ProbeCFGRecover(Function* Func);
  ~ProbeCFGRecover();
};

class ProbeSelectorBase {
public:
  explicit ProbeSelectorBase(Function* Func);
  virtual void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) = 0;
  virtual void resolveBBWeights(DenseMap<const BasicBlock*, uint64_t>& BlockWeights) = 0;
protected:
  Function* F;
};

class ProbeSelectorSpanningTree: ProbeSelectorBase {
public:
  explicit ProbeSelectorSpanningTree(Function* Func);
  void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) override;
  void resolveBBWeights(DenseMap<const BasicBlock*, uint64_t>& BlockWeights) override;
};

class ProbeSelectorCallSite: ProbeSelectorBase {
public:
  explicit ProbeSelectorCallSite(Function* Func);
  void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) override;
  void resolveBBWeights(DenseMap<const BasicBlock*, uint64_t>& BlockWeights) override;
};

class ProbeSelectorEquivalentBBs : ProbeSelectorBase {
private:
  EquivalenceClasses<BasicBlock*> EC;
public:
  explicit ProbeSelectorEquivalentBBs(Function *Func);
  void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) override;
  void resolveBBWeights(DenseMap<const BasicBlock *, uint64_t> &BlockWeights) override;
};

};

#endif //SAMPLEPROFILEPROBESELECTOR_H
