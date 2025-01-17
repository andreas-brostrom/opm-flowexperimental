#ifndef BLACK_OIL_MODEL_FV_FAST_HPP
#define BLACK_OIL_MODEL_FV_FAST_HPP
#include <ebos/FIBlackOilModel.hpp>
namespace Opm{
    template<typename TypeTag>
    class BlackOilModelFvFast: public BlackOilModel<TypeTag>{


        using Parent = BlackOilModel<TypeTag>;
        using Simulator = GetPropType<TypeTag, Properties::Simulator>;
        using IntensiveQuantities = GetPropType<TypeTag, Properties::IntensiveQuantities>;
    public:
        using Indices = GetPropType<TypeTag, Properties::Indices>;
    private:
        using GridView = GetPropType<TypeTag, Properties::GridView>;
        using Element = typename GridView::template Codim<0>::Entity;
        using ElementIterator = typename GridView::template Codim<0>::Iterator;
        using ElementMapper = Dune::MultipleCodimMultipleGeomTypeMapper<GridView>;
        static constexpr bool waterEnabled = Indices::waterEnabled;
        static constexpr bool gasEnabled = Indices::gasEnabled;
        static constexpr bool oilEnabled = Indices::oilEnabled;
        enum { numPhases = getPropValue<TypeTag, Properties::NumPhases>() };



    public:
        BlackOilModelFvFast(Simulator& simulator): BlackOilModel<TypeTag>(simulator){
        }

        void invalidateAndUpdateIntensiveQuantities(unsigned timeIdx){
            OPM_TIMEBLOCK(updateIntensiveQuantities);
//            std::cout << "----------------------Update quantities-------------------\n"
//                      << std::flush;
//            Parent::invalidateAndUpdateIntensiveQuantities(timeIdx);
//             Parent::invalidateAndUpdateIntensiveQuantitiesSimple(*this,solution,/*timeIdx*/0);
            const auto& primaryVars = this->solution(timeIdx);
            const auto& problem = this->simulator_.problem();
            size_t numGridDof = primaryVars.size();

#ifdef _OPENMP
#pragma omp parallel for
#endif
            for (unsigned dofIdx = 0; dofIdx < numGridDof; ++dofIdx) {
                const auto& primaryVar = primaryVars[dofIdx];
                auto& intquant = this->intensiveQuantityCache_[timeIdx][dofIdx];
                intquant.update(problem, primaryVar, dofIdx, timeIdx);
                //intquant.update(problem,priVars, globalSpaceIdx, timeIdx, waterpvt, gaspvt, oilpvt);
            }

            std::fill(this->intensiveQuantityCacheUpToDate_[timeIdx].begin(),
                      this->intensiveQuantityCacheUpToDate_[timeIdx].end(),
                      /*value=*/true);

        }

        void invalidateAndUpdateIntensiveQuantitiesOverlap(unsigned timeIdx) const
        {
            // loop over all elements
            ThreadedEntityIterator<GridView, /*codim=*/0> threadedElemIt(this->gridView_);
            const auto& primaryVars = this->solution(timeIdx);
            const auto& problem = this->simulator_.problem();
            const auto& mapper = this->simulator_.model().dofMapper();
#ifdef _OPENMP
#pragma omp parallel
#endif
        {
            //ElementContext elemCtx(this->simulator_);
            auto elemIt = threadedElemIt.beginParallel();
            for (; !threadedElemIt.isFinished(elemIt); elemIt = threadedElemIt.increment()) {
                if (elemIt->partitionType() != Dune::OverlapEntity) {
                    continue;
                }
                const Element& elem = *elemIt;
                const auto& dofIdx = mapper.index(elem);
                const auto& primaryVar = primaryVars[dofIdx];
                auto& intquant = this->intensiveQuantityCache_[timeIdx][dofIdx];
                intquant.update(problem, primaryVar, dofIdx, timeIdx);
                this->intensiveQuantityCacheUpToDate_[timeIdx][dofIdx]=true;
            }
        }
        }

