/**CFile****************************************************************

  FileName    [tabooList.hpp]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Using Exact Synthesis with the SAT-based Local Improvement Method (eSLIM).]

  Synopsis    [Taboo list for subcircuit selection.]

  Author      [Franz-Xaver Reichl]
  
  Affiliation [University of Freiburg]

  Date        [Ver. 1.0. Started - April 2026.]

  Revision    [$Id: eSLIM.cpp,v 1.00 2025/04/14 00:00:00 Exp $]

***********************************************************************/

#ifndef ABC__OPT__ESLIM__TABOOLIST_hpp
#define ABC__OPT__ESLIM__TABOOLIST_hpp

#include <set>
#include <random>
#include <algorithm>

#include "misc/util/abc_namespaces.h"

#include "eslimCirMan.hpp"
#include "subcircuit.hpp"

ABC_NAMESPACE_CXX_HEADER_START
namespace eSLIM {

  class TabooList {

    public:
      TabooList(const eSLIMCirMan& es_man);
      int getRandomNonTaboo(std::mt19937& rng, double taboo_ratio, unsigned int taboo_time);
      template<typename BiasCondition>
      int getBiasedRandomNonTaboo(std::mt19937& rng, unsigned int bias, double taboo_ratio, unsigned int taboo_time);
      void reset();

    private:
      struct eSLIMCirObjCompBase {
        bool operator()(eSLIMCirObj* a, eSLIMCirObj* b) const {
          return a->getId() < b->getId();
        }
      };

      struct eSLIMCirObjComp  {
        bool operator()(eSLIMCirObj* a, eSLIMCirObj* b) const {
          return a->isTaboo < b->isTaboo || (a->isTaboo == b->isTaboo && eSLIMCirObjCompBase()(a,b));
        }
      };

      void updateTaboo(double taboo_ratio, unsigned int taboo_time);
      void registerReplacement(eSLIMCirMan& replacement, const Subcircuit& subcir);
      void discardSubcircuit(const Subcircuit& subcir);
      void removeRedundantNode(eSLIMCirObj* nptr);
      void fill();

      void addNode(eSLIMCirObj* pobj);
      void discardNode(eSLIMCirObj* pobj);
      void addNonTaboo(eSLIMCirObj* pobj);
      void removeNonTaboo(eSLIMCirObj* pobj);


      const eSLIMCirMan& es_man;
      std::set<eSLIMCirObj*, eSLIMCirObjComp> taboo_nodes;
      std::set<eSLIMCirObj*, eSLIMCirObjCompBase> non_taboo_nodes;

    friend eSLIMCirMan;
  };

  inline TabooList::TabooList(const eSLIMCirMan& es_man)
                  : es_man(es_man) {
    fill();
  }

  inline void TabooList::fill() {
    for (int i = es_man.getNofPis() + 1; i < es_man.getNofObjs()-es_man.getNofPos(); i++) {
      auto pObj = es_man.nodes[i].get();
      addNonTaboo(pObj);
    }
  }

  inline void TabooList::reset() {
    taboo_nodes.clear();
    non_taboo_nodes.clear();
    fill();
  }

  inline void TabooList::updateTaboo(double taboo_ratio, unsigned int taboo_time) {
    assert (taboo_ratio > 0 && taboo_ratio < 1);
    auto it = taboo_nodes.begin();
    while (it != taboo_nodes.end()) {
      if (!es_man.isTaboo(*(*it), taboo_time)) {
        addNonTaboo(*it);
        it = taboo_nodes.erase(it);
      } else if (taboo_nodes.size() > static_cast<double>(es_man.getNofGates()) * taboo_ratio) {
        addNonTaboo(*it);
        it = taboo_nodes.erase(it);
      } else {
        return;
      }
    }
  }

  inline void TabooList::registerReplacement(eSLIMCirMan& replacement, const Subcircuit& subcir) {
    discardSubcircuit(subcir);
    for (int i = replacement.getNofPis() + 1; i < replacement.getNofObjs() - replacement.getNofPos(); i++) {
      auto pObj = replacement.nodes[i].get();
      taboo_nodes.insert(pObj);
    }
  }

  inline void TabooList::discardSubcircuit(const Subcircuit& subcir) {
    for (int nd : subcir.nodes) {
      discardNode(es_man.nodes[nd].get());
    }
  }

  inline void TabooList::removeRedundantNode(eSLIMCirObj* nptr) {
    discardNode(nptr);
  }


  inline int TabooList::getRandomNonTaboo(std::mt19937& rng, double taboo_ratio, unsigned int taboo_time) {
    updateTaboo(taboo_ratio, taboo_time);
    assert(non_taboo_nodes.size() > 0);
    std::uniform_int_distribution<std::mt19937::result_type> udistr(0, non_taboo_nodes.size() - 1);
    int rv = udistr(rng);
    auto it = non_taboo_nodes.begin();
    std::advance(it, rv);
    return (*it)->node_id;
  }

  template<typename BiasCondition>
  int TabooList::getBiasedRandomNonTaboo(std::mt19937& rng, unsigned int bias, double taboo_ratio, unsigned int taboo_time) {
    updateTaboo(taboo_ratio, taboo_time);
    std::vector<int> weights;
    weights.reserve(non_taboo_nodes.size());
    for (const auto& pObj : non_taboo_nodes) {
      if (BiasCondition::useBias(es_man, *pObj)) {
        weights.push_back(bias);
      } else {
        weights.push_back(1);
      }
    }
    std::discrete_distribution<std::mt19937::result_type> ddistr(weights.begin(), weights.end()); 
    int rv = ddistr(rng);
    auto it = non_taboo_nodes.begin();
    std::advance(it, rv);
    return (*it)->node_id;
  }

  inline void TabooList::addNode(eSLIMCirObj* pobj) {
    taboo_nodes.insert(pobj);
  }

  inline void TabooList::discardNode(eSLIMCirObj* pobj) {
    removeNonTaboo(pobj);
    taboo_nodes.erase(pobj);
  }

  inline void TabooList::addNonTaboo(eSLIMCirObj* pobj) {
    non_taboo_nodes.insert(pobj);
  }

  inline void TabooList::removeNonTaboo(eSLIMCirObj* pobj) {
     non_taboo_nodes.erase(pobj);
  }

}
ABC_NAMESPACE_CXX_HEADER_END
#endif