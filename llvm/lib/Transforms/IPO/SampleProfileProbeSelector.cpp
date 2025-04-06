//===----- Transforms/IPO/SampleProfileProbeSelector.h ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/SampleProfileProbeSelector.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/InstrTypes.h"

namespace llvm {

ProbeEdge::ProbeEdge(BasicBlock* Src, BasicBlock* Dest, bool Critical):
    SrcBB(Src), DestBB(Dest), inSpanningTree(Critical), isCritical(Critical), Weight(UINT64_MAX) {}

ProbeEdge::ProbeEdge(BasicBlock* Src, BasicBlock* Dest):
    SrcBB(Src), DestBB(Dest), inSpanningTree(false), isCritical(false), Weight(UINT64_MAX) {}

bool ProbeEdge::operator<(const ProbeEdge& other) const {
  if (SrcBB<other.SrcBB)
    return true;
  else if (SrcBB==other.SrcBB)
    return DestBB<other.DestBB;
  else
    return false;
}

void ProbeCFGSpanningTree::findAllEdges() {
  for (auto &BB : *F) {
    EC.insert(&BB);
    for (auto Succ : successors(&BB)) {
      if (!BB.getSingleSuccessor() && !Succ->getSinglePredecessor()) {
        AllEdges.insert(std::make_unique<ProbeEdge>(&BB, Succ, true));
        if (EC.isEquivalent(&BB, Succ)) {
          isComplex = true;
        } else {
          EC.unionSets(&BB, Succ);
        }
      } else {
        AllEdges.insert(std::make_unique<ProbeEdge>(&BB, Succ, false));
      }
    }
  }
}

std::set<ProbeEdge> ProbeCFGSpanningTree::getAllNSTEdges() {
  std::set<ProbeEdge> NST;
  for (auto& E: AllEdges) {
    if (!E->inSpanningTree) {
      NST.insert(*E);
    }
  }
  return NST;
}

bool ProbeCFGSpanningTree::markSTEdges() {
  if (isComplex) {
    return false;
  }
  for (auto& E: AllEdges) {
    auto SrcBB = E->SrcBB, DestBB = E->DestBB;
    if (!EC.isEquivalent(SrcBB, DestBB)) {
      EC.unionSets(SrcBB, DestBB);
      E->inSpanningTree = true;
    }
  }
  // check all BBs are in spanning tree
  return EC.getNumClasses() == 1;
}

ProbeCFGSpanningTree::ProbeCFGSpanningTree(Function* Func): F(Func), isComplex(false) {
  findAllEdges();
}

ProbeCFGRecover::BBInfo::BBInfo(BasicBlock* BB): BB(BB), InEdgesCount(0) {}

void ProbeCFGRecover::BBInfo::insertEdge(ProbeEdge* E) {
  assert(E->SrcBB == BB || E->DestBB == BB);
  if (E->SrcBB == BB)
    Edges.push_back(E);
  else {
    Edges.push_front(E);
    InEdgesCount++;
  }
}

unsigned ProbeCFGRecover::BBInfo::getDegree() const {
  unsigned Degree = 0;
  for (auto E: Edges) {
    if (E->Weight == UINT64_MAX)
      Degree++;
  }
  return Degree;
}

ProbeEdge *ProbeCFGRecover::BBInfo::evaluateUniqueEdgeWeight() {
  ProbeEdge *NoWeightEdge = nullptr;
  uint64_t InWeight = 0, OutWeight = 0;
  for (auto E : Edges) {
    if (E->Weight == UINT64_MAX) {
      assert(NoWeightEdge == nullptr);
      NoWeightEdge = E;
    } else {
      if (E->SrcBB == BB)
        OutWeight += E->Weight;
      else
        InWeight += E->Weight;
    }
  }
  if (NoWeightEdge->SrcBB == BB)
    NoWeightEdge->Weight = OutWeight < InWeight ? InWeight - OutWeight : 0;
  else
    NoWeightEdge->Weight = OutWeight > InWeight ? OutWeight - InWeight : 0;
  return NoWeightEdge;
}

uint64_t ProbeCFGRecover::BBInfo::evaluateWeight() {
  uint64_t InWeight = 0, OutWeight = 0;
  unsigned Steps = 0;
  for (auto E: Edges) {
    assert(E->Weight!=UINT64_MAX);
    if (Steps++<InEdgesCount)
      InWeight+=E->Weight;
    else
      OutWeight+=E->Weight;
  }
  return (InWeight+OutWeight)/2;
}

bool ProbeCFGRecover::BBInfo::operator<(const BBInfo& other) const {
  unsigned Degree1 = getDegree();
  unsigned Degree2 = other.getDegree();
  if (Degree1<Degree2)
    return true;
  else if (Degree1>Degree2)
    return false;
  else
    return this < &other;
}

void ProbeCFGRecover::insertEdge(ProbeEdge* Edge) {
  BB2Info.at(Edge->SrcBB).insertEdge(Edge);
  BB2Info.at(Edge->DestBB).insertEdge(Edge);
}

void ProbeCFGRecover::reassignBB(const BasicBlock* BB) {
  auto Info = std::move(BB2Info.at(BB));
  BB2Info.insert_or_assign(BB, Info);
}

void ProbeCFGRecover::findAllEdges() {
  auto& Entry = F->getEntryBlock();
  for (auto& BB: *F) {
    for (auto Succ: successors(&BB)) {
      ProbeEdge* Edge = new ProbeEdge{&BB, Succ};
      AllEdges.insert(Edge);
      insertEdge(Edge);
    }
    // for Exit -> Entry virtual edge
    if (succ_size(&BB) == 0) {
      ProbeEdge* Edge = new ProbeEdge{&BB, &Entry};
      AllEdges.insert(Edge);
      insertEdge(Edge);
    }
  }
}

void ProbeCFGRecover::markEdgesWeight(const std::map<const BasicBlock*, uint64_t>& BlockWeights) {
  for (auto& I: BlockWeights) {
    auto BB = I.first;
    uint64_t Weight = I.second;
    auto& Info = BB2Info.at(BB);
    auto& Edges = Info.Edges;
    if (Info.InEdgesCount == 1) {
      Edges.front()->Weight = Weight;
      reassignBB(BB);
    }
    if (Edges.size() - Info.InEdgesCount == 1) {
      Edges.back()->Weight = Weight;
      reassignBB(BB);
    }
  }
}

bool ProbeCFGRecover::propagateWeights(std::map<const BasicBlock*, uint64_t>& BlockWeights) {
  for (auto It = BB2Info.begin(); It != BB2Info.end(); It = BB2Info.begin()) {
    auto BB = It->first;
    auto& Info = It->second;
    if (Info.getDegree() == 0) {
      BlockWeights.insert_or_assign(BB, Info.evaluateWeight());
      BB2Info.erase(It);
    } else if (Info.getDegree() == 1) {
      ProbeEdge* EvaluatedEdge = Info.evaluateUniqueEdgeWeight();
      BlockWeights.insert_or_assign(BB, Info.evaluateWeight()); // update map
      BB2Info.erase(It);
      if (EvaluatedEdge->SrcBB == BB)
        reassignBB(EvaluatedEdge->DestBB);
      else
        reassignBB(EvaluatedEdge->SrcBB);
    } else {
      return false;
    }
  }
  return true;
}

ProbeCFGRecover::ProbeCFGRecover(Function* Func): F(Func) {
  for (auto& BB: *Func) {
    BB2Info.insert({&BB, &BB});
  }
  findAllEdges();
}

ProbeCFGRecover::~ProbeCFGRecover() {
  for (ProbeEdge* E: AllEdges) {
    delete E;
  }
}

ProbeSelectorBase::ProbeSelectorBase(Function* Func): F(Func) {}
ProbeSelectorSpanningTree::ProbeSelectorSpanningTree(Function* Func): ProbeSelectorBase(Func) {}

void ProbeSelectorSpanningTree::getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) {
  ProbeCFGSpanningTree SpanningTree(F);
  assert(SpanningTree.markSTEdges()); // TODO: fix interface
  for (auto& E: SpanningTree.getAllNSTEdges()) {
    if (E.SrcBB->getSingleSuccessor()) {
      InstrumentBBs.insert(E.SrcBB);
    } else if (E.DestBB->getSinglePredecessor()) {
      InstrumentBBs.insert(E.DestBB);
    } else {
      // for critical edge, we probe both BBs, for that we cannot split the
      // edge to insert a pseudo probe.
      errs() << "cannot make selective probe when function has critical edge which is not in spanning tree";
      exit(1);
    }
  }
  // we assume the 0 BB connecting to exit BB is in SpanningTree, so 0 BB connecting to entry BB is not
  InstrumentBBs.insert(&F->getEntryBlock());
}

