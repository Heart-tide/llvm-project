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

class ProbeCFGST {
public:
  struct Edge {
    BasicBlock* SrcBB;
    BasicBlock* DestBB;

    Edge(BasicBlock* Src, BasicBlock* Dest): SrcBB(Src), DestBB(Dest) {}

    bool operator<(const Edge& other) const {
      if (SrcBB<other.SrcBB)
        return true;
      else if (SrcBB==other.SrcBB)
        return DestBB<other.DestBB;
      else
        return false;
    }
  };

private:
  std::set<BasicBlock*> visited;

  std::set<Edge> AllEdges;
  std::set<Edge> STEdges;

  Function* F;

  void findSTEdgesDFS(BasicBlock* Cur) {
    visited.insert(Cur);
    for (auto Succ: successors(Cur)) {
      if (visited.find(Succ) == visited.end()) {
        Edge E(Cur, Succ);
        STEdges.insert(E);
        findSTEdgesDFS(Succ);
      }
    }
  }

  void findAllEdges() {
    AllEdges.clear();
    for (auto& It: *F) {
      auto BB = &It;
      for (auto Succ: successors(BB)) {
        Edge E(BB, Succ);
        AllEdges.insert(E);
      }
    }
  }

  // Apply DFS from Entry BB
  void findSTEdges() {
    visited.clear();
    STEdges.clear();
    auto& Entry = F->getEntryBlock();
    findSTEdgesDFS(&Entry);
  }

public:
  std::set<Edge> getAllNSTEdges() {
    return move(set_difference(AllEdges, STEdges));
  }

  explicit ProbeCFGST(Function* Func): F(Func) {
    findAllEdges();
    findSTEdges();
  }
};

class ProbeSelectorBase {
public:
  ProbeSelectorBase(Function* Func): F(Func) {}
  virtual void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) = 0;
  virtual void resolveBBWeights() = 0;
protected:
  Function* F;
};

class ProbeSelectorST: ProbeSelectorBase {
public:
  explicit ProbeSelectorST(Function* Func): ProbeSelectorBase(Func) {}

  void getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) override {
    ProbeCFGST ST(F);
    for (ProbeCFGST::Edge E: ST.getAllNSTEdges()) {
      if (E.SrcBB->getSingleSuccessor()) {
        InstrumentBBs.insert(E.SrcBB);
      } else if (E.DestBB->getSinglePredecessor()) {
        InstrumentBBs.insert(E.DestBB);
      } else {
        // for critical edge, we probe both BBs, for that we cannot split the
        // edge to insert a pseudo probe.
        InstrumentBBs.insert(E.SrcBB);
        InstrumentBBs.insert(E.DestBB);
      }
    }
    // we assume the 0 BB connecting to exit BB is in ST, so 0 BB connecting to entry BB is not
    InstrumentBBs.insert(&F->getEntryBlock());
  }

  void resolveBBWeights() override {
    // TODO
  }
};

};

#endif //SAMPLEPROFILEPROBESELECTOR_H
