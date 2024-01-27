
#include <fstream>
#include <memory>
#include <functional>
#ifdef USE_MPI
#include <boost/mpi.hpp>
#endif
#include <boost/lexical_cast.hpp>
#include <Eigen/Dense>
#include "geners/BinaryFileArchive.hh"
#include "libstoch/tree/Tree.h"
#include "libstoch/core/grids/FullGrid.h"
#include "libstoch/dp/FinalStepDPCut.h"
#include "libstoch/dp/TransitionStepTreeDPCut.h"
#include "libstoch/dp/OptimizerDPCutTreeBase.h"

using namespace std;
using namespace Eigen;


double  DynamicProgrammingByTreeCut(const shared_ptr<libstoch::FullGrid> &p_grid,
                                    const shared_ptr<libstoch::OptimizerDPCutTreeBase > &p_optimize,
                                    const function< ArrayXd(const int &, const ArrayXd &, const ArrayXd &)>  &p_funcFinalValue,
                                    const ArrayXd &p_pointStock,
                                    const int &p_initialRegime,
                                    const string   &p_fileToDump
#ifdef USE_MPI
                                    , const boost::mpi::communicator &p_world
#endif
                                   )
{
    // from the optimizer get back the simulator
    shared_ptr< libstoch::SimulatorDPBaseTree> simulator = p_optimize->getSimulator();
    // final cut values
    vector< shared_ptr< ArrayXXd > >  valueCutsNext = libstoch::FinalStepDPCut(p_grid, p_optimize->getNbRegime())(p_funcFinalValue, simulator->getNodes());
    shared_ptr<gs::BinaryFileArchive> ar = make_shared<gs::BinaryFileArchive>(p_fileToDump.c_str(), "w");
    // name for object in archive
    string nameAr = "ContinuationTree";
    // iterate on time steps
    for (int iStep = 0; iStep < simulator->getNbStep(); ++iStep)
    {
        simulator->stepBackward();
        // probabilities
        std::vector<double>  proba = simulator->getProba();
        // get connection between nodes
        std::vector< std::vector<std::array<int, 2>  > >  connected = simulator->getConnected();
        // conditional expectation operator
        shared_ptr<libstoch::Tree> tree = std::make_shared<libstoch::Tree>(proba, connected);

        // transition object
        libstoch::TransitionStepTreeDPCut  transStep(p_grid, p_grid, p_optimize
#ifdef USE_MPI
                , p_world
#endif
                                                 );
        vector< shared_ptr< ArrayXXd > > valueCuts = transStep.oneStep(valueCutsNext, tree);

        // dump continuation values
        transStep.dumpContinuationCutsValues(ar, nameAr, iStep, valueCutsNext,  tree);
        valueCutsNext = valueCuts;
    }
    // interpolate at the initial stock point and initial regime
    // Only keep the first  value  of the cuts  (other components are  for derivatives with respect to stock value)
    return p_grid->createInterpolator(p_pointStock)->apply(valueCutsNext[p_initialRegime]->row(0).transpose());
}