void ProbeSelectorSpanningTree::resolveBBWeights(
    DenseMap<const BasicBlock *, uint64_t> &BlockWeights) {
  if (BlockWeights.size() == F->size())
    return;
  ProbeCFGRecover Recover(F);
  std::map<const BasicBlock *, uint64_t> NewMap;
  for (auto &It : BlockWeights) {
    NewMap.insert(std::make_pair(It.getFirst(), It.getSecond()));
  }
  Recover.markEdgesWeight(NewMap);
  assert(Recover.propagateWeights(NewMap)); // TODO: fix interface
  for (auto &It : NewMap) {
    BlockWeights.insert_or_assign(It.first, It.second);
  }
}

ProbeSelectorCallSite::ProbeSelectorCallSite(Function *Func):
  ProbeSelectorBase(Func) {}

void ProbeSelectorCallSite::getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) {
  for (auto& BB: *F) {
    for (auto& Inst: BB) {
      if (!dyn_cast<CallBase>(&Inst)) {
        InstrumentBBs.insert(&BB);
      }
    }
  }
}

void ProbeSelectorCallSite::resolveBBWeights(
    DenseMap<const BasicBlock *, uint64_t> &BlockWeights) {}

ProbeSelectorEquivalentBBs::ProbeSelectorEquivalentBBs(Function *Func):
  ProbeSelectorBase(Func) {
  DominatorTree ForwardDomTree;
  PostDominatorTree PostDomTree;
  for (auto& BB1: *F) {
    EC.insert(&BB1);
    SmallVector<BasicBlock*> Descendants;
    ForwardDomTree.getDescendants(&BB1, Descendants);
    for (auto BB2: Descendants) {
      if (PostDomTree.dominates(BB2, &BB1)) {
        EC.unionSets(&BB1, BB2);
      }
    }
  }
}

void ProbeSelectorEquivalentBBs::getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) {
  for (auto& I: EC) {
    if (!I.isLeader())
      continue;
    InstrumentBBs.insert(I.getData());
  }
}

void ProbeSelectorEquivalentBBs::resolveBBWeights(
    DenseMap<const BasicBlock *, uint64_t> &BlockWeights) {
  for (auto& I: EC) {
    if (!I.isLeader())
      continue;
    uint64_t MaxWeightInEC = 0;
    for (auto Member = EC.member_begin(I); Member != EC.member_end(); Member++) {
      auto It = BlockWeights.find(*Member);
      if (It != BlockWeights.end()) {
        MaxWeightInEC = std::max(MaxWeightInEC, It->getSecond());
      }
    }
    for (auto Member = EC.member_begin(I); Member != EC.member_end(); Member++) {
        BlockWeights.insert_or_assign(*Member, MaxWeightInEC);
    }
  }
}

};