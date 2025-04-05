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

#include "llvm/IR/CFG.h"
#include <map>
#include <set>
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

class ProbeCFGST {
public:
  class BBInfoList {
    struct BBInfo {
      BasicBlock* BB;
      BBInfo* Parent;

      BBInfo(BasicBlock* BB);
    };

    std::map<BasicBlock*, BBInfo> BB2Info;

    BBInfo* getRoot(BBInfo* BBI);
    BBInfo* getBBInfo(BasicBlock* BB);

  public:
    BBInfoList(Function* Func);

    // union two group, assuming they are not unioned.
    void unionGroup(BasicBlock* BB1, BasicBlock* BB2);

    bool hasUnioned(BasicBlock* BB1, BasicBlock* BB2);
    bool checkAllUnioned();
  };

private:
  Function* F;

  std::set<std::unique_ptr<ProbeEdge>> AllEdges;
  BBInfoList BBIL;

  void findAllEdges();

public:
  // the function has edges that can form a circle
  bool isComplex;

  std::set<ProbeEdge> getAllNSTEdges();

  // return true if success, otherwise return false
  bool markSTEdges();

  explicit ProbeCFGST(Function* Func);
};

class ProbeCFGRecover {
  struct BBInfo {
    BasicBlock* BB;
    std::list<ProbeEdge*> Edges; // InEdges at front, OutEdges at back
    size_t InEdgesCount;
    uint64_t Weight;

    BBInfo(BasicBlock* BB);

    void insertEdge(ProbeEdge* E);
    unsigned getDegree() const;

    // assume there is unique edge of this block whose weight is unknown,
    // evaluate it and assign estimated block weight.
    ProbeEdge* evaluateUniqueEdgeWeight();

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
  ProbeSelectorBase(Function* Func): F(Func) {}
  virtual void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) = 0;
  virtual void resolveBBWeights(DenseMap<const BasicBlock*, uint64_t>& BlockWeights) = 0;
protected:
  Function* F;
};

class ProbeSelectorST: ProbeSelectorBase {
public:
  explicit ProbeSelectorST(Function* Func): ProbeSelectorBase(Func) {}
  void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) override;
  void resolveBBWeights(DenseMap<const BasicBlock*, uint64_t>& BlockWeights) override;
};

};

#endif //SAMPLEPROFILEPROBESELECTOR_H
