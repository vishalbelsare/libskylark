#ifndef SKYLARK_DENSE_TRANSFORM_ELEMENTAL_HPP
#define SKYLARK_DENSE_TRANSFORM_ELEMENTAL_HPP

/** Triggering High Performance (HP) functionality
    for Elemental-based dense transforms (commented #define's):
    -- use the first AND any of the rest #define's for selective combinations of
       input-output matrix combinations.
    -- use ALL #define's to enfoce HP functionality wherever available.

#define HP_DENSE_TRANSFORM_ELEMENTAL
#define HP_DENSE_TRANSFORM_ELEMENTAL_MC_MR
#define HP_DENSE_TRANSFORM_ELEMENTAL_MC_MR_LOCAL
#define HP_DENSE_TRANSFORM_ELEMENTAL_COLDIST_STAR
#define HP_DENSE_TRANSFORM_ELEMENTAL_COLDIST_STAR_LOCAL
#define HP_DENSE_TRANSFORM_ELEMENTAL_STAR_ROWDIST
#define HP_DENSE_TRANSFORM_ELEMENTAL_STAR_ROWDIST_LOCAL

*/

// testing the full stack
// #define HP_DENSE_TRANSFORM_ELEMENTAL
// #define HP_DENSE_TRANSFORM_ELEMENTAL_MC_MR

#include "dense_transform_Elemental_local.hpp"
#include "dense_transform_Elemental_coldist_star.hpp"
#include "dense_transform_Elemental_coldist_star_local.hpp"
#include "dense_transform_Elemental_coldist_star_localall.hpp"
#include "dense_transform_Elemental_star_rowdist.hpp"
#include "dense_transform_Elemental_star_rowdist_local.hpp"
#include "dense_transform_Elemental_mc_mr.hpp"
#include "dense_transform_Elemental_mc_mr_local.hpp"

#endif // SKYLARK_DENSE_TRANSFORM_ELEMENTAL_HPP