        template <class GridSubDomain>
        void invalidateAndUpdateIntensiveQuantities(unsigned timeIdx, const GridSubDomain& gridSubDomain) const{
            OPM_TIMEBLOCK_LOCAL(updateIntensiveQuantitiesSubdomain);
//            std::cout << "----------------------Update quantitiesSubdomain-------------------\n";
//                      << std::flush;
//            Parent::invalidateAndUpdateIntensiveQuantities(timeIdx);
//Parent::invalidateAndUpdateIntensiveQuantitiesSimple(*this,solution,/*timeIdx*/0);
            const auto& primaryVars = this->solution(timeIdx);
            const auto& problem = this->simulator_.problem();
#ifdef _OPENMP
#pragma omp parallel for
#endif
            for (auto dofIdx : gridSubDomain.cells) {
                //unsigned dofIdx = cell;
                const auto& primaryVar = primaryVars[dofIdx];
                auto& intquant = this->intensiveQuantityCache_[timeIdx][dofIdx];
                intquant.update(problem, primaryVar, dofIdx, timeIdx);
                this->intensiveQuantityCacheUpToDate_[timeIdx][dofIdx]=true;
            }
        }
    template <class GridSubDomain>
    void invalidateAndUpdateIntensiveQuantitiesGrid(unsigned timeIdx, const GridSubDomain& gridSubDomain) const
    {
        using ElementContext = GetPropType<TypeTag, Properties::ElementContext>;
        using ThreadManager = GetPropType<TypeTag, Properties::ThreadManager>;
        //using GridView = GetPropType<TypeTag, Properties::GridView>;
        using Element = typename GridView::template Codim<0>::Entity;
        //using ElementIterator = typename GridView::template Codim<0>::Iterator;
        // loop over all elements...
        using GridViewType = decltype(gridSubDomain.view);
        const auto& primaryVars = this->solution(timeIdx);
        const auto& problem = this->simulator_.problem();
        const auto& mapper = this->simulator_.model().dofMapper();
        ThreadedEntityIterator<GridViewType, /*codim=*/0> threadedElemIt(gridSubDomain.view);
#ifdef _OPENMP
#pragma omp parallel
#endif
        {

            ElementContext elemCtx(this->simulator_);
            auto elemIt = threadedElemIt.beginParallel();
            for (; !threadedElemIt.isFinished(elemIt); elemIt = threadedElemIt.increment()) {
                if (elemIt->partitionType() != Dune::InteriorEntity) {
                    continue;
                }
                const Element& elem = *elemIt;
                const auto& dofIdx = mapper.index(elem);
                const auto& primaryVar = primaryVars[dofIdx];
                auto& intquant = this->intensiveQuantityCache_[timeIdx][dofIdx];
                intquant.update(problem, primaryVar, dofIdx, timeIdx);
                this->intensiveQuantityCacheUpToDate_[timeIdx][dofIdx]=true;

                // elemCtx.updatePrimaryStencil(elem);
                // // Mark cache for this element as invalid.
                // const std::size_t numPrimaryDof = elemCtx.numPrimaryDof(timeIdx);
                // for (unsigned dofIdx = 0; dofIdx < numPrimaryDof; ++dofIdx) {
                //     const unsigned globalIndex = elemCtx.globalSpaceIndex(dofIdx, timeIdx);
                //     this->setIntensiveQuantitiesCacheEntryValidity(globalIndex, timeIdx, false);
                // }
                // // Update for this element.
                // elemCtx.updatePrimaryIntensiveQuantities(/*timeIdx=*/0);
            }
        }
    }

        const IntensiveQuantities& intensiveQuantities(unsigned globalIdx, unsigned timeIdx) const{
            OPM_TIMEBLOCK_LOCAL(intensiveQuantities);
            //const auto& primaryVars = this->solution(timeIdx);
            //const auto& problem = this->simulator_.problem();
            const auto intquant = this->cachedIntensiveQuantities(globalIdx, timeIdx);
            if (!this->enableIntensiveQuantityCache_){
                OPM_THROW(std::logic_error, "Run without intentive quantites not enabled: Use --enable-intensive-quantity=true");
            }
            if(!intquant){
                OPM_THROW(std::logic_error, "Intensive quantites need to be updated in code");
            }
            return *intquant;
        }

    };
}
#endif
