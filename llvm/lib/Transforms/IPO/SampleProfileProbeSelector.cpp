//===----- Transforms/IPO/SampleProfileProbeSelector.h ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/SampleProfileProbeSelector.h"

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

ProbeCFGST::BBInfoList::BBInfo::BBInfo(BasicBlock* BB): BB(BB), Parent(nullptr) {}

auto ProbeCFGST::BBInfoList::getRoot(BBInfo* BBI) -> BBInfo* {
  auto P = BBI;
  while (P->Parent!=nullptr)
    P=P->Parent;
  return P;
}

auto ProbeCFGST::BBInfoList::getBBInfo(BasicBlock* BB) -> BBInfo* {
  auto It = BB2Info.find(BB);
  return &It->second;
}

ProbeCFGST::BBInfoList::BBInfoList(Function* Func) {
  for (auto& BB:*Func) {
    BB2Info.insert(std::make_pair(&BB, BBInfo(&BB)));
  }
}

// union two group, assuming they are not unioned.
void ProbeCFGST::BBInfoList::unionGroup(BasicBlock* BB1, BasicBlock* BB2) {
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

bool ProbeCFGST::BBInfoList::hasUnioned(BasicBlock* BB1, BasicBlock* BB2) {
  auto BBI1=getBBInfo(BB1), BBI2 = getBBInfo(BB2);
  auto BBI1Root = getRoot(BBI1);
  auto BBI2Root = getRoot(BBI2);
  return BBI1Root == BBI2Root;
}

bool ProbeCFGST::BBInfoList::checkAllUnioned() {
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

void ProbeCFGST::findAllEdges() {
  for (auto& BB: *F) {
    for (auto Succ: successors(&BB)) {
      if (!BB.getSingleSuccessor() && !Succ->getSinglePredecessor()) {
        AllEdges.insert(std::make_unique<ProbeEdge>(&BB, Succ, true));
        if (BBIL.hasUnioned(&BB, Succ)) {
          isComplex=true;
        } else {
          BBIL.unionGroup(&BB, Succ);
        }
      } else {
        AllEdges.insert(std::make_unique<ProbeEdge>(&BB, Succ, false));
      }
    }
  }
}

std::set<ProbeEdge> ProbeCFGST::getAllNSTEdges() {
  std::set<ProbeEdge> NST;
  for (auto& E: AllEdges) {
    if (!E->inSpanningTree) {
      NST.insert(*E);
    }
  }
  return NST;
}

bool ProbeCFGST::markSTEdges() {
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

ProbeCFGST::ProbeCFGST(Function* Func): F(Func), BBIL(Func), isComplex(false) {
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
ProbeSelectorST::ProbeSelectorST(Function* Func): ProbeSelectorBase(Func) {}

void ProbeSelectorST::getProbeBBs(DenseSet<BasicBlock *> &InstrumentBBs) {
  ProbeCFGST SpanningTree(F);
  assert(SpanningTree.markSTEdges()); // TODO: fix interface
  for (auto& E: SpanningTree.getAllNSTEdges()) {
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

void ProbeSelectorST::resolveBBWeights(DenseMap<const BasicBlock*, uint64_t>& BlockWeights) {
  if (BlockWeights.size() == F->size())
    return;
  ProbeCFGRecover Recover(F);
  std::map<const BasicBlock*, uint64_t> NewMap;
  for (auto& It: BlockWeights) {
    NewMap.insert(std::make_pair(It.getFirst(), It.getSecond()));
  }
  Recover.markEdgesWeight(NewMap);
  assert(Recover.propagateWeights(NewMap)); // TODO: fix interface
  for (auto& It: NewMap) {
    BlockWeights.insert_or_assign(It.first, It.second);
  }
}

};