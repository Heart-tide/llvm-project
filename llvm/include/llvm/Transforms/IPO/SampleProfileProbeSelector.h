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

    bool inSpanningTree;
    bool isCritical;

    uint64_t Weight;

    Edge(BasicBlock* Src, BasicBlock* Dest, bool Critical, uint64_t Weight=-1):
      SrcBB(Src), DestBB(Dest), inSpanningTree(Critical), isCritical(Critical), Weight(Weight) {}

    bool operator<(const Edge& other) const {
      if (SrcBB<other.SrcBB)
        return true;
      else if (SrcBB==other.SrcBB)
        return DestBB<other.DestBB;
      else
        return false;
    }
  };

  class BBInfoList {
    struct BBInfo {
      BasicBlock* BB;
      BBInfo* Parent;

      BBInfo(BasicBlock* BB): BB(BB), Parent(nullptr) {}
    };

    std::map<BasicBlock*, BBInfo> BB2Info;

    BBInfo* getRoot(BBInfo* BBI) {
      auto P = BBI;
      while (P->Parent!=nullptr)
        P=P->Parent;
      return P;
    }

    BBInfo* getBBInfo(BasicBlock* BB) {
      auto It = BB2Info.find(BB);
      return &It->second;
    }

  public:
    BBInfoList(Function* Func) {
      for (auto& BB:*Func) {
        BB2Info.insert(std::make_pair(&BB, BBInfo(&BB)));
      }
    }

    // union two group, assuming they are not unioned.
    void unionGroup(BasicBlock* BB1, BasicBlock* BB2) {
      auto BBI1=getBBInfo(BB1), BBI2 = getBBInfo(BB2);
      if (BBI1->Parent==nullptr) {
        BBI1->Parent=BBI2;
      } else if (BBI2->Parent==nullptr){
        BBI2->Parent=BBI1;
      } else {
        auto BBI1Root = getRoot(BBI1);
        BBI1Root->Parent = BBI2;
      }
    }

    bool hasUnioned(BasicBlock* BB1, BasicBlock* BB2) {
      auto BBI1=getBBInfo(BB1), BBI2 = getBBInfo(BB2);
      auto BBI1Root = getRoot(BBI1);
      auto BBI2Root = getRoot(BBI2);
      return BBI1Root == BBI2Root;
    }

    bool checkAllUnioned() {
      int NoParentBBICount = 0;
      for (auto& I: BB2Info) {
        auto& BBI = I.second;
        if (BBI.Parent == nullptr) {
          if (++NoParentBBICount == 2) {
            return false;
          }
        }
      }
      return true;
    }
  };

private:
  Function* F;

  std::set<BasicBlock*> visited;

  std::set<std::unique_ptr<Edge>> AllEdges;
  BBInfoList BBIL;

  void findAllEdges() {
    for (auto& BB: *F) {
      for (auto Succ: successors(&BB)) {
        if (!BB.getSingleSuccessor() && !Succ->getSinglePredecessor()) {
          AllEdges.insert(std::make_unique<Edge>(&BB, Succ, true));
          if (BBIL.hasUnioned(&BB, Succ)) {
            isComplex=true;
            return;
          } else {
            BBIL.unionGroup(&BB, Succ);
          }
        }else {
          AllEdges.insert(std::make_unique<Edge>(&BB, Succ, false));
        }
      }
    }
  }

public:
  // the function has edges that can form a circle
  bool isComplex;

  std::set<Edge> getAllNSTEdges() {
    std::set<Edge> NST;
    for (auto& E: AllEdges) {
      if (!E->inSpanningTree) {
        NST.insert(*E);
      }
    }
    return NST;
  }

  // return true if success, otherwise return false
  bool markSTEdges() {
    if(isComplex) {
      return false;
    }
    for (auto& E: AllEdges) {
      auto SrcBB = E->SrcBB, DestBB = E->DestBB;
      if (!BBIL.hasUnioned(SrcBB, DestBB)) {
        BBIL.unionGroup(SrcBB, DestBB);
        E->inSpanningTree = true;
      }
    }
    return BBIL.checkAllUnioned();
  }

  explicit ProbeCFGST(Function* Func): F(Func), BBIL(Func), isComplex(false) {
    findAllEdges();
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
    ProbeCFGST SpanningTree(F);
    assert(SpanningTree.markSTEdges()); // TODO: fix interface
    for (ProbeCFGST::Edge E: SpanningTree.getAllNSTEdges()) {
      if (E.SrcBB->getSingleSuccessor()) {
        InstrumentBBs.insert(E.SrcBB);
      } else if (E.DestBB->getSinglePredecessor()) {
        InstrumentBBs.insert(E.DestBB);
      } else {
        // for critical edge, we probe both BBs, for that we cannot split the
        // edge to insert a pseudo probe.
        errs() << "cannot make selective probe when function has critical edge which is not in ST";
        exit(1);
      }
    }
    // we assume the 0 BB connecting to exit BB is in SpanningTree, so 0 BB connecting to entry BB is not
    InstrumentBBs.insert(&F->getEntryBlock());
  }

  void resolveBBWeights() override {
    // TODO
  }
};

};

#endif //SAMPLEPROFILEPROBESELECTOR_H
