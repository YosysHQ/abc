/**CFile****************************************************************

  FileName    [eSLIMMan.hpp]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Using Exact Synthesis with the SAT-based Local Improvement Method (eSLIM).]

  Synopsis    [eSLIM manager.]

  Author      [Franz-Xaver Reichl]
  
  Affiliation [University of Freiburg]

  Date        [Ver. 1.0. Started - March 2025.]

  Revision    [$Id: eSLIMMan.hpp,v 1.00 2025/03/17 00:00:00 Exp $]

***********************************************************************/

#ifndef ABC__OPT__ESLIM__ESLIMMAN_h
#define ABC__OPT__ESLIM__ESLIMMAN_h

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "misc/util/abc_namespaces.h"
#include "aig/gia/gia.h"


#include "eslimCirMan.hpp"
#include "relationGeneration.hpp"
#include "selectionStrategies.hpp"
#include "subcircuit.hpp"
#include "utils.hpp"



ABC_NAMESPACE_CXX_HEADER_START

namespace eSLIM {


  template <typename SynthesisEngine, typename SelectionStrategy>
  class eSLIM_Man {
    public:
      static void applyeSLIM(eSLIMCirMan& cir, const eSLIMConfig& cfg, eSLIMLog& log);
    private:

      eSLIM_Man(eSLIMCirMan& es_man, const eSLIMConfig& cfg, eSLIMLog& log);

      void optimize();

      void findReplacement();
      eSLIMCirMan computeReplacement(Subcircuit& subcir);
      eSLIMCirMan synthesiseRelation(Subcircuit& subcir, const Relation& rel);


      eSLIMCirMan& es_man;
      SelectionStrategy selection_strategy;
      const eSLIMConfig& cfg;
      eSLIMLog& log;

      double relation_generation_time ;
      double synthesis_time ;

      bool stopeSLIM = false;


  };

}
ABC_NAMESPACE_CXX_HEADER_END

#include "eSLIMMan.tpp"

#endif